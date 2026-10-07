CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic

.PHONY: all check clean
all: key-monitor

key-monitor: key-monitor.c key-verify.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 $< -o $@ $(LDFLAGS)

test-key-verify: test-key-verify.c key-verify.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 $< -o $@ $(LDFLAGS)

check: test-key-verify
	./test-key-verify

clean:
	rm -f key-monitor test-key-verify
