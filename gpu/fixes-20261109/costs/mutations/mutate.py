import sys
root='/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/attach/nv_attach_impl/'
m=sys.argv[1]
def sub(f, old, new):
    p=root+f; s=open(p).read(); assert old in s, (m, old[:50]); open(p,'w').write(s.replace(old,new,1))
E='nv_elf_introspect.cpp'; I='nv_attach_impl.cpp'
if m=='eager-all-modules':
    sub(E,'''	auto key = std::make_pair(mod->path, mod->base_addr);''','''	for (const auto &other : list_loaded_modules()) {
		auto k = std::make_pair(other.path, other.base_addr);
		if (!symbols_by_module.count(k))
			symbols_by_module.emplace(k, read_function_symbols(other));
	}
	auto key = std::make_pair(mod->path, mod->base_addr);''')
elif m=='keep-mapping-symbols':
    sub(E,"if (name[0] == '\\0' || name[0] == '$')","if (name[0] == '\\0')")
elif m=='old-naming':
    # no module containment (search the executable), unsized symbols name everything above them
    sub(E,'''	auto mod = module_containing(addr);
	if (!mod || mod->path.empty())
		return std::nullopt;''','''	auto mod = std::optional<loaded_module>(list_loaded_modules().front());''')
    sub(E,'''	if (sym.end > sym.start ? addr >= sym.end : addr != sym.start)''','''	if (sym.end > sym.start && addr >= sym.end)''')
elif m=='read-every-candidate':
    sub(I,'''		if (start == nullptr || (read_end != nullptr && start < read_end))''','''		if (start == nullptr)''')
elif m=='ptx-always':
    sub(I,'''	const auto read = [&](std::size_t off, auto &value) {''','''	return true;
	const auto read = [&](std::size_t off, auto &value) {''')
else:
    raise SystemExit('unknown')
