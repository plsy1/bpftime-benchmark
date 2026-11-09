#include "catch2/catch_test_macros.hpp"
#include "nv_attach_fatbin_record.hpp"
#include "nv_attach_impl.hpp"
#include <stdexcept>

using namespace bpftime::attach;

// nvPTXCompiler errors were thrown inside a boost thread pool task, and an
// exception leaving a pool thread ends the process (std::terminate): the
// traced application died instead of running unpatched (results
// gpu/examples-20261109, "found by reading only"). They reach the caller now,
// which keeps the original module.
TEST_CASE("A PTX compile error reaches the caller instead of ending the process",
	  "[ptx-failure]")
{
	nv_attach_impl impl;
	// Checked before compiling; nothing is loaded or launched here.
	impl.shared_mem_ptr = 1;
	fatbin_record rec;
	rec.module_pool = impl.module_pool;
	rec.ptx_pool = impl.ptx_pool;
	rec.original_ptx["broken.1.sm_87.ptx"] =
		".version 8.1\n.target sm_87\n.address_size 64\n"
		".visible .entry broken()\n{\n\tnot_an_instruction;\n}\n";
	REQUIRE_THROWS_AS(rec.try_loading_ptxs(impl), std::runtime_error);
	REQUIRE_FALSE(rec.ptx_loaded);
}
