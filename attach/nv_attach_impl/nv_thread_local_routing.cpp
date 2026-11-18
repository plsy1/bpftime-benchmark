#include "nv_thread_local_routing.hpp"
#include "ptxpass/core.hpp"
#include <map>
#include <unordered_map>
#include <algorithm>
#include <tuple>
#include <regex>
#include <set>
#include <sstream>
#include <cstring>
#include <cctype>

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

// PTX text as statements. Comments become a space (string literals are
// kept whole, so a "//" inside one is not a comment), the text is split on
// ';' outside string literals, and each statement's leading text is taken
// off: whitespace, braces, labels (identifiers may start with %, $ or a
// letter, PTX ISA 4.4) and a guard predicate (@p, @!p, any identifier).
// What the leading text held is kept as flags. Directives without a ';'
// (.loc, .file) stay glued to the statement after them.
struct piece {
	std::string text; // the statement without its leading text
	bool label = false;
	bool brace = false;
	bool guarded = false;
};

std::string strip_comments(const std::string &t)
{
	std::string out;
	out.reserve(t.size());
	for (size_t i = 0; i < t.size();) {
		const char c = t[i];
		if (c == '"') {
			size_t j = i + 1;
			while (j < t.size() && t[j] != '"')
				j += (t[j] == '\\' && j + 1 < t.size()) ? 2 : 1;
			j = std::min(j + 1, t.size());
			out.append(t, i, j - i);
			i = j;
		} else if (c == '/' && i + 1 < t.size() && t[i + 1] == '/') {
			size_t j = t.find('\n', i);
			out += ' ';
			i = j == std::string::npos ? t.size() : j;
		} else if (c == '/' && i + 1 < t.size() && t[i + 1] == '*') {
			size_t j = t.find("*/", i + 2);
			out += ' ';
			i = j == std::string::npos ? t.size() : j + 2;
		} else {
			out += c;
			i++;
		}
	}
	return out;
}

std::vector<piece> pieces(const std::string &body)
{
	static const std::regex lead(
		R"(^(?:\s|[{}]|[%$A-Za-z_][\w$]*\s*:(?!:)|@\s*!?\s*[%$A-Za-z_][\w$]*)+)");
	static const std::regex lab(R"([%$A-Za-z_][\w$]*\s*:(?!:))");
	const std::string t = strip_comments(body);
	std::vector<piece> out;
	size_t start = 0;
	bool in_str = false;
	for (size_t i = 0; i <= t.size(); i++) {
		if (i < t.size() && t[i] == '"')
			in_str = !in_str;
		if (i < t.size() && (t[i] != ';' || in_str))
			continue;
		std::string s = t.substr(start, i - start);
		piece p;
		std::smatch m;
		if (std::regex_search(s, m, lead)) {
			const std::string l = m.str(0);
			p.brace = l.find_first_of("{}") != std::string::npos;
			p.guarded = l.find('@') != std::string::npos;
			// a label is an identifier before ':' that is not a guard
			std::string nl = std::regex_replace(
				l, std::regex(R"(@\s*!?\s*[%$A-Za-z_][\w$]*)"), " ");
			p.label = std::regex_search(nl, lab);
			s = s.substr(m.length(0));
		}
		p.text = trim(s);
		out.push_back(p);
		start = i + 1;
	}
	return out;
}

std::vector<std::string> statements(const std::string &body)
{
	std::vector<std::string> out;
	for (auto &p : pieces(body))
		out.push_back(p.text);
	return out;
}

// an instruction's mnemonic (the leading run of letters, digits, '_', '.',
// ':') and its operand text
void split_op(const std::string &st, std::string *op, std::string *rest)
{
	size_t k = 0;
	while (k < st.size() &&
	       (std::isalnum((unsigned char)st[k]) || st[k] == '_' ||
		st[k] == '.' || st[k] == ':'))
		k++;
	*op = st.substr(0, k);
	*rest = st.substr(k);
}

