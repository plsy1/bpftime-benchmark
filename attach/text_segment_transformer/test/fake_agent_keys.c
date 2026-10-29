// Stands in for the agent in test_agent_namespace.cpp: loaded into its own
// link-map namespace (its own libc), it creates pthread keys in its
// constructor and stores known values on the loading thread, as GLib does.
#include <pthread.h>

#define FAKE_AGENT_KEYS 6

static pthread_key_t keys[FAKE_AGENT_KEYS];
static int created;

int fake_agent_set(void)
{
	for (int i = 0; i < created; i++)
		if (pthread_setspecific(keys[i], (void *)(0xa000L + i)) != 0)
			return -1;
	return 0;
}

__attribute__((constructor)) static void fake_agent_init(void)
{
	for (int i = 0; i < FAKE_AGENT_KEYS; i++) {
		if (pthread_key_create(&keys[i], 0) != 0)
			return;
		created++;
	}
	fake_agent_set();
}

// Values of this thread that are not the ones fake_agent_set stored; -1 when
// not every key could be created.
int fake_agent_wrong(void)
{
	if (created != FAKE_AGENT_KEYS)
		return -1;
	int wrong = 0;
	for (int i = 0; i < FAKE_AGENT_KEYS; i++)
		if (pthread_getspecific(keys[i]) != (void *)(0xa000L + i))
			wrong++;
	return wrong;
}

unsigned fake_agent_key(int i)
{
	return keys[i];
}

int fake_agent_key_count(void)
{
	return FAKE_AGENT_KEYS;
}
