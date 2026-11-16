#include "nv_thread_local_routing.hpp"
#include "ptxpass/core.hpp"
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <cstring>

namespace bpftime
{
namespace attach
{
namespace
{
const char *AREA_SYM = "__bpftime_tl";
const char *AREA_REG = "%bpftime_tl";
const char *AREA_IN = "%bpftime_tl_in";

[[noreturn]] void fail(const std::string &what)
{
	throw std::runtime_error("thread-local routing: " + what);
}

// index just past the '}' matching the '{' at open
size_t match_brace(const std::string &s, size_t open)
{
	int depth = 0;
	for (size_t i = open; i < s.size(); i++) {
		if (s[i] == '{')
			depth++;
		else if (s[i] == '}') {
			if (--depth == 0)
				return i + 1;
		}
	}
	fail("unbalanced braces");
}

struct func_span {
	std::string name;
	size_t header;	   // ".visible .func" or ".func"
	size_t params_open; // '('
	size_t params_close; // ')'
	size_t body_open;    // '{'
	size_t body_end;     // past '}'
};

bool is_probe_function_name(const std::string &n)
{
	return n.rfind("__probe_func__", 0) == 0 ||
	       n.rfind("__retprobe_func__", 0) == 0 ||
	       n.rfind("__memcapture__", 0) == 0 || n == "bpf_main";
}

// every probe function definition (not declarations)
std::vector<func_span> find_probe_functions(const std::string &ptx)
{
	std::vector<func_span> out;
	size_t pos = 0;
	while ((pos = ptx.find(".func", pos)) != std::string::npos) {
		size_t p = pos + 5;
		while (p < ptx.size() && isspace((unsigned char)ptx[p]))
			p++;
		size_t n0 = p;
		while (p < ptx.size() &&
		       (isalnum((unsigned char)ptx[p]) || ptx[p] == '_' ||
			ptx[p] == '$'))
			p++;
		std::string name = ptx.substr(n0, p - n0);
		size_t here = pos;
		pos = p;
		if (!is_probe_function_name(name))
			continue;
		size_t po = ptx.find_first_not_of(" \t\r\n", p);
		if (po == std::string::npos || ptx[po] != '(')
			continue;
		size_t pc = ptx.find(')', po);
		if (pc == std::string::npos)
			fail("unterminated parameter list of " + name);
		size_t next = ptx.find_first_not_of(" \t\r\n", pc + 1);
		if (next == std::string::npos || ptx[next] != '{')
			continue; // a declaration
		func_span f;
		f.name = name;
		size_t vis = ptx.rfind(".visible", here);
		f.header = (vis != std::string::npos &&
			    ptx.find_first_not_of(" \t", vis + 8) == here) ?
				   vis :
				   here;
		f.params_open = po;
		f.params_close = pc;
		f.body_open = next;
		f.body_end = match_brace(ptx, next);
		out.push_back(f);
		pos = f.body_end;
	}
	return out;
}

std::string trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	size_t b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

bool parse_int(const std::string &s, long long *v)
{
	if (s.empty())
		return false;
	char *end = nullptr;
	long long x = strtoll(s.c_str(), &end, 10);
	if (*end != '\0')
		return false;
	*v = x;
	return true;
}

// the constant a callseq passes as its map argument, if it is one
bool constant_fd(const std::string &fbody, const std::string &block,
		 long long *fd)
{
	static const std::regex p0(
		R"(st\.param\.b64\s+\[param0\+0\],\s*([^;]+);)");
	std::smatch m;
	if (!std::regex_search(block, m, p0))
		return false;
	std::string arg = trim(m[1]);
	if (parse_int(arg, fd))
		return true;
	if (arg.empty() || arg[0] != '%')
		return false;
	// a register set exactly once, to a constant, in the function
	std::regex def(R"(mov\.[ub]64\s+)" + std::regex_replace(
						   arg, std::regex(R"([$%.])"),
						   R"(\$&)") +
		       R"(,\s*(-?\d+)\s*;)");
	std::regex any_def(R"((?:mov|ld|add|sub|cvt|selp|mul|and|or|shl|shr)\S*\s+)" +
			   std::regex_replace(arg, std::regex(R"([$%.])"),
					      R"(\$&)") +
			   R"(\s*,)");
	auto b = std::sregex_iterator(fbody.begin(), fbody.end(), def);
	auto e = std::sregex_iterator();
	int defs = 0;
	long long val = 0;
	for (auto it = b; it != e; ++it) {
		defs++;
		val = std::stoll((*it)[1]);
	}
	int all = std::distance(std::sregex_iterator(fbody.begin(),
						     fbody.end(), any_def),
				e);
	if (defs != 1 || all != 1)
		return false;
	*fd = val;
	return true;
}

// A generic atom/red whose address lies in the local window is undefined
// (PTX ISA 9.7.15.5: scalar atom only on .global/.shared, or generic
// addresses pointing there); a thread-local map's value is such an address.
// Every generic atom/red of a probe function gets an isspacep.local test
// (PTX ISA 9.7.10.22): local addresses take a plain load, the operation and
// a store (exact: the memory is the thread's own), others keep the atomic.
std::string guard_generic_atomics(const std::string &fbody,
				  const std::string &fname, int *count)
{
	static const std::regex atom_line(
		R"(^(\s*)(@!?%[\w$]+\s+)?(atom|red)((?:\.[a-zA-Z0-9_:]+)+)\s+([^;]*);(.*)$)");
	std::istringstream in(fbody);
	std::string line, out;
	int n = 0;
	bool first = true;
	while (std::getline(in, line)) {
		if (!first)
			out += "\n";
		first = false;
		std::smatch m;
		if (!std::regex_match(line, m, atom_line)) {
			out += line;
			continue;
		}
		std::string mods = m[4];
		// explicit state spaces are not generic
		if (mods.find(".global") != std::string::npos ||
		    mods.find(".shared") != std::string::npos) {
			out += line;
			continue;
		}
		if (m[2].length())
			fail("predicated generic atomic in " + fname);
		const bool is_red = m[3] == "red";
		std::vector<std::string> parts;
		{
			std::string ops = m[5];
			std::string cur;
			int depth = 0;
			for (char c : ops) {
				if (c == '[')
					depth++;
				if (c == ']')
					depth--;
				if (c == ',' && depth == 0) {
					parts.push_back(trim(cur));
					cur.clear();
				} else
					cur += c;
			}
			parts.push_back(trim(cur));
		}
		// op and width from the modifiers
		std::string op, ty;
		for (const char *o :
		     { ".add.", ".and.", ".or.", ".xor.", ".exch.", ".cas." })
			if ((mods + ".").find(o) != std::string::npos)
				op = std::string(o).substr(1,
							 strlen(o) - 2);
		for (const char *t : { ".u64", ".s64", ".b64", ".u32",
				       ".s32", ".b32" })
			if (mods.size() >= strlen(t) &&
			    mods.compare(mods.size() - strlen(t),
					 strlen(t), t) == 0)
				ty = t + 1;
		if (op.empty() || ty.empty())
			fail("unsupported generic atomic '" + line + "' in " +
			     fname);
		const std::string bits = ty.substr(1);
		size_t ai = is_red ? 0 : 1;
		const size_t want = (is_red ? 2 : 3) + (op == "cas" ? 1 : 0);
		if (parts.size() != want || parts[ai].size() < 3 ||
		    parts[ai].front() != '[' || parts[ai].back() != ']')
			fail("unexpected operands of '" + line + "' in " +
			     fname);
		std::string addr = trim(parts[ai].substr(1, parts[ai].size() - 2));
		std::string base = addr, off = "0";
		size_t plus = addr.find('+');
		if (plus != std::string::npos) {
			base = trim(addr.substr(0, plus));
			off = trim(addr.substr(plus + 1));
		}
		const std::string id = std::to_string(n++);
		const std::string ws = m[1];
		std::ostringstream o;
		o << ws << "{ // bpftime: atomic on a possibly local address\n"
		  << ws << ".reg .pred %bpat_l" << id << ", %bpat_q" << id
		  << ";\n"
		  << ws << ".reg .b64 %bpat_a" << id << ";\n"
		  << ws << ".reg .b" << bits << " %bpat_o" << id
		  << ", %bpat_n" << id << ";\n"
		  << ws << "add.u64 %bpat_a" << id << ", " << base << ", "
		  << off << ";\n"
		  << ws << "isspacep.local %bpat_l" << id << ", %bpat_a" << id
		  << ";\n"
		  << ws << "@!%bpat_l" << id << " " << m[3].str() << mods << " "
		  << m[5].str() << ";\n"
		  << ws << "@%bpat_l" << id << " ld.b" << bits << " %bpat_o"
		  << id << ", [%bpat_a" << id << "];\n";
		const std::string b = parts[ai + 1];
		if (op == "add")
			o << ws << "@%bpat_l" << id << " add.u" << bits
			  << " %bpat_n" << id << ", %bpat_o" << id << ", " << b
			  << ";\n";
		else if (op == "and" || op == "or" || op == "xor")
			o << ws << "@%bpat_l" << id << " " << op << ".b"
			  << bits << " %bpat_n" << id << ", %bpat_o" << id
			  << ", " << b << ";\n";
		else if (op == "exch")
			o << ws << "@%bpat_l" << id << " mov.b" << bits
			  << " %bpat_n" << id << ", " << b << ";\n";
		else { // cas: store c when the old value equals b
			o << ws << "@%bpat_l" << id << " setp.eq.b" << bits
			  << " %bpat_q" << id << ", %bpat_o" << id << ", " << b
			  << ";\n"
			  << ws << "@%bpat_l" << id << " selp.b" << bits
			  << " %bpat_n" << id << ", " << parts[ai + 2]
			  << ", %bpat_o" << id << ", %bpat_q" << id << ";\n";
		}
		o << ws << "@%bpat_l" << id << " st.b" << bits << " [%bpat_a"
		  << id << "], %bpat_n" << id << ";\n";
		if (!is_red)
			o << ws << "@%bpat_l" << id << " mov.b" << bits << " "
			  << parts[0] << ", %bpat_o" << id << ";\n";
		o << ws << "}" << m[6].str();
		out += o.str();
	}
	if (!fbody.empty() && fbody.back() == '\n')
		out += "\n";
	if (count)
		*count += n;
	return out;
}

std::string route_function_body(
	const std::string &fbody, bool routed,
	const std::map<long long, thread_local_map_desc> &maps,
	thread_local_routing_stats &st, const std::string &fname)
{
	static const std::regex p1(
		R"(st\.param\.b64\s+\[param1\+0\],\s*([^;]+);)");
	static const std::regex p4(
		R"(st\.param\.b64\s+\[param4\+0\],\s*([^;]+);)");
	static const std::regex ret(
		R"(ld\.param\.b64\s+(%[\w$]+),\s*\[retval0\+0\];)");
	static const std::regex helper(R"(_bpf_helper_ext_(\d{4})\b)");
	std::string out;
	size_t pos = 0;
	int direct_seq = 0;
	for (;;) {
		size_t b = fbody.find("{ // callseq ", pos);
		if (b == std::string::npos)
			break;
		size_t close = fbody.find("} // callseq ", b);
		if (close == std::string::npos)
			fail("unterminated callseq in " + fname);
		size_t e = fbody.find('\n', close);
		e = (e == std::string::npos) ? fbody.size() : e + 1;
		out.append(fbody, pos, b - pos);
		std::string block = fbody.substr(b, e - b);
		pos = e;
		std::smatch hm;
		if (!std::regex_search(block, hm, helper)) {
			out += block;
			continue;
		}
		int id = std::stoi(hm[1]);
		if (id < 1 || id > 3) {
			out += block;
			continue;
		}
		long long fd = -1;
		bool is_const = constant_fd(fbody, block, &fd);
		auto tl = is_const ? maps.find(fd) : maps.end();
		if (routed && id == 1 && tl != maps.end()) {
			std::smatch km, rm;
			if (!std::regex_search(block, km, p1) ||
			    !std::regex_search(block, rm, ret))
				fail("unexpected lookup shape in " + fname);
			const auto &d = tl->second;
			std::ostringstream o;
			int n = direct_seq++;
			o << "{ // bpftime thread-local lookup, map " << fd
			  << "\n\t.reg .pred \t%bptl_p" << n
			  << ";\n\t.reg .b32 \t%bptl_k" << n
			  << ";\n\t.reg .b64 \t%bptl_a" << n << ";\n"
			  << "\tld.u32 \t%bptl_k" << n << ", ["
			  << trim(km[1]) << "];\n"
			  << "\tsetp.lt.u32 \t%bptl_p" << n << ", %bptl_k"
			  << n << ", " << d.max_entries << ";\n"
			  << "\tmul.wide.u32 \t%bptl_a" << n << ", %bptl_k"
			  << n << ", " << d.value_size << ";\n"
			  << "\tadd.u64 \t%bptl_a" << n << ", %bptl_a" << n
			  << ", " << AREA_IN << ";\n"
			  << "\tadd.u64 \t%bptl_a" << n << ", %bptl_a" << n
			  << ", " << d.offset << ";\n"
			  << "\tselp.b64 \t" << rm[1].str() << ", %bptl_a"
			  << n << ", 0, %bptl_p" << n << ";\n"
			  << "} // bpftime thread-local lookup\n";
			out += o.str();
			st.direct_lookups++;
			continue;
		}
		std::smatch am;
		if (!std::regex_search(block, am, p4))
			fail("helper call without a fifth argument in " +
			     fname);
		std::string arg = routed ? AREA_IN : "0";
		std::string nb = block.substr(0, am.position(0)) +
				 "st.param.b64 \t[param4+0], " + arg + ";" +
				 block.substr(am.position(0) + am.length(0));
		std::string rest = nb.substr(am.position(0) + 1);
		std::smatch again;
		if (std::regex_search(rest, again, p4))
			fail("two fifth arguments in one call in " + fname);
		out += nb;
		if (routed)
			st.helper_calls_given_area++;
		else
			st.helper_calls_given_zero++;
	}
	out.append(fbody, pos, std::string::npos);
	return guard_generic_atomics(out, fname, &st.guarded_atomics);
}
} // namespace

std::string route_thread_local_maps(const std::string &ptx_in,
				    const std::vector<std::string> &kernels_in,
				    const std::vector<thread_local_map_desc> &descs,
				    uint64_t area_bytes,
				    thread_local_routing_stats *stats_out)
{
	thread_local_routing_stats st;
	if (area_bytes > THREAD_LOCAL_AREA_MAX)
		fail("thread-local maps need " + std::to_string(area_bytes) +
		     " bytes per thread, more than " +
		     std::to_string(THREAD_LOCAL_AREA_MAX));
	area_bytes = std::max<uint64_t>(8, (area_bytes + 7) & ~7ull);
	std::map<long long, thread_local_map_desc> maps;
	for (auto &d : descs)
		maps[d.fd] = d;
	std::string ptx = ptx_in;
	std::set<std::string> kernels(kernels_in.begin(), kernels_in.end());

	// 1. kernels: the area, its zeroing, the address at every probe call
	std::set<std::string> routed_functions;
	for (const auto &k : kernels) {
		auto [kb, ke] = ptxpass::find_kernel_body(ptx, k);
		if (kb == std::string::npos)
			continue;
		std::string text = ptx.substr(kb, ke - kb);
		size_t open = text.find('{');
		if (open == std::string::npos)
			fail("kernel without a body: " + k);
		const std::string names[] = { "__probe_func__" + k,
					      "__retprobe_func__" + k };
		std::istringstream in(text.substr(open + 1));
		std::string line, body;
		int sites = 0;
		std::set<std::string> called;
		while (std::getline(in, line)) {
			bool done = false;
			for (const auto &n : names) {
				size_t at = line.find(n);
				if (at == std::string::npos)
					continue;
				size_t after = at + n.size();
				size_t semi =
					line.find_first_not_of(" \t", after);
				if (semi == std::string::npos ||
				    line[semi] != ';')
					continue; // a longer name
				size_t c = line.rfind("call", at);
				if (c == std::string::npos)
					fail("probe name outside a call in " +
					     k);
				// what precedes "call": spaces, a label, a
				// predicate; the area's address is formed
				// after the label and before the predicate
				std::string head = line.substr(0, c);
				size_t pred = head.find('@');
				std::string before = pred == std::string::npos ?
							     head :
							     head.substr(0, pred);
				std::string predicate =
					pred == std::string::npos ?
						"" :
						head.substr(pred);
				std::string indent = "\t";
				body += before + "cvta.local.u64 \t" +
					AREA_REG + ", " + AREA_SYM + ";\n" +
					indent + predicate +
					line.substr(c, after - c) + ", (" +
					AREA_REG + ")" + line.substr(semi) +
					"\n";
				sites++;
				called.insert(n);
				done = true;
				break;
			}
			if (!done)
				body += line + "\n";
		}
		if (sites == 0)
			continue;
		std::ostringstream decl;
		decl << "\n\t.local .align 16 .b8 \t" << AREA_SYM << "["
		     << area_bytes << "];\n\t.reg .b64 \t" << AREA_REG
		     << ";\n\tcvta.local.u64 \t" << AREA_REG << ", "
		     << AREA_SYM << ";\n";
		for (uint64_t off = 0; off < area_bytes; off += 8)
			decl << "\tst.u64 \t[" << AREA_REG << "+" << off
			     << "], 0;\n";
		std::string new_text = text.substr(0, open + 1) + decl.str() +
				       body;
		// getline dropped the text's last newline state: keep the
		// closing brace as it was
		if (!text.empty() && text.back() != '\n' &&
		    !new_text.empty() && new_text.back() == '\n')
			new_text.pop_back();
		ptx = ptx.substr(0, kb) + new_text + ptx.substr(ke);
		st.kernels++;
		st.call_sites += sites;
		for (auto &n : called)
			routed_functions.insert(n);
	}

	// every call of a routed function must have been rewritten
	for (const auto &f : routed_functions) {
		size_t pos = 0;
		while ((pos = ptx.find(f, pos)) != std::string::npos) {
			size_t after = pos + f.size();
			size_t nx = ptx.find_first_not_of(" \t", after);
			bool is_call_without_arg =
				nx != std::string::npos && ptx[nx] == ';' &&
				ptx.rfind("call", pos) != std::string::npos &&
				pos - ptx.rfind("call", pos) < 12;
			if (is_call_without_arg)
				fail("a call of " + f +
				     " outside its kernel was not routed");
			pos = after;
		}
	}

	// 2. probe functions: the parameter, and their helper calls
	auto funcs = find_probe_functions(ptx);
	// back to front, so earlier offsets stay valid
	for (auto it = funcs.rbegin(); it != funcs.rend(); ++it) {
		const auto &f = *it;
		bool routed = routed_functions.count(f.name) > 0;
		std::string fbody = ptx.substr(f.body_open + 1,
					       f.body_end - 1 - f.body_open - 1);
		std::string nbody =
			route_function_body(fbody, routed, maps, st, f.name);
		std::string params = trim(ptx.substr(
			f.params_open + 1, f.params_close - f.params_open - 1));
		std::string head;
		if (routed) {
			if (!params.empty())
				fail(f.name + " already has parameters");
			head = "(\n\t.param .b64 " + f.name + "_bpftime_tl\n)";
			nbody = "\n\t.reg .b64 \t" + std::string(AREA_IN) +
				";\n\tld.param.b64 \t" + AREA_IN + ", [" +
				f.name + "_bpftime_tl];" + nbody;
			st.routed_functions++;
		} else {
			head = ptx.substr(f.params_open,
					  f.params_close + 1 - f.params_open);
			st.unrouted_functions++;
		}
		std::string between = ptx.substr(
			f.params_close + 1, f.body_open - f.params_close - 1);
		ptx = ptx.substr(0, f.params_open) + head + between + "{" +
		      nbody + "}" + ptx.substr(f.body_end);
	}
	for (const auto &f : routed_functions) {
		bool found = false;
		for (auto &g : funcs)
			found = found || g.name == f;
		if (!found)
			fail("routed function " + f + " has no definition");
	}
	if (stats_out)
		*stats_out = st;
	return ptx;
}
} // namespace attach
} // namespace bpftime