// Definitions of every register in a body, found in one pass (the same
// rule as before, fail closed): a statement counts for each register in its
// destination position, everything before the first operand comma outside
// [], {} and () (a plain register, a {..} vector, a p|q pair, call's (..)
// return list), except instructions that write no register (stores,
// reductions, branches); a directive that is not a one-line declaration (a
// .loc glued to an instruction) counts for every register it mentions; a
// one-line declaration names registers, it does not write them (a
// same-named register of a nested scope is written by instructions, which
// count by name). Also the .reg declarations, for register widths.
struct body_index {
	std::unordered_map<std::string, int> defs;
	std::unordered_map<std::string, std::string> last_def;
	// name or range prefix, is a range, range count, width
	std::vector<std::tuple<std::string, bool, long long, int>> regs;
};

void registers_in(const std::string &t, std::vector<std::string> *out)
{
	for (size_t i = 0; i < t.size(); i++) {
		if (t[i] != '%')
			continue;
		if (i > 0 && (std::isalnum((unsigned char)t[i - 1]) ||
			      t[i - 1] == '_' || t[i - 1] == '$' ||
			      t[i - 1] == '%'))
			continue;
		size_t j = i + 1;
		while (j < t.size() && (std::isalnum((unsigned char)t[j]) ||
					t[j] == '_' || t[j] == '$'))
			j++;
		if (j > i + 1)
			out->push_back(t.substr(i, j - i));
		i = j - 1;
	}
}

body_index build_index(const std::string &body)
{
	static const std::regex decl(
		R"(^\.(?:reg|local|shared|param|const|global|pragma)\s[^\n]*$)");
	static const std::regex regdecl(
		R"(^\.reg\s+\.([bus])(\d+)\s+([\s\S]*)$)");
	static const std::regex range(R"(^(%[\w$]*?)<(\d+)>$)");
	body_index ix;
	for (const auto &p : pieces(body)) {
		const std::string &st = p.text;
		if (st.empty())
			continue;
		std::vector<std::string> regs;
		if (st[0] == '.') {
			std::smatch m;
			if (std::regex_match(st, m, regdecl)) {
				const int bits = std::stoi(m[2]);
				std::stringstream names(m[3].str());
				std::string nm;
				while (std::getline(names, nm, ',')) {
					nm = trim(nm);
					std::smatch r;
					if (std::regex_match(nm, r, range) &&
					    r[2].length() < 10)
						ix.regs.emplace_back(
							r[1], true,
							std::stoll(r[2]), bits);
					else
						ix.regs.emplace_back(nm, false, 0,
								     bits);
				}
			}
			if (!std::regex_match(st, decl))
				registers_in(st, &regs);
		} else {
			std::string op, rest;
			split_op(st, &op, &rest);
			if (op.rfind("st", 0) == 0 || op.rfind("red", 0) == 0 ||
			    op.rfind("bra", 0) == 0 || op == "ret" ||
			    op == "exit" || op == "trap")
				continue;
			int depth = 0;
			size_t end = rest.size();
			for (size_t i = 0; i < rest.size(); i++) {
				char c = rest[i];
				if (c == '[' || c == '{' || c == '(')
					depth++;
				else if (c == ']' || c == '}' || c == ')')
					depth--;
				else if (c == ',' && depth == 0) {
					end = i;
					break;
				}
			}
			registers_in(rest.substr(0, end), &regs);
		}
		std::sort(regs.begin(), regs.end());
		regs.erase(std::unique(regs.begin(), regs.end()), regs.end());
		for (auto &r : regs) {
			ix.defs[r]++;
			ix.last_def[r] = st;
		}
	}
	return ix;
}

// one index per body text, kept per thread (the pass runs on a pool)
const body_index &index_of(const std::string &body)
{
	thread_local std::unordered_map<std::string, body_index> cache;
	auto it = cache.find(body);
	if (it != cache.end())
		return it->second;
	if (cache.size() > 64)
		cache.clear();
	return cache.emplace(body, build_index(body)).first->second;
}

