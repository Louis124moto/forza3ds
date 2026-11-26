/* Host-test stand-in for <3ds.h>: only what the portable modules use.
 * Key values match libctru/include/3ds/services/hid.h. */
#ifndef F3DS_TEST_3DS_H
#define F3DS_TEST_3DS_H
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t s16;
typedef int32_t s32;

#define BIT(n) (1U << (n))

enum {
	KEY_A = BIT(0), KEY_B = BIT(1), KEY_SELECT = BIT(2), KEY_START = BIT(3),
	KEY_DRIGHT = BIT(4), KEY_DLEFT = BIT(5), KEY_DUP = BIT(6), KEY_DDOWN = BIT(7),
	KEY_R = BIT(8), KEY_L = BIT(9), KEY_X = BIT(10), KEY_Y = BIT(11),
	KEY_ZL = BIT(14), KEY_ZR = BIT(15), KEY_TOUCH = BIT(20),
	KEY_CSTICK_RIGHT = BIT(24), KEY_CSTICK_LEFT = BIT(25),
	KEY_CSTICK_UP = BIT(26), KEY_CSTICK_DOWN = BIT(27),
	KEY_CPAD_RIGHT = BIT(28), KEY_CPAD_LEFT = BIT(29),
	KEY_CPAD_UP = BIT(30), KEY_CPAD_DOWN = BIT(31),
	KEY_UP = KEY_DUP | KEY_CPAD_UP, KEY_DOWN = KEY_DDOWN | KEY_CPAD_DOWN,
	KEY_LEFT = KEY_DLEFT | KEY_CPAD_LEFT, KEY_RIGHT = KEY_DRIGHT | KEY_CPAD_RIGHT
};
#endif
