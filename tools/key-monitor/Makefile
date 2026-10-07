CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean
all: key-monitor

key-monitor: key-monitor.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 $< -o $@ $(LDFLAGS)

clean:
	rm -f key-monitor
