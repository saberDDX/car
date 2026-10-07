/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CAR_KEY_VERIFY_H
#define CAR_KEY_VERIFY_H
#include <linux/input.h>
#include <stddef.h>

struct key_verification {
	unsigned long target, presses, releases, repeats;
	int down, long_press;
	long long press_ms;
};

/* Accept alternating KEY_CAMERA transitions and at least one >=2s hold. */
static inline const char *key_verify_event(struct key_verification *state,
					 const struct input_event *event)
{
	long long now_ms;

	if (event->type != EV_KEY)
		return NULL;
	if (event->code != KEY_CAMERA)
		return "Unexpected keycode (expected KEY_CAMERA/212).";
	now_ms = (long long)event->time.tv_sec * 1000 + event->time.tv_usec / 1000;
	if (event->value == 1) {
		if (state->down)
			return "Duplicate press without a release.";
		state->down = 1;
		state->press_ms = now_ms;
		if (++state->presses > state->target)
			return "More presses than requested; check bounce or extra clicks.";
	} else if (event->value == 0) {
		if (!state->down)
			return "Release without a preceding press.";
		state->down = 0;
		++state->releases;
		if (now_ms < state->press_ms)
			return "Non-monotonic event timestamps.";
		if (now_ms - state->press_ms >= 2000)
			state->long_press = 1;
	} else if (event->value == 2) {
		++state->repeats;
		return "Auto-repeat was observed; this driver must not repeat.";
	} else {
		return "Invalid key event value.";
	}
	return NULL;
}

static inline int key_verify_balanced(const struct key_verification *state)
{
	return state->presses == state->target && state->releases == state->target && !state->down;
}

static inline const char *key_verify_result(const struct key_verification *state)
{
	if (!key_verify_balanced(state))
		return "The requested press/release pairs were not completed.";
	if (state->repeats)
		return "Auto-repeat was observed.";
	if (!state->long_press)
		return "No >=2-second hold was observed; hold one press for 3 seconds.";
	return NULL;
}
#endif
