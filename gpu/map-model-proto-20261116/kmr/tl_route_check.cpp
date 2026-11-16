// Runs the agent's thread-local routing (nv_thread_local_routing.cpp) on a
// PTX file and prints the routed PTX and its statistics.
// usage: tl_route_check IN.ptx OUT.ptx AREA KERNEL,... FD:OFF:VS:N ...
#include "nv_thread_local_routing.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
namespace ptxpass
{
// copy of ptxpass_core's find_kernel_body (core.cpp)
std::pair<size_t, size_t> find_kernel_body(const std::string &ptx,
					   const std::string &kernel)
{
	static std::regex kernel_entry(
		R"((\.visible\s+)?\.entry\s+(\w+)\s*\(([^)]*)\))");
	std::smatch m;
	std::string::const_iterator search_start(ptx.cbegin());
	while (std::regex_search(search_start, ptx.cend(), m, kernel_entry)) {
		if (m[2] == kernel) {
			size_t begin = (size_t)(m[0].first - ptx.cbegin());
			size_t end = begin;
			std::vector<char> st;
			do {
				while (end < ptx.size() && ptx[end] != '{' &&
				       ptx[end] != '}')
					end++;
				if (end >= ptx.size())
					break;
				if (ptx[end] == '{')
					st.push_back('{');
				else
					st.pop_back();
				end++;
			} while (!st.empty());
			return { begin, end };
		}
		search_start = m.suffix().first;
	}
	return { std::string::npos, std::string::npos };
}
} // namespace ptxpass
int main(int argc, char **argv)
{
	std::ifstream in(argv[1]);
	std::stringstream ss;
	ss << in.rdbuf();
	std::vector<std::string> kernels;
	std::stringstream ks(argv[4]);
	for (std::string k; std::getline(ks, k, ',');)
		kernels.push_back(k);
	std::vector<bpftime::attach::thread_local_map_desc> d;
	for (int i = 5; i < argc; i++) {
		bpftime::attach::thread_local_map_desc x{};
		sscanf(argv[i], "%d:%lu:%lu:%lu", &x.fd, &x.offset,
		       &x.value_size, &x.max_entries);
		d.push_back(x);
	}
	bpftime::attach::thread_local_routing_stats st;
	auto out = bpftime::attach::route_thread_local_maps(
		ss.str(), kernels, d, strtoull(argv[3], nullptr, 10), &st);
	std::ofstream(argv[2]) << out;
	std::cout << "kernels " << st.kernels << " call_sites " << st.call_sites
		  << " routed " << st.routed_functions << " unrouted "
		  << st.unrouted_functions << " direct " << st.direct_lookups
		  << " area_args " << st.helper_calls_given_area << " zero_args "
		  << st.helper_calls_given_zero << " guarded_atomics "
		  << st.guarded_atomics << "\n";
}
