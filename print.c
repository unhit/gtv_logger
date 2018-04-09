#include <stdio.h>
#include <time.h>
#include <string.h>

#include "print.h"

// logger handle
FILE *logger = NULL;
char str_tstamp[32];

#include "config.h"
// from config.c
extern struct configuration cfg;

void print_to(int dest, const char *fmt, va_list ap) {
    va_list argp;
    va_copy(argp, ap);
    
    switch (dest) {
        case DEST_CONSOLE:
            vfprintf(stdout, fmt, argp);
            break;
        case DEST_LOG:
            if (logger != NULL) {
                vfprintf(logger, fmt, argp);
                
                if (!cfg.buffered_logging) {
            	    fflush(logger);
                }
            }

            break;
        case DEST_ERROR:
            vfprintf(stderr, fmt, argp);
            break;
    }

    va_end(argp);
}

// prints to stdout and log
void print(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    timestamp();
    
    if (cfg.log_destination & LOG_DEST_STDOUT) {
	print_to(DEST_CONSOLE, str_tstamp, NULL);
	print_to(DEST_CONSOLE, fmt, argp);
    }
    
    if (cfg.log_destination & LOG_DEST_FILE) {
	print_to(DEST_LOG, str_tstamp, NULL);
	print_to(DEST_LOG, fmt, argp);
    }
    
    va_end(argp);
}

// prints to stderr and log
void error(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    timestamp();
    
    if (cfg.log_destination & LOG_DEST_STDOUT) {
	print_to(DEST_ERROR, str_tstamp, NULL);
	print_to(DEST_ERROR, "[ERROR] ", NULL);
	print_to(DEST_ERROR, fmt, argp);
    }
    
    if (cfg.log_destination & LOG_DEST_FILE) {
	print_to(DEST_LOG, str_tstamp, NULL);
	print_to(DEST_LOG, "[ERROR] ", NULL);
	print_to(DEST_LOG, fmt, argp);
    }
    
    va_end(argp);
}

// prints to stdout and log
void debug(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
#ifdef DEBUG
    timestamp();
    
    if (cfg.log_destination & LOG_DEST_STDOUT) {
	print_to(DEST_CONSOLE, str_tstamp, NULL);
	print_to(DEST_CONSOLE, "[DEBUG] ", NULL);
	print_to(DEST_CONSOLE, fmt, argp);
    }
    
    if (cfg.log_destination & LOG_DEST_FILE) {
	print_to(DEST_LOG, str_tstamp, NULL);
	print_to(DEST_LOG, "[DEBUG] ", NULL);
	print_to(DEST_LOG, fmt, argp);
    }
#endif
    va_end(argp);
}

// save current timestamp
void timestamp() {
    time_t now;
    struct tm *tm;

    now = time(NULL);
    tm = localtime(&now);
    memset(str_tstamp, '\0', sizeof(str_tstamp));
    sprintf(str_tstamp, "(%d-%02d-%02d %02d:%02d:%02d) ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
}
