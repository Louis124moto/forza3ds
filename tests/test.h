/* Minimal check macros: report every failure, keep going. */
#ifndef F3DS_TEST_H
#define F3DS_TEST_H
#include <stdio.h>

extern int g_checks, g_failures;

#define CHECK(cond) CHECKF(cond, "%s", "")
#define CHECKF(cond, ...) do { \
	g_checks++; \
	if (!(cond)) { \
		g_failures++; \
		printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} \
} while (0)

void test_core_input_camera(void);
void test_physics(void);
void test_world(void);
void test_island(void);
void test_saves(const char *dir);
void test_workshop_audio(void);
void test_app(const char *dir);
void test_render(void);

#endif
