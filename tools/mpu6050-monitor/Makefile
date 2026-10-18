CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean
all: mpu6050-monitor

mpu6050-monitor: mpu6050-monitor.c ../../include/car_mpu6050_uapi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 $< -o $@ $(LDFLAGS) -lm

clean:
	rm -f mpu6050-monitor
