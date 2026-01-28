// logger/logger_api.h
#ifndef LOGGER_API_H
#define LOGGER_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Logger human-readable name (printed each line).
void set_logger_name(const char* name);
const char* get_logger_name(void);

// Log destination: set the file path (e.g., "/var/log/vmrac.log").
// Opens the file in append mode; creates if missing.
int  set_log_file(const char* path);

// Rotation threshold in bytes (default: 1 MB).
void set_log_rotate_bytes(size_t bytes);

// Max number of rotated files to keep (0 = unlimited). Default: 5.
// Rotation produces: base.log -> base.1.log, shifts existing up: .1->.2, ..., deletes oldest if cap exceeded.
void set_log_keep_files(int max_files);

// Enable/disable mirroring to stderr (default: disabled).
void logger_enable_console_mirror(int enable);

// Close the log file (flush & close; optional).
void logger_close(void);

// Internal writer used by macros; builds rotation-safe writes.
void logger_write_line(const char* line, size_t len);

#ifdef __cplusplus
}
#endif

#endif // LOGGER_API_H