int count_defs(const std::string &body, const std::string &reg,
	       std::string *def = nullptr)
{
	const auto &ix = index_of(body);
	auto it = ix.defs.find(reg);
	if (it == ix.defs.end())
		return 0;
	if (def)
		*def = ix.last_def.at(reg);
	return it->second;
}

// the width (32 or 64) the body's .reg declarations give reg, 0 when not
// exactly one integer width is found
int reg_width(const std::string &body, const std::string &reg)
{
	int w = 0;
	for (const auto &[nm, is_range, count, bits] : index_of(body).regs) {
		bool hit = !is_range && nm == reg;
		if (is_range && reg.size() > nm.size() &&
		    reg.compare(0, nm.size(), nm) == 0) {
			const std::string idx = reg.substr(nm.size());
			hit = idx.find_first_not_of("0123456789") ==
				      std::string::npos &&
			      idx.size() < 10 && std::stoll(idx) < count;
		}
		if (!hit)
			continue;
		if (w != 0 && w != bits)
			return 0;
		w = bits;
	}
	return (w == 32 || w == 64) ? w : 0;
}

// The stack slot a register addresses: base %SP (the frame's generic
// address) or %SPL (its local address) plus a constant, through registers
// each written by one statement, an add of an integer or a mov (llvmbpf's
// frame addressing). The caller checks that %SP is cvta.local of %SPL.
bool stack_slot(const std::string &body, const std::string &reg,
		std::string *base, long long *off, int depth = 0)
{
	if (reg == "%SP" || reg == "%SPL") {
		*base = reg;
		*off = 0;
		return true;
	}
	if (depth > 8 || reg.empty() || reg[0] != '%')
		return false;
	std::string def;
	if (count_defs(body, reg, &def) != 1)
		return false;
	static const std::regex addr(
		R"(^(add\.[us]64|mov\.[ub]64)\s+(%[\w$]+)\s*,\s*(%[\w$]+)\s*(?:,\s*(-?(?:0|[1-9]\d{0,17})))?\s*$)");
	std::smatch m;
	if (!std::regex_match(def, m, addr) || m[2] != reg)
		return false;
	const bool is_add = m[1].str()[0] == 'a';
	if (is_add != m[4].matched)
		return false;
	if (!stack_slot(body, m[3], base, off, depth + 1))
		return false;
	if (is_add && __builtin_add_overflow(*off, std::stoll(m[4]), off))
		return false;
	return true;
}

// llvmbpf's frame: %SPL the depot, %SP its generic address (cvta.local), so
// [%SP + o] and [%SPL + o] are the same bytes (PTX ISA 6.4.1.1, cvta)
bool has_frame(const std::string &body)
{
	std::string a, b;
	static const std::regex spl(R"(^mov\.u64\s+%SPL\s*,\s*__local_depot\d+$)");
	static const std::regex sp(R"(^cvta\.local\.u64\s+%SP\s*,\s*%SPL$)");
	return count_defs(body, "%SPL", &a) == 1 &&
	       std::regex_match(a, spl) && count_defs(body, "%SP", &b) == 1 &&
	       std::regex_match(b, sp);
}

