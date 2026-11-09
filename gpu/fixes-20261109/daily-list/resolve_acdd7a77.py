#!/usr/bin/env python3
"""Resolves acdd7a77 (load-time substitution) cherry-picked onto official 4708f409, which has
upstream #709: #709 adds clear_patched_state_for_next_session() to the late-bootstrap rerun
block of create_attach_with_ebpf_callback, which acdd7a77 moved after load_guard.unlock()
(the call takes late_bootstrap_mutex and load_substitution_mutex). Keep acdd7a77's structure
and put #709's call into the moved block. Run in the worktree, mid cherry-pick."""
from pathlib import Path
p = Path("attach/nv_attach_impl/nv_attach_impl.cpp")
t = p.read_text()
start = t.index("<<<<<<< HEAD\n")
end_marker = ">>>>>>> acdd7a77 (nv_attach: patch fatbins as cudart loads them (load-time substitution))\n"
end = t.index(end_marker) + len(end_marker)
removed = t[start:end]
assert "clear_patched_state_for_next_session();" in removed
t = t[:start] + t[end:]
old = """				"nv_attach_impl: new hook added after bootstrap; rerunning late bootstrap");
			reset_late_bootstrap_state_for_next_attach();
		}
		if (substitutes_at_load() && !loaded_already) {"""
new = """				"nv_attach_impl: new hook added after bootstrap; rerunning late bootstrap");
			// Drop the previous bootstrap's module set so the rerun doesn't
			// leave two sets with split device-global addresses
			// (upstream #709). It takes late_bootstrap_mutex and
			// load_substitution_mutex: after load_guard is released.
			clear_patched_state_for_next_session();
			reset_late_bootstrap_state_for_next_attach();
		}
		if (substitutes_at_load() && !loaded_already) {"""
assert t.count(old) == 1
t = t.replace(old, new)
assert "<<<<<<<" not in t and ">>>>>>>" not in t
p.write_text(t)
print("resolved")
