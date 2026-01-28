// logger/logger.c
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>

#include "logger_api.h"

// --- State ---
static char  g_logger_name[64] = "default";
static char  g_log_path[512]   = {0};          // e.g., "/var/log/vmarc.log"
static FILE* g_log_fp          = NULL;         // current log stream
static size_t g_rotate_bytes   = 1024 * 1024;  // 1 MB default
static int   g_console_mirror  = 0;            // mirror to stderr
static int   g_keep_files      = 5;            // keep last N rotated files (0 = unlimited)
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Helpers ---
static int _open_log_if_needed_unlocked(void) {
    if (g_log_fp) return 0;
    if (!g_log_path[0]) return -1; // no path set
    g_log_fp = fopen(g_log_path, "a");
    if (!g_log_fp) return -1;
    // Line-buffering (flush on newline)
    setvbuf(g_log_fp, NULL, _IOLBF, 0);
    return 0;
}

static off_t _current_size_unlocked(void) {
    if (!g_log_fp) return 0;
    struct stat st;
    int fd = fileno(g_log_fp);
    if (fd < 0) return 0;
    if (fstat(fd, &st) != 0) return 0;
    return st.st_size;
}

// Build "<base>.N.log" from base path "/var/log/vmarc.log"
static int _make_numbered(const char* base, int n, char* out, size_t outsz) {
    if (!base || !*base || n < 1 || !out || outsz < 8) return -1;
    const char* suffix = ".log";
    size_t base_len = strlen(base);
    // Expect base to end with ".log"
    if (base_len < 4 || strcmp(base + base_len - 4, suffix) != 0) {
        // If not, we still append .N.log
        return snprintf(out, outsz, "%s.%d.log", base, n) > 0 ? 0 : -1;
    }
    // Strip ".log" and append ".N.log"
    char stem[512];
    snprintf(stem, sizeof(stem), "%.*s", (int)(base_len - 4), base);
    return snprintf(out, outsz, "%s.%d.log", stem, n) > 0 ? 0 : -1;
}

// Numeric rotation: shift existing files up, current becomes .1
static void _numeric_rotate_unlocked(void) {
    if (!g_log_path[0]) return;

    // Close current file first
    if (g_log_fp) {
        fflush(g_log_fp);
        fclose(g_log_fp);
        g_log_fp = NULL;
    }

    // If we cap the number of files, delete the oldest first (e.g., .5)
    if (g_keep_files > 0) {
        char oldest[640];
        if (_make_numbered(g_log_path, g_keep_files, oldest, sizeof(oldest)) == 0) {
            (void)unlink(oldest); // ignore errors
        }
    }

    // Shift down: .(N-1) -> .N, starting from high to low to avoid overwrite
    int max_shift = (g_keep_files > 0) ? g_keep_files - 1 : 100; // arbitrary upper bound if unlimited
    for (int i = max_shift; i >= 1; --i) {
        char src[640], dst[640];
        if (_make_numbered(g_log_path, i, src, sizeof(src)) != 0) continue;
        if (_make_numbered(g_log_path, i + 1, dst, sizeof(dst)) != 0) continue;
        // Rename src -> dst if src exists
        if (access(src, F_OK) == 0) {
            (void)rename(src, dst); // ignore errors; best-effort
        }
    }

    // Finally, rename base -> .1
    char first[640];
    if (_make_numbered(g_log_path, 1, first, sizeof(first)) == 0) {
        (void)rename(g_log_path, first); // ignore errors
    }

    // Reopen fresh base file
    (void)_open_log_if_needed_unlocked();
}

// Rotate if next write would exceed threshold
static void _rotate_if_needed_unlocked(size_t upcoming_bytes) {
    if (!_open_log_if_needed_unlocked()) {
        off_t cur = _current_size_unlocked();
        if (g_rotate_bytes > 0 && (cur + (off_t)upcoming_bytes) > (off_t)g_rotate_bytes) {
            _numeric_rotate_unlocked();
        }
    }
}

// --- Public API ---
void set_logger_name(const char* name) {
    if (!name || !*name) return;
    pthread_mutex_lock(&g_log_mutex);
    snprintf(g_logger_name, sizeof(g_logger_name), "%s", name);
    pthread_mutex_unlock(&g_log_mutex);
}

const char* get_logger_name(void) {
    return g_logger_name;
}

int set_log_file(const char* path) {
    if (!path || !*path) return -1;
    pthread_mutex_lock(&g_log_mutex);

    // Close previous stream if any
    if (g_log_fp) {
        fflush(g_log_fp);
        fclose(g_log_fp);
        g_log_fp = NULL;
    }

    snprintf(g_log_path, sizeof(g_log_path), "%s", path);

    // Ensure parent directory exists? (Optional: create)
    // For /var/log, we assume it exists and permissions are correct.

    int rc = _open_log_if_needed_unlocked();

    pthread_mutex_unlock(&g_log_mutex);
    return rc;
}

void set_log_rotate_bytes(size_t bytes) {
    pthread_mutex_lock(&g_log_mutex);
    g_rotate_bytes = bytes;
    pthread_mutex_unlock(&g_log_mutex);
}

void set_log_keep_files(int max_files) {
    pthread_mutex_lock(&g_log_mutex);
    g_keep_files = (max_files < 0) ? 0 : max_files; // 0 = unlimited
    pthread_mutex_unlock(&g_log_mutex);
}

void logger_enable_console_mirror(int enable) {
    pthread_mutex_lock(&g_log_mutex);
    g_console_mirror = enable ? 1 : 0;
    pthread_mutex_unlock(&g_log_mutex);
}

void logger_write_line(const char* line, size_t len) {
    if (!line) return;
    pthread_mutex_lock(&g_log_mutex);

    // If we have a file path, rotate if needed and write to file
    if (!_open_log_if_needed_unlocked()) {
        _rotate_if_needed_unlocked(len);
        if (g_log_fp) {
            (void)fwrite(line, 1, len, g_log_fp);
            if (len == 0 || line[len - 1] != '\n') {
                fputc('\n', g_log_fp);
            }
        }
    }

    // Mirror to stderr if requested, or if file isn't openable
    if (g_console_mirror || !g_log_fp) {
        (void)fwrite(line, 1, len, stderr);
        if (len == 0 || line[len - 1] != '\n') {
            fputc('\n', stderr);
        }
    }

    pthread_mutex_unlock(&g_log_mutex);
}

void logger_close(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_fp) {
        fflush(g_log_fp);
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
    pthread_mutex_unlock(&g_log_mutex);
}