// The value a map lookup's key holds, taken from the store that put it in
// its stack slot: the last store to the slot before the call, in the same
// basic block and scope, unguarded, with only instructions in between that
// cannot write memory (an allowlist: arithmetic, moves, conversions,
// compares, selects, loads, branches) and none that writes the stored
// register. Fail closed: a label, a brace, a directive, any other
// instruction (a store elsewhere, st.param, wmma, a call, an atomic), a
// literal that is not plain decimal, an offset that overflows, a store whose
// state space does not match its base (st.local through the depot's local
// address, st through its generic one), a key pointer not on the generic
// frame, or a stored register whose .reg width is not found each give up,
// and the lookup loads the key as before.
std::string forwarded_key(const std::string &fbody, size_t call_at,
			  const std::string &kp, int *width)
{
	static const std::set<std::string> no_memory_write = {
		"mov",	"add",	"sub",	"mul",	"mad",	"mul24", "mad24",
		"sad",	"div",	"rem",	"abs",	"neg",	"min",	 "max",
		"shl",	"shr",	"and",	"or",	"xor",	"not",	 "cnot",
		"lop3", "shf",	"prmt", "bfe",	"bfi",	"brev",	 "popc",
		"clz",	"cvt",	"cvta", "setp", "selp", "set",	 "slct",
		"ld",	"bra",
	};
	std::string kb;
	long long ko = 0;
	if (!has_frame(fbody) || !stack_slot(fbody, kp, &kb, &ko) ||
	    kb != "%SP")
		return "";
	const auto ps = pieces(fbody.substr(0, call_at));
	static const std::regex store(
		R"(^st(\.local)?\.[ubs](8|16|32|64)\s+\[\s*(%[\w$]+)\s*(?:\+\s*(-?(?:0|[1-9]\d{0,17})))?\s*\]\s*,\s*(%[\w$]+|-?(?:0|[1-9]\d{0,9}))\s*$)");
	for (int i = (int)ps.size() - 1; i >= 0; i--) {
		const piece &p = ps[i];
		const std::string &st = p.text;
		std::string op, rest;
		split_op(st, &op, &rest);
		const std::string base_op = op.substr(0, op.find('.'));
		const bool is_store = base_op == "st";
		if (is_store && !p.guarded) {
			std::smatch m;
			if (!std::regex_match(st, m, store))
				return ""; // a store the pass cannot read
			std::string sb;
			long long so = 0;
			if (!stack_slot(fbody, m[3], &sb, &so))
				return ""; // it may write the slot
			if (m[4].matched &&
			    __builtin_add_overflow(so, std::stoll(m[4]), &so))
				return "";
			const long long w = std::stoll(m[2]) / 8;
			long long end = 0, kend = 0;
			if (__builtin_add_overflow(so, w, &end) ||
			    __builtin_add_overflow(ko, 4LL, &kend))
				return "";
			if (end <= ko || so >= kend) {
				// a store into another slot of the frame
				if (p.label || p.brace)
					return "";
				continue;
			}
			if (so != ko || w != 4)
				return ""; // part of the key
			if (m[1].matched != (sb == "%SPL"))
				return ""; // not the depot's bytes
			{
				const std::string v = m[5];
				if (v[0] == '%') {
					std::string after;
					for (size_t j = i + 1; j < ps.size(); j++)
						after += ps[j].text + ";";
					if (count_defs(after, v) != 0)
						return "";
					*width = reg_width(fbody, v);
					if (*width == 0)
						return "";
				} else {
					*width = 32;
				}
				return v;
			}
		}
		// between the slot's store and the call
		if (p.label || p.brace)
			return ""; // another path or scope
		if (st.empty())
			continue;
		if (st[0] == '.' || !no_memory_write.count(base_op))
			return ""; // it may write memory
	}
	return "";
}

// the key into a 32-bit register: forwarded, or loaded from the slot
std::string key_into(const std::string &dst, const std::string &key_ptr,
		     const std::string &fwd, int width)
{
	if (fwd.empty())
		return "\tld.u32 \t" + dst + ", [" + key_ptr + "];\n";
	if (width == 64)
		return "\tcvt.u32.u64 \t" + dst + ", " + fwd + ";\n";
	return "\tmov.u32 \t" + dst + ", " + fwd + ";\n";
}

