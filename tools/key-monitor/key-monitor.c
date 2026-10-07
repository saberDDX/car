/* SPDX-License-Identifier: GPL-2.0 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include "key-verify.h"

static volatile sig_atomic_t stopped;

static void stop_monitor(int signal_number)
{
	(void)signal_number;
	stopped = 1;
}

static int open_key(const char *path)
{
	char name[128] = { 0 };
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		return -1;
	if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return -1;
	}
	name[sizeof(name) - 1] = '\0';
	if (strcmp(name, "car-gpio-key") != 0) {
		close(fd);
		errno = ENODEV;
		return -1;
	}
	return fd;
}

static int discover_key(void)
{
	glob_t paths;
	int selected = -1;
	size_t i;
	int result;

	memset(&paths, 0, sizeof(paths));
	result = glob("/dev/input/event*", 0, NULL, &paths);
	if (result != 0) {
		globfree(&paths);
		fprintf(stderr, "No input event paths found (glob status %d).\n", result);
		return -1;
	}
	for (i = 0; i < paths.gl_pathc; ++i) {
		int candidate = open_key(paths.gl_pathv[i]);

		if (candidate < 0)
			continue;
		if (selected >= 0) {
			close(selected);
			close(candidate);
			globfree(&paths);
			fprintf(stderr, "Multiple car-gpio-key devices; specify one event path.\n");
			return -1;
		}
		selected = candidate;
		printf("Device: %s\n", paths.gl_pathv[i]);
	}
	globfree(&paths);
	if (selected < 0)
		fprintf(stderr, "No accessible car-gpio-key device. Check probe, device tree, and permissions.\n");
	return selected;
}

int main(int argc, char **argv)
{
	struct sigaction action;
	struct pollfd descriptor;
	unsigned long presses = 0, releases = 0, repeats = 0;
	struct key_verification verification = { 0 };
	const char *path = NULL;
	int verify = 0;
	long long quiet_until = 0;
	int fd;
	int status = EXIT_SUCCESS;

	if ((argc == 3 || argc == 4) && strcmp(argv[1], "--verify") == 0) {
		char *end;

		errno = 0;
		verification.target = strtoul(argv[2], &end, 10);
		if (errno || *end || !verification.target || verification.target > 1000)
			goto usage;
		verify = 1;
		if (argc == 4)
			path = argv[3];
	} else if (argc == 2 && argv[1][0] != '-') {
		path = argv[1];
	} else if (argc != 1) {
		goto usage;
	}
	fd = path ? open_key(path) : discover_key();
	if (fd < 0) {
		if (path)
			fprintf(stderr, "Cannot open car key %s: %s\n", path, strerror(errno));
		return EXIT_FAILURE;
	}
	if (verify) {
		unsigned long keys[(KEY_MAX + 1 + sizeof(unsigned long) * 8 - 1) /
				   (sizeof(unsigned long) * 8)] = { 0 };
		const unsigned int bits = sizeof(unsigned long) * 8;
		int clock_id = CLOCK_MONOTONIC;

		if (ioctl(fd, EVIOCSCLOCKID, &clock_id) < 0 ||
		    ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) < 0) {
			perror("Cannot initialize key verification");
			close(fd);
			return EXIT_FAILURE;
		}
		if (keys[KEY_CAMERA / bits] & (1UL << (KEY_CAMERA % bits))) {
			fprintf(stderr, "Release K1 before starting verification.\n");
			close(fd);
			return EXIT_FAILURE;
		}
	}
	memset(&action, 0, sizeof(action));
	action.sa_handler = stop_monitor;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0 ||
	    sigaction(SIGTERM, &action, NULL) < 0) {
		perror("sigaction");
		close(fd);
		return EXIT_FAILURE;
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	descriptor.fd = fd;
	descriptor.events = POLLIN;
	if (verify)
		printf("Verify %lu press/release pairs. Hold one press for 3 seconds; stop pressing after the target.\n",
		       verification.target);
	else
		printf("Watching car key. Press and release 20 times; Ctrl+C prints totals.\n");
	while (!stopped) {
		struct input_event events[16];
		ssize_t bytes;
		size_t i, count;
		if (quiet_until) {
			struct timespec now;

			if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
				perror("clock_gettime");
				status = EXIT_FAILURE;
				break;
			}
			if ((long long)now.tv_sec * 1000 + now.tv_nsec / 1000000 >= quiet_until)
				break;
		}
		int ready = poll(&descriptor, 1, 1000);

		if (ready < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			status = EXIT_FAILURE;
			break;
		}
		if (!ready)
			continue;
		if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "Input device disconnected or unavailable.\n");
			status = EXIT_FAILURE;
			break;
		}
		if (!(descriptor.revents & POLLIN))
			continue;
		bytes = read(fd, events, sizeof(events));
		if (bytes < 0 && errno == EINTR)
			continue;
		if (bytes <= 0 || (size_t)bytes % sizeof(events[0]) != 0) {
			fprintf(stderr, "Invalid input read: %zd bytes%s%s\n", bytes,
				bytes < 0 ? ": " : "", bytes < 0 ? strerror(errno) : "");
			status = EXIT_FAILURE;
			break;
		}
		count = (size_t)bytes / sizeof(events[0]);
		for (i = 0; i < count; ++i) {
			const struct input_event *event = &events[i];

			if (event->type == EV_SYN && event->code == SYN_DROPPED) {
				fprintf(stderr, "Input queue overflowed; counts are incomplete.\n");
				status = EXIT_FAILURE;
				stopped = 1;
				break;
			}
			if (event->type != EV_KEY)
				continue;
			if (event->value == 1)
				++presses;
			else if (event->value == 0)
				++releases;
			else if (event->value == 2)
				++repeats;
			printf("code=%u value=%d (%s)\n", event->code, event->value,
			       event->value == 1 ? "pressed" :
			       event->value == 0 ? "released" :
			       event->value == 2 ? "repeat" : "unknown");
			if (verify) {
				const char *error = key_verify_event(&verification, event);

				if (error) {
					fprintf(stderr, "%s\n", error);
					status = EXIT_FAILURE;
					stopped = 1;
					break;
				}
				if (key_verify_balanced(&verification) && !quiet_until) {
					struct timespec now;

					if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
						perror("clock_gettime");
						status = EXIT_FAILURE;
						stopped = 1;
						break;
					}
					quiet_until = (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000 + 1000;
				}
			}
		}
	}
	printf("Totals: pressed=%lu released=%lu repeated=%lu\n", presses, releases, repeats);
	if (!presses) {
		printf("No key presses observed; this run does not validate key behavior.\n");
		status = EXIT_FAILURE;
	}
	if (verify) {
		const char *error = key_verify_result(&verification);

		if (error || stopped) {
			fprintf(stderr, "FAIL: %s\n", error ? error : "Verification was interrupted.");
			status = EXIT_FAILURE;
		} else if (status == EXIT_SUCCESS) {
			printf("PASS: %lu pairs, zero repeats, >=2-second hold, and 1-second quiet observation.\n",
			       verification.target);
		}
	}
	close(fd);
	return status;
usage:
	fprintf(stderr, "Usage: %s [--verify COUNT] [/dev/input/eventN]\n", argv[0]);
	return EXIT_FAILURE;
}
