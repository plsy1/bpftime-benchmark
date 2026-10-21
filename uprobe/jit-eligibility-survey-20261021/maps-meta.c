// Print libbpf's view of an unloaded BPF object as JSON (no kernel access).
#include <bpf/libbpf.h>
#include <stdarg.h>
#include <stdio.h>

static int quiet(enum libbpf_print_level level, const char *fmt, va_list args)
{
	(void)level; (void)fmt; (void)args;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
	libbpf_set_print(quiet);
	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj) {
		printf("{\"error\":\"open failed\"}\n");
		return 1;
	}
	struct bpf_map *map;
	struct bpf_program *prog;
	const char *sep = "";
	printf("{\"maps\":[");
	bpf_object__for_each_map(map, obj) {
		printf("%s{\"name\":\"%s\",\"type\":%d,\"key_size\":%u,\"value_size\":%u,\"internal\":%s}",
		       sep, bpf_map__name(map), bpf_map__type(map), bpf_map__key_size(map),
		       bpf_map__value_size(map), bpf_map__is_internal(map) ? "true" : "false");
		sep = ",";
	}
	sep = "";
	printf("],\"programs\":[");
	bpf_object__for_each_program(prog, obj) {
		printf("%s{\"name\":\"%s\",\"section\":\"%s\"}", sep, bpf_program__name(prog),
		       bpf_program__section_name(prog));
		sep = ",";
	}
	printf("]}\n");
	bpf_object__close(obj);
	return 0;
}
