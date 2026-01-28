// logger/log_macros.h
#ifndef LOG_MACROS_H
#define LOG_MACROS_H

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "log_codes.h"
#include "logger_api.h"

// Timestamp: YYYY-MM-DD HH:MM:SS
static inline void _fmt_timestamp(char* buf, size_t sz) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sz, "%Y-%m-%d %H:%M:%S", &tmv);
}

// Build one complete line into a buffer and hand off to logger_write_line.
// Severity suffix: 'I' (info), 'W' (warn), 'E' (error).
static inline void _log_emitv(const char* base_id_str, char severity,
                              const char* help_text,
                              const char* fmt, va_list ap)
{
    char ts[20];
    _fmt_timestamp(ts, sizeof(ts));
    pid_t pid = getpid();
    const char* logger = get_logger_name();

    char line[4096];
    size_t off = 0;

    int n = snprintf(line + off, sizeof(line) - off,
                     "%s[pid=%d][logger=%s]: %s%c",
                     ts, (int)pid, logger, base_id_str, severity);
    if (n < 0) return;
    off += (size_t)n;
    if (off >= sizeof(line)) { off = sizeof(line) - 1; goto write; }

    if (help_text && *help_text) {
        n = snprintf(line + off, sizeof(line) - off, " %s", help_text);
        if (n < 0) return;
        off += (size_t)n;
        if (off >= sizeof(line)) { off = sizeof(line) - 1; goto write; }
    }

    if (fmt && *fmt) {
        n = snprintf(line + off, sizeof(line) - off, " ");
        if (n < 0) return;
        off += (size_t)n;
        if (off >= sizeof(line)) { off = sizeof(line) - 1; goto write; }

        n = vsnprintf(line + off, sizeof(line) - off, fmt, ap);
        if (n < 0) return;
        off += (size_t)n;
        if (off >= sizeof(line)) off = sizeof(line) - 1;
    }

write:
    logger_write_line(line, off);
}

static inline void _log_emitf(const char* base_id_str, char severity,
                              const char* help_text,
                              const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _log_emitv(base_id_str, severity, help_text, fmt, ap);
    va_end(ap);
}

// #code → "IUCVCLNT011"; (code) → help text string
#define LOG_INFO(code, fmt, ...)  do { _log_emitf(#code, 'I', (code), (fmt), ##__VA_ARGS__); } while (0)
#define LOG_WARN(code, fmt, ...)  do { _log_emitf(#code, 'W', (code), (fmt), ##__VA_ARGS__); } while (0)
#define LOG_DEG(code,  fmt, ...)  do { _log_emitf(#code, 'D', (code), (fmt), ##__VA_ARGS__); } while (0)
#define LOG_ERR(code,  fmt, ...)  do { _log_emitf(#code, 'E', (code), (fmt), ##__VA_ARGS__); } while (0)

#endif // LOG_MACROS_H
