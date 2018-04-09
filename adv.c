#include <unistd.h>
#include <pthread.h>

#include "adv.h"
#include "code/server/server.h"

// set by main thread o terminate advertiser
int adv_terminate = 0;
// original function for print, chat, cp, etc..
void (*orig_SV_SendServerCommand)(client_t *, const char *, ...) = (void *)OFFSET_SV_SENDSERVERCOMMAND;

#include "config.h"
// from config.c
extern struct configuration cfg;

// advertiser thread
void * thread_adv(void *id) {
    // we have to wait unless we want segfault ;)
    sleep(10);
    
    while (!adv_terminate) {
	switch (cfg.adv_method) {
	    case ADV_CHAT:
		orig_SV_SendServerCommand(NULL, "chat \"%s\"", cfg.adv_message);
		break;
	    case ADV_PRINT:
		orig_SV_SendServerCommand(NULL, "print \"%s\"", cfg.adv_message);
		break;
	    case ADV_BIGTEXT:
		orig_SV_SendServerCommand(NULL, "cp \"%s\"", cfg.adv_message);
		break;
	}
	
	sleep(cfg.adv_interval);
    }
    
    pthread_exit(NULL);
}
