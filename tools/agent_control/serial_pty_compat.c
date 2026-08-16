#define _GNU_SOURCE

#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * Linux PTYs intentionally do not implement modem-control lines. The DOMES
 * serial client deasserts RTS and DTR after every open to avoid resetting a
 * physical ESP32-S3. Inside the evidence sandbox the client sees only a PTY,
 * so acknowledge those two harmless operations while forwarding every other
 * ioctl unchanged to the kernel.
 */
int ioctl(int descriptor, unsigned long request, ...) {
    if (request == TIOCMBIC || request == TIOCMBIS) {
        return 0;
    }

    unsigned long argument = 0;
    if (request != TIOCEXCL && request != TIOCNXCL) {
        va_list arguments;
        va_start(arguments, request);
        argument = va_arg(arguments, unsigned long);
        va_end(arguments);
    }
    return (int)syscall(SYS_ioctl, descriptor, request, argument);
}
