/* SPDX-License-Identifier: GPL-2.0 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../../include/car_mpu6050_uapi.h"

_Static_assert(sizeof(struct car_mpu6050_sample) == 24, "ABI size");
_Static_assert(offsetof(struct car_mpu6050_sample, accel_x) == 8, "ABI accel");
_Static_assert(offsetof(struct car_mpu6050_sample, reserved) == 22, "ABI padding");

static volatile sig_atomic_t stopping;
static void stop(int signo) { (void)signo; stopping = 1; }

static uint64_t monotonic_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int sleep_until(uint64_t deadline)
{
    struct timespec ts = { .tv_sec = (time_t)(deadline / 1000000000ULL),
                           .tv_nsec = (long)(deadline % 1000000000ULL) };
    int error;
    while ((error = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL)) == EINTR)
        if (stopping) return 0;
    return error;
}

static double number(const char *text, double low, double high)
{
    char *end;
    errno = 0;
    double value = strtod(text, &end);
    if (errno || end == text || *end || !isfinite(value) || value < low || value > high) {
        fprintf(stderr, "Invalid number: %s (range %.2f..%.2f)\n", text, low, high);
        exit(EXIT_FAILURE);
    }
    return value;
}

static void usage(const char *program)
{
    printf("Usage: %s [-d device] [-s seconds] [-r hz] [-o new.csv] [-q]\n"
           "Default: /dev/car_mpu6050, 10 seconds, target 20 Hz.\n"
           "Display once/second; CSV saves every sample. -q: summary only.\n"
           "Sensor internal rate is 100 Hz. Host read rate is not real-time.\n", program);
}

int main(int argc, char **argv)
{
    const char *device = "/dev/car_mpu6050", *csv_path = NULL;
    double seconds = 10, hz = 20;
    int option, quiet = 0, result = EXIT_SUCCESS;
    FILE *csv = NULL;
    while ((option = getopt(argc, argv, "d:s:r:o:qh")) != -1) {
        switch (option) {
        case 'd': device = optarg; break;
        case 's': seconds = number(optarg, 0.01, 3600); break;
        case 'r': hz = number(optarg, 1, 100); break;
        case 'o': csv_path = optarg; break;
        case 'q': quiet = 1; break;
        case 'h': usage(argv[0]); return EXIT_SUCCESS;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }
    if (optind != argc) { usage(argv[0]); return EXIT_FAILURE; }
    struct sigaction action = { .sa_handler = stop };
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) {
        perror("sigaction"); return EXIT_FAILURE;
    }
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { perror(device); return EXIT_FAILURE; }
    if (csv_path) {
        csv = fopen(csv_path, "wx"); /* Never silently replace a previous run. */
        if (!csv) { perror(csv_path); close(fd); return EXIT_FAILURE; }
        fprintf(csv, "timestamp_ns,ax_raw,ay_raw,az_raw,temp_raw,gx_raw,gy_raw,gz_raw,"
                     "ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,chip_temp_c,accel_norm_g\n");
    }
    uint64_t period = (uint64_t)(1000000000.0 / hz);
    uint64_t start = monotonic_ns(), end = start + (uint64_t)(seconds * 1e9);
    uint64_t next = start, display = start, first = 0, previous = 0;
    uint64_t minimum = UINT64_MAX, maximum = 0, samples = 0, errors = 0;
    if (!quiet) printf("Target %.1f Hz for %.2fs; ranges +/-2g, +/-250 deg/s.\n", hz, seconds);
    while (!stopping && monotonic_ns() < end) {
        struct car_mpu6050_sample sample;
        ssize_t size = read(fd, &sample, sizeof(sample));
        if (size < 0 && errno == EINTR) {
            if (stopping) break;
            continue;
        }
        if (size != sizeof(sample)) {
            if (size < 0) perror("read sample");
            else fprintf(stderr, "Incomplete sample: %zd/24 bytes\n", size);
            errors++; result = EXIT_FAILURE; break;
        }
        uint64_t timestamp = sample.timestamp_ns;
        if (!timestamp || sample.reserved || (samples && timestamp <= previous)) {
            fprintf(stderr, "Invalid ABI sample or non-increasing host timestamp\n");
            errors++; result = EXIT_FAILURE; break;
        }
        if (samples) {
            uint64_t interval = timestamp - previous;
            if (interval < minimum) minimum = interval;
            if (interval > maximum) maximum = interval;
        } else first = timestamp;
        previous = timestamp;
        samples++;
        double ax = sample.accel_x / 16384.0, ay = sample.accel_y / 16384.0;
        double az = sample.accel_z / 16384.0, gx = sample.gyro_x / 131.0;
        double gy = sample.gyro_y / 131.0, gz = sample.gyro_z / 131.0;
        double temperature = sample.temperature / 340.0 + 36.53;
        double norm = sqrt(ax * ax + ay * ay + az * az);
        if (csv && fprintf(csv, "%" PRIu64 ",%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,"
                               "%.6f,%.6f,%.6f,%.3f,%.6f\n",
                           timestamp, sample.accel_x, sample.accel_y, sample.accel_z,
                           sample.temperature, sample.gyro_x, sample.gyro_y, sample.gyro_z,
                           ax, ay, az, gx, gy, gz, temperature, norm) < 0) {
            perror("write CSV"); errors++; result = EXIT_FAILURE; break;
        }
        uint64_t now = monotonic_ns();
        if (now >= display) {
            if (!quiet) {
                printf("t=%6.2fs A[g]=%+.3f %+.3f %+.3f |A|=%.3f "
                       "G[deg/s]=%+.2f %+.2f %+.2f chip=%.2fC\n",
                       (now - start) / 1e9, ax, ay, az, norm, gx, gy, gz, temperature);
                fflush(stdout);
            }
            if (csv && fflush(csv)) {
                perror("flush CSV"); errors++; result = EXIT_FAILURE; break;
            }
            display = now + 1000000000ULL;
        }
        next += period;
        /* Skip missed deadlines instead of a burst of catch-up reads. */
        if (next <= now) next = now + period;
        int error = sleep_until(next < end ? next : end);
        if (error) {
            fprintf(stderr, "clock_nanosleep: %s\n", strerror(error));
            errors++; result = EXIT_FAILURE; break;
        }
    }
    if (csv && fclose(csv)) { perror("close CSV"); errors++; result = EXIT_FAILURE; }
    if (close(fd)) { perror("close device"); errors++; result = EXIT_FAILURE; }
    if (!samples) result = EXIT_FAILURE;
    fprintf(stderr, "Samples=%" PRIu64 " errors=%" PRIu64 " elapsed=%.3fs target_hz=%.2f\n",
            samples, errors, (monotonic_ns() - start) / 1e9, hz);
    if (samples > 1) {
        double mean_ms = (previous - first) / (1e6 * (samples - 1));
        fprintf(stderr, "Host intervals: min=%.3f mean=%.3f max=%.3f ms actual_hz=%.3f\n",
                minimum / 1e6, mean_ms, maximum / 1e6, 1000.0 / mean_ms);
    }
    if (csv_path) fprintf(stderr, "CSV: %s\n", csv_path);
    return stopping && result == EXIT_SUCCESS ? 130 : result;
}
