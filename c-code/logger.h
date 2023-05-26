#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdio.h>
#include <syslog.h>

/**
 * If the library client wants to use the syslog facility,
 * just #define SYSLOG before including this header file
 * The support is always compiled into the library
 */
//#define SYSLOG /* using syslog */

#ifdef SYSLOG

void openlogger();
void closelogger();

int get_global_mask();
void set_global_mask(char* mask);

void slog0(int, const char*);
void slog(int mask, const char* fmt, const char* file, int line, const char* format, ...);
#define SLOG0(mask, format) slog0(mask, format)
#define SLOG(mask, format, ...) slog (mask, "[%s (%d)]", __FILE__, __LINE__, format, __VA_ARGS__)

void log_this0(const char*, const char*, const char*, int);
void log_this(const char*, const char*, int, const char*, ...);
#define LOG0(msg) log_this0("[%s (%d)]", msg, __FILE__, __LINE__)
#define LOG(format, ...) log_this("[%s (%d)] ", __FILE__, __LINE__, format, __VA_ARGS__)

#else /* not using syslog */

#define openlogger()
#define closelogger()
#define set_global_mask(mask)
#define LOG0(msg) printf("[%s (%d)] " msg "\n", __FILE__, __LINE__)
#define LOG(msg, ...) printf("[%s (%d)] " msg "\n", __FILE__, __LINE__, __VA_ARGS__)
#define SLOG0(mask, msg) printf("[%s (%d)] " msg "\n", __FILE__, __LINE__)
#define SLOG(mask, msg, ...) printf("[%s (%d)] " msg "\n", __FILE__, __LINE__, __VA_ARGS__)
#endif

#endif
