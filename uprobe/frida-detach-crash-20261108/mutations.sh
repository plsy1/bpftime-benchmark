#!/bin/bash
# mutations.sh <name>: applies one mutation to the worktree's Frida impl.
# M1 gum-detach: the last owner's removal detaches Gum's listeners and drops
#    the slot (Gum reverts the code under load, and re-hooks every cycle):
#    the behaviour before frida_hook_slot.
# M2 no-grace: synchronize() returns at once (dispatches freed under callbacks).
# M3 live-lookup: the override handler checks the owners' live attaches
#    (has_override) and throws when they have none (the bug before the dispatch's
#    override_attach).
set -eu
cd /home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/frida-detach-crash
case $1 in
gum-detach)
  perl -0pi -e 's/(\t\t     owners\.end\(\)\);\n\trepublish_locked\(\*slot\);\n)/$1\tif (owners.empty()) { GumInterceptor *i = gum_interceptor_obtain(); gum_interceptor_detach(i, slot->enter_listener); if (slot->leave_listener) gum_interceptor_detach(i, slot->leave_listener); g_object_unref(i); slots().erase(slot->function); }\n/' attach/frida_uprobe_attach_impl/src/frida_hook_slot.cpp ;;
no-grace)
  perl -0pi -e 's/(void bpftime::attach::probe_grace::synchronize\(\)\n\{\n)/$1\treturn;\n/' attach/frida_uprobe_attach_impl/src/frida_probe_grace.cpp ;;
live-lookup)
  perl -0pi -e 's/\tif \(const frida_attach_entry \*attach =\n\t\t    dispatch \? dispatch->override_attach : nullptr\) \{/\tconst frida_attach_entry *attach = dispatch ? dispatch->override_attach : nullptr;\n\tif (attach) { bool live = false; for (auto *o : slot->owners) live = live || o->has_override(); if (!live) throw std::runtime_error("no override"); }\n\tif (attach) {/' attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp ;;
*) echo "unknown mutation $1"; exit 2 ;;
esac
git diff --stat -- attach/frida_uprobe_attach_impl/src