// insert a state space before the operation of atom/red modifiers
// (atom{.sem}{.scope}{.space}.op.type)
std::string with_space(const std::string &mods, const std::string &op,
		       const char *space)
{
	size_t at = (mods + ".").find("." + op + ".");
	if (at == std::string::npos)
		fail("no operation in modifiers " + mods);
	return mods.substr(0, at) + space + mods.substr(at);
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
	// a register written by one statement only, a mov of an integer
	if (count_defs(fbody, arg) != 1)
		return false;
	std::regex def(R"(mov\.[ubs]64\s+)" + std::regex_replace(
						   arg, std::regex(R"([$%.])"),
						   R"(\$&)") +
		       R"(\s*,\s*(-?\d+)\s*;)");
	std::smatch dm;
	if (!std::regex_search(fbody, dm, def))
		return false;
	long long val = std::stoll(dm[1]);
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
				  const std::string &fname, int *count,
				  const std::set<std::string> &global_regs,
				  int *global_count)
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
		const std::string ws = m[1];
		// the operands with the address replaced
		auto ops_at = [&](const std::string &a) {
			std::string r;
			for (size_t i = 0; i < parts.size(); i++)
				r += (i ? ", " : "") +
				     (i == ai ? "[" + a + "]" : parts[i]);
			return r;
		};
		const std::string gmods = with_space(mods, op, ".global");
		if (global_regs.count(base) && op == "add" && bits == "64") {
			// a sum map's value: global memory (see the header)
			const std::string id = std::to_string((*global_count)++);
			std::ostringstream o;
			o << ws << "{ // bpftime: atomic on a sum map's value\n"
			  << ws << ".reg .b64 %bpag_a" << id << ";\n"
			  << ws << "add.u64 %bpag_a" << id << ", " << base << ", "
			  << off << ";\n"
			  << ws << "cvta.to.global.u64 %bpag_a" << id
			  << ", %bpag_a" << id << ";\n"
			  << ws << m[3].str() << gmods << " "
			  << ops_at("%bpag_a" + id) << ";\n"
			  << ws << "}" << m[6].str();
			out += o.str();
			continue;
		}
		const std::string id = std::to_string(n++);
		std::ostringstream o;
		// global addresses take the .global form (no wait for a
		// generic atomic's reply), local ones the plain operation,
		// anything else the generic atomic as written
		o << ws << "{ // bpftime: atomic on a possibly local address\n"
		  << ws << ".reg .pred %bpat_l" << id << ", %bpat_q" << id
		  << ", %bpat_g" << id << ", %bpat_x" << id << ";\n"
		  << ws << ".reg .b64 %bpat_a" << id << ", %bpat_ga" << id
		  << ";\n"
		  << ws << ".reg .b" << bits << " %bpat_o" << id
		  << ", %bpat_n" << id << ";\n"
		  << ws << "add.u64 %bpat_a" << id << ", " << base << ", "
		  << off << ";\n"
		  << ws << "isspacep.local %bpat_l" << id << ", %bpat_a" << id
		  << ";\n"
		  << ws << "isspacep.global %bpat_g" << id << ", %bpat_a" << id
		  << ";\n"
		  << ws << "or.pred %bpat_x" << id << ", %bpat_l" << id
		  << ", %bpat_g" << id << ";\n"
		  << ws << "@%bpat_g" << id << " cvta.to.global.u64 %bpat_ga"
		  << id << ", %bpat_a" << id << ";\n"
		  << ws << "@%bpat_g" << id << " " << m[3].str() << gmods << " "
		  << ops_at("%bpat_ga" + id) << ";\n"
		  << ws << "@!%bpat_x" << id << " " << m[3].str() << mods << " "
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

bool sum_has_copy(const sum_map_desc &d)
{
	return d.base != 0 && d.shards >= 32 && !(d.shards & (d.shards - 1)) &&
	       d.max_entries != 0 && d.value_size != 0;
}

