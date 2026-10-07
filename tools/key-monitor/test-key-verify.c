/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include "key-verify.h"

static const char *send(struct key_verification *state, unsigned int code, int value, long long ms)
{
	struct input_event event = { 0 };

	event.type = EV_KEY;
	event.code = code;
	event.value = value;
	event.time.tv_sec = ms / 1000;
	event.time.tv_usec = (ms % 1000) * 1000;
	return key_verify_event(state, &event);
}

int main(void)
{
	struct key_verification state = { .target = 20 };
	unsigned int i;

	assert(key_verify_result(&state)); /* Zero events must fail. */
	for (i = 0; i < 20; i++) {
		long long start = i * 5000;

		assert(!send(&state, KEY_CAMERA, 1, start));
		assert(!send(&state, KEY_CAMERA, 0, start + (i == 0 ? 3000 : 200)));
	}
	assert(key_verify_balanced(&state));
	assert(!key_verify_result(&state));
	assert(send(&state, KEY_CAMERA, 1, 110000)); /* Extra bounce after target. */
	state = (struct key_verification){ .target = 1 };
	assert(send(&state, KEY_CAMERA, 0, 100)); /* Out-of-order release. */
	assert(send(&state, KEY_ENTER, 1, 100)); /* Wrong hardware keycode. */
	assert(!send(&state, KEY_CAMERA, 1, 100));
	assert(send(&state, KEY_CAMERA, 1, 200)); /* Duplicate press. */
	assert(send(&state, KEY_CAMERA, 2, 500)); /* Repeats fail. */
	state = (struct key_verification){ .target = 1 };
	assert(!send(&state, KEY_CAMERA, 1, 100));
	assert(!send(&state, KEY_CAMERA, 0, 300));
	assert(key_verify_result(&state)); /* Short clicks do not validate long holds. */
	state = (struct key_verification){ .target = 1 };
	assert(!send(&state, KEY_CAMERA, 1, 3000));
	assert(send(&state, KEY_CAMERA, 0, 2000)); /* Bad timestamps. */
	state = (struct key_verification){ .target = 1 };
	assert(send(&state, KEY_CAMERA, 3, 2000)); /* Invalid event value. */
	puts("PASS: key verifier accepts 20 balanced pairs with a long hold and rejects invalid sequences.");
	return 0;
}
