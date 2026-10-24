Stopped before tests on new bpftime_prog ODR/type-mismatch warnings. Primary
header edits were copied with preserved mtimes while an earlier build still
compiled old headers; some newer objects were incorrectly considered current.
The revised runner refreshes each target's compiler dependencies. Copied
interface/test timestamps were refreshed before rebuilding. No timing from
this attempt is valid. The earlier ebpf_inst upstream/compat ODR warning is a
separate existing warning; do not treat new class-layout warnings as harmless.