// A helper call block that the pass replaces holds nothing else: its
// .reg/.param declarations, the argument stores, the call and the return
// load. Anything more (an instruction llvmbpf never puts there) would be
// dropped with the block, so the caller refuses the module instead.
bool plain_call_block(const std::string &block)
{
	static const std::regex ok(
		R"(^(?:\.reg\s[\s\S]*|\.param\s[\s\S]*|st\.param\.b64\s+\[param\d+\+0\]\s*,\s*[^\s,]+|call\.uni\s[\s\S]*|ld\.param\.b64\s+%[\w$]+\s*,\s*\[retval0\+0\])$)");
	for (const auto &p : pieces(block)) {
		if (p.text.empty())
			continue;
		if (p.guarded || p.label || !std::regex_match(p.text, ok))
			return false;
	}
	return true;
}

// a constant-fd lookup of a sum map with a device copy: the shard address
// of the key, as the trampoline's sum_array_offset ((SM mod slots) x 32 +
// lane), or NULL for a key out of range
std::string sum_lookup(const sum_map_desc &d, long long fd, int n,
		       const std::string &key_ptr, const std::string &ret,
		       const std::string &fwd, int fwd_width)
{
	std::ostringstream o;
	const std::string N = std::to_string(n);
	o << "{ // bpftime sum-map lookup, map " << fd << "\n";
	o << "\t.reg .pred \t%bpsm_p" << N << ";\n"
	  << "\t.reg .b32 \t%bpsm_k" << N << ", %bpsm_s" << N << ", %bpsm_l"
	  << N << ";\n"
	  << "\t.reg .b64 \t%bpsm_a" << N << ", %bpsm_w" << N << ";\n"
	  << key_into("%bpsm_k" + N, key_ptr, fwd, fwd_width)
	  << "\tsetp.lt.u32 \t%bpsm_p" << N << ", %bpsm_k" << N << ", "
	  << d.max_entries << ";\n"
	  << "\tmov.u32 \t%bpsm_s" << N << ", %smid;\n"
	  << "\tand.b32 \t%bpsm_s" << N << ", %bpsm_s" << N << ", "
	  << ((d.shards >> 5) - 1) << ";\n"
	  << "\tshl.b32 \t%bpsm_s" << N << ", %bpsm_s" << N << ", 5;\n"
	  << "\tmov.u32 \t%bpsm_l" << N << ", %laneid;\n"
	  << "\tor.b32 \t%bpsm_s" << N << ", %bpsm_s" << N << ", %bpsm_l"
	  << N << ";\n";
	if (d.max_entries * d.shards <= (1ull << 32)) {
		// the index (key x shards + shard) fits in 32 bits
		o << "\tmad.lo.u32 \t%bpsm_k" << N << ", %bpsm_k" << N << ", "
		  << d.shards << ", %bpsm_s" << N << ";\n"
		  << "\tmul.wide.u32 \t%bpsm_a" << N << ", %bpsm_k" << N
		  << ", " << d.value_size << ";\n";
	} else {
		o << "\tmul.wide.u32 \t%bpsm_a" << N << ", %bpsm_k" << N << ", "
		  << d.shards << ";\n"
		  << "\tcvt.u64.u32 \t%bpsm_w" << N << ", %bpsm_s" << N
		  << ";\n"
		  << "\tadd.u64 \t%bpsm_a" << N << ", %bpsm_a" << N
		  << ", %bpsm_w" << N << ";\n"
		  << "\tmul.lo.u64 \t%bpsm_a" << N << ", %bpsm_a" << N << ", "
		  << d.value_size << ";\n";
	}
	o << "\tadd.u64 \t%bpsm_a" << N << ", %bpsm_a" << N << ", " << d.base
	  << ";\n"
	  << "\tselp.b64 \t" << ret << ", %bpsm_a" << N << ", 0, %bpsm_p" << N
	  << ";\n"
	  << "} // bpftime sum-map lookup\n";
	return o.str();
}

