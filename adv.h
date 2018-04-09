#ifndef _ADV_H_
#define _ADV_H_

#include <stdarg.h>
#include "code/server/server.h"

#define OFFSET_SV_SENDSERVERCOMMAND     0x0805f8f4

#define	ADV_PRINT			0x0
#define	ADV_CHAT			0x1
#define	ADV_BIGTEXT			0x2

void * thread_adv(void *);
void (*orig_SV_SendServerCommand)(client_t *, const char *, ...);

#endif
