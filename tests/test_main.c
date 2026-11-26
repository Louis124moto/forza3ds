#include "test.h"
#include <stdlib.h>

int g_checks, g_failures;

int main(void)
{
	char dir[] = "/tmp/f3ds-test-XXXXXX";
	if (!mkdtemp(dir)) {
		perror("mkdtemp");
		return 2;
	}
	test_core_input_camera();
	test_physics();
	test_world();
	test_saves(dir);
	test_workshop_audio();
	test_app(dir);
	test_render();
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
