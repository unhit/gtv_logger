#ifndef _PRINT_H_
#define _PRINT_H_

#include <stdarg.h>

void timestamp();
void print_to(int, const char *, va_list);
void print(const char *, ...);
void error(const char *, ...);
void debug(const char *, ...);

#define DEST_CONSOLE                    0x01
#define DEST_LOG                        0x02
#define DEST_ERROR                      0x03

#endif