std::string route_function_body(
	const std::string &fbody, bool routed,
	const std::map<long long, thread_local_map_desc> &maps,
	const std::map<long long, sum_map_desc> &sums,
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
	int sum_seq = 0;
	// registers sum lookups define, with the number of such definitions
	std::map<std::string, int> sum_defs;
	for (;;) {
		size_t b = fbody.find("{ // callseq ", pos);
		if (b == std::string::npos)
			break;
		// a marker only at the start of a line (not inside a comment)
		{
			size_t ls = fbody.rfind('\n', b);
			ls = ls == std::string::npos ? 0 : ls + 1;
			if (fbody.find_first_not_of(" \t", ls) != b) {
				out.append(fbody, pos, b + 1 - pos);
				pos = b + 1;
				continue;
			}
		}
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
		auto sm = is_const ? sums.find(fd) : sums.end();
		// with a device copy only: without one the trampoline answers
		// from the run-time map_info (a copy made later, after a cached
		// image was patched, is then still found)
		if (id == 1 && sm != sums.end() && sum_has_copy(sm->second)) {
			if (!plain_call_block(block))
				fail("unexpected instructions in a helper call of " +
				     fname);
			std::smatch km, rm;
			if (!std::regex_search(block, km, p1) ||
			    !std::regex_search(block, rm, ret))
				fail("unexpected lookup shape in " + fname);
			int fwd_width = 0;
			std::string fwd =
				forwarded_key(fbody, b, trim(km[1]), &fwd_width);
			if (!fwd.empty())
				st.keys_forwarded++;
			out += sum_lookup(sm->second, fd, sum_seq++,
					  trim(km[1]), rm[1].str(), fwd,
					  fwd_width);
			sum_defs[rm[1].str()]++;
			st.sum_lookups++;
			continue;
		}
		if (routed && id == 1 && tl != maps.end()) {
			if (!plain_call_block(block))
				fail("unexpected instructions in a helper call of " +
				     fname);
			std::smatch km, rm;
			if (!std::regex_search(block, km, p1) ||
			    !std::regex_search(block, rm, ret))
				fail("unexpected lookup shape in " + fname);
			const auto &d = tl->second;
			int fwd_width = 0;
			std::string fwd =
				forwarded_key(fbody, b, trim(km[1]), &fwd_width);
			if (!fwd.empty())
				st.keys_forwarded++;
			std::ostringstream o;
			int n = direct_seq++;
			o << "{ // bpftime thread-local lookup, map " << fd
			  << "\n\t.reg .pred \t%bptl_p" << n
			  << ";\n\t.reg .b32 \t%bptl_k" << n
			  << ";\n\t.reg .b64 \t%bptl_a" << n << ";\n"
			  << key_into("%bptl_k" + std::to_string(n),
				      trim(km[1]), fwd, fwd_width)
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
	// a register every definition of which is such a lookup (llvmbpf
	// joins the lookups of several branches into one register) holds a
	// sum map's address wherever it is used
	std::set<std::string> sum_ptrs;
	for (const auto &[reg, n] : sum_defs)
		if (count_defs(out, reg) == n)
			sum_ptrs.insert(reg);
	return guard_generic_atomics(out, fname, &st.guarded_atomics, sum_ptrs,
				     &st.global_atomics);
}
} // namespace

std::string route_thread_local_maps(const std::string &ptx_in,
				    const std::vector<std::string> &kernels_in,
				    const std::vector<thread_local_map_desc> &descs,
				    uint64_t area_bytes,
				    thread_local_routing_stats *stats_out,
				    const std::vector<sum_map_desc> &sum_descs)
{
	thread_local_routing_stats st;
	std::map<long long, sum_map_desc> sums;
	for (auto &d : sum_descs)
		sums[d.fd] = d;
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
	// (only with a thread-local map)
	std::set<std::string> routed_functions;
	if (descs.empty())
		kernels.clear();
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
		std::string nbody = route_function_body(fbody, routed, maps,
							sums, st, f.name);
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
