/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef CAR_MPU6050_UAPI_H
#define CAR_MPU6050_UAPI_H
#include <linux/types.h>

/* Native-endian local ABI, 24 bytes on 32/64-bit Linux. One read(count >= 24)
 * returns one complete sample, no EOF. timestamp_ns is host CLOCK_MONOTONIC
 * at I2C read completion, not sensor time. Fixed ranges: +/-2g (16384 LSB/g),
 * +/-250 deg/s (131 LSB/(deg/s)). Short reads: EINVAL. After unbind: ENODEV.
 */
struct car_mpu6050_sample {
	__aligned_u64 timestamp_ns;
	__s16 accel_x;
	__s16 accel_y;
	__s16 accel_z;
	__s16 temperature;
	__s16 gyro_x;
	__s16 gyro_y;
	__s16 gyro_z;
	__u16 reserved; /* Always zero. */
};
#endif
