#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include "pl_util.h"

#define PENDING_ERROR_SIZE 4096

static char pending_error[PENDING_ERROR_SIZE];
static int pending_error_set = 0;

void pl_set_pending_error(const char* fmt, ...)
{
    if (pending_error_set) {
        return; /* keep the first error, it is the most informative one */
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(pending_error, PENDING_ERROR_SIZE, fmt, ap);
    va_end(ap);
    pending_error_set = 1;
}

int pl_has_pending_error(void)
{
    return pending_error_set;
}

const char* pl_get_pending_error(void)
{
    return pending_error;
}

void pl_clear_pending_error(void)
{
    pending_error_set = 0;
    pending_error[0] = '\0';
}

#define FILE_MEMORY_STATUS "/proc/self/statm"

double now_us(void)
{
    struct timeval tv;
    double now = 0.0;
    int rc = gettimeofday(&tv, 0);
    if (rc == 0) {
        now = 1000000.0 * tv.tv_sec + tv.tv_usec;
    }
    return now;
}

long total_memory_pages(void)
{
    long pages = 0;

    /*
     * /proc/[pid]/statm
     *        Provides information about memory usage, measured in pages.
     *            size       total program size
     *                       (same as VmSize in /proc/[pid]/status)
     *            resident   resident set size
     *                       (same as VmRSS in /proc/[pid]/status)
     *            share      shared pages (from shared mappings)
     *            text       text (code)
     *            lib        library (unused in Linux 2.6)
     *            data       data + stack
     *            dirty      dirty pages (unused in Linux 2.6)
     */
    FILE* fp = 0;
    do {
        long size, resident, share, text, lib, data, pdirty;
        int nread;
        fp = fopen(FILE_MEMORY_STATUS, "r");
        if (!fp) {
            /* silently ignore, some OSs do not have this file */
            break;
        }
        nread = fscanf(fp, "%ld %ld %ld %ld %ld %ld %ld",
                       &size, &resident, &share, &text, &lib, &data, &pdirty);
        if (nread != 7) {
            /* silently ignore, avoid noisy errors */
            break;
        }
        pages = size;
    } while (0);
    if (fp) {
        fclose(fp);
        fp = 0;
    }
    return pages;
}
