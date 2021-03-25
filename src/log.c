/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>

#include "../dist/src/sds/sds.h"
#include "log.h"

int loglevel;
bool log_to_syslog;
bool log_on_tty;

static const char *loglevel_names[] = {
    "EMERG", "ALERT", "CRITICAL", "ERROR", "WARN", "NOTICE", "INFO", "DEBUG"};

static const char *loglevel_colors[] = {
    "\033[0;31m", "\033[0;31m", "\033[0;31m", "\033[0;31m", "\033[0;33m", "", "", "\033[0;34m"};

void set_loglevel(int level)
{
    if (level > 7)
    {
        level = 7;
    }
    else if (level < 0)
    {
        level = 0;
    }
    MYMPD_LOG_NOTICE("Setting loglevel to %s", loglevel_names[level]);
    loglevel = level;
}

void mympd_log(int level, const char *file, int line, const char *fmt, ...)
{
    if (level > loglevel)
    {
        return;
    }

    if (log_to_syslog == true)
    {
        va_list args;
        va_start(args, fmt);
        vsyslog(level, fmt, args);
        va_end(args);
        return;
    }

    sds logline = sdsempty();
    if (log_on_tty == true)
    {
        logline = sdscat(logline, loglevel_colors[level]);
    }

    if (log_on_tty == true)
    {
        time_t now = time(NULL);
        struct tm timeinfo;
        if (localtime_r(&now, &timeinfo) != NULL)
        {
            logline = sdscatprintf(logline, "%02d:%02d:%02d ", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
    }
    logline = sdscatprintf(logline, "%-8s %-10s", loglevel_names[level], thread_logname);
    if (loglevel == 7)
    {
        logline = sdscatprintf(logline, "%s:%d: ", file, line);
    }

    va_list args;
    va_start(args, fmt);
    logline = sdscatvprintf(logline, fmt, args);
    va_end(args);

    if (sdslen(logline) > 1023)
    {
        sdsrange(logline, 0, 1020);
        logline = sdscatlen(logline, "...\n", 4);
    }
    else
    {
        logline = sdscatlen(logline, "\n", 1);
    }
    if (log_on_tty == true)
    {
        logline = sdscat(logline, "\033[0m");
    }

    if (level > 1)
    {
        //info, verbose and debug to stdout
        fputs(logline, stdout);
        fflush(stdout);
    }
    else
    {
        //error and warning to stderr
        fputs(logline, stderr);
        fflush(stderr);
    }
    sdsfree(logline);
}
