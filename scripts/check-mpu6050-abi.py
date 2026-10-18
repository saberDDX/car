#!/usr/bin/env python3
"""Board-only read ABI/concurrency and optional open-fd unbind/rebind test."""
import argparse
import concurrent.futures
import ctypes
import errno
import os
from pathlib import Path
import struct

SAMPLE = struct.Struct("=QhhhhhhhH")
DEVICE = "/dev/car_mpu6050"
DRIVER = Path("/sys/bus/i2c/drivers/car-mpu6050")


def require_error(operation, expected):
    try:
        operation()
    except OSError as error:
        if error.errno != expected:
            raise AssertionError(f"expected errno {expected}, got {error}") from error
    else:
        raise AssertionError(f"operation did not fail with errno {expected}")


def sample(fd, size=SAMPLE.size):
    raw = os.read(fd, size)
    if len(raw) != SAMPLE.size:
        raise AssertionError(f"sample length {len(raw)}, expected {SAMPLE.size}")
    values = SAMPLE.unpack(raw)
    if not values[0] or values[-1]:
        raise AssertionError("invalid host timestamp or reserved field")
    return values


def reader():
    fd = os.open(DEVICE, os.O_RDONLY | os.O_CLOEXEC)
    try:
        previous = 0
        for _ in range(20):
            current = sample(fd)[0]
            if current <= previous:
                raise AssertionError("non-increasing host timestamps")
            previous = current
    finally:
        os.close(fd)


def check_unbind(client, run_id):
    if Path("/sys/module/car_mpu6050/parameters/run_id").read_text().strip() != run_id:
        raise RuntimeError("module ownership changed; refusing to unbind")
    path = Path("/sys/bus/i2c/devices") / client
    if path.parent != Path("/sys/bus/i2c/devices") or path.name != client:
        raise ValueError("invalid I2C client name")
    if path.joinpath("driver").resolve() != DRIVER:
        raise RuntimeError("unexpected client driver")
    fd = os.open(DEVICE, os.O_RDONLY | os.O_CLOEXEC)
    detached = False
    try:
        sample(fd)
        (DRIVER / "unbind").write_text(client)
        detached = True
        require_error(lambda: os.read(fd, SAMPLE.size), errno.ENODEV)
        if Path(DEVICE).exists():
            raise AssertionError("misc node remains after unbind")
        (DRIVER / "bind").write_text(client)
        detached = False
        # A stale descriptor must not start controlling the newly-bound object.
        require_error(lambda: os.read(fd, SAMPLE.size), errno.ENODEV)
        new_fd = os.open(DEVICE, os.O_RDONLY | os.O_CLOEXEC)
        try:
            sample(new_fd)
        finally:
            os.close(new_fd)
    finally:
        os.close(fd)
        if detached:
            (DRIVER / "bind").write_text(client)
    print("PASS: old fd returns ENODEV across unbind/rebind; new fd reads normally.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client")
    parser.add_argument("--run-id")
    args = parser.parse_args()
    if bool(args.client) != bool(args.run_id):
        parser.error("--client and --run-id must be used together")
    fd = os.open(DEVICE, os.O_RDONLY | os.O_CLOEXEC)
    try:
        if os.read(fd, 0) != b"":
            raise AssertionError("zero-sized read failed")
        require_error(lambda: os.read(fd, SAMPLE.size - 1), errno.EINVAL)
        require_error(lambda: os.lseek(fd, 0, os.SEEK_SET), errno.ESPIPE)
        sample(fd, 4096)  # A larger request still returns exactly one sample.
        libc = ctypes.CDLL(None, use_errno=True)
        libc.read.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t]
        libc.read.restype = ctypes.c_ssize_t
        ctypes.set_errno(0)
        if libc.read(fd, None, SAMPLE.size) != -1 or ctypes.get_errno() != errno.EFAULT:
            raise AssertionError("invalid userspace pointer did not return EFAULT")
    finally:
        os.close(fd)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        futures = [pool.submit(reader) for _ in range(2)]
        for future in futures:
            future.result()
    print("PASS: 24-byte ABI, short/zero/large reads, EFAULT, seek rejection, two readers.")
    if args.client:
        check_unbind(args.client, args.run_id)


if __name__ == "__main__":
    main()
