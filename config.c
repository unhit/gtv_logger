#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "print.h"
#include "adv.h"

// defaults to be overwritten by cfg file
struct configuration cfg = {
    // pointer to conf. handle
    NULL,
    // log file name
    "gtv_logger.log",
    // buffered logging
    0,
    // log destination
    LOG_DEST_STDOUT | LOG_DEST_FILE,
    // strip colors
    0,
    // msg_welcome
    "^5Welcome to UrT GTV\n^1Visit ^2yourwebsite.com",
    // msg_example
    "\n^2Type ^1/gtv_watch 2^2 to start the stream",
    // adv enable
    0,
    // adv method
    ADV_CHAT,
    // adv interval
    60,
    // adv message
    "^3(^2ADV^3) ^4Your wonderful advert",
    // adm enable
    0,
    // adm method
    ADV_CHAT
};

enum lcfg_status iterator(const char *key, void *data, size_t len, void *user_data) {
    char *ptr;
    ptr = data;
    
    if (strcmp("log_file_name", key) == 0) {
	strncpy(cfg.log_file_name, ptr, NAME_MAX);
	print("[CONFIG] Log file name: %s\n", cfg.log_file_name);
    } else if (strcmp("log_destination", key) == 0) {
	int choice = atoi(ptr);
	
	switch (choice) {
	    case LOG_DEST_STDOUT:
		cfg.log_destination = LOG_DEST_STDOUT;
		print("[CONFIG] Log destination: stdout\n");
		break;
	    case LOG_DEST_FILE:
		cfg.log_destination = LOG_DEST_FILE;
		print("[CONFIG] Log destination: file\n");
		break;
	    default:
		cfg.log_destination = LOG_DEST_STDOUT | LOG_DEST_FILE;
		print("[CONFIG] Log destination: stdout & file\n");
		break;
	}
    } else if (strcmp("buffered_logging", key) == 0) {
	cfg.buffered_logging = strcmp("yes", ptr) ? 0 : 1;
	print("[CONFIG] Buffered logging: %s\n", cfg.buffered_logging ? "enabled" : "disabled");
    } else if (strcmp("strip_colors", key) == 0) {
	cfg.strip_colors = strcmp("yes", ptr) ? 0 : 1;
	print("[CONFIG] Strip colors: %s\n", cfg.strip_colors ? "enabled" : "disabled");
    } else if (strcmp("msg_welcome", key) == 0) {
	strncpy(cfg.msg_welcome, ptr, NAME_MAX);
    } else if (strcmp("msg_example", key) == 0) {
	strncpy(cfg.msg_example, ptr, NAME_MAX);
    } else if (strcmp("adv_enable", key) == 0) {
	cfg.adv_enable = strcmp("yes", ptr) == 0 ? 1 : 0;
	print("[CONFIG] Advertising: %s\n", cfg.adv_enable ? "enabled" : "disabled");
    } else if (strcmp("adv_method", key) == 0) {
	if (strcmp("print", ptr) == 0) {
	    cfg.adv_method = ADV_PRINT;
	} else if (strcmp("chat", ptr) == 0) {
	    cfg.adv_method = ADV_CHAT;
	} else if (strcmp("bigtext", ptr) == 0) {
	    cfg.adv_method = ADV_BIGTEXT;
	}
    } else if (strcmp("adv_interval", key) == 0) {
	cfg.adv_interval = atoi(ptr);
	print("[CONFIG] Advertising interval: %d seconds\n", cfg.adv_interval);
    } else if (strcmp("adv_message", key) == 0) {
	strncpy(cfg.adv_message, ptr, NAME_MAX);
    } else if (strcmp("adm_enable", key) == 0) {
	cfg.adm_enable = strcmp("yes", ptr) == 0 ? 1 : 0;
	print("[CONFIG] Extended admin chat: %s\n", cfg.adm_enable ? "enabled" : "disabled");
    } else if (strcmp("adm_method", key) == 0) {
	if (strcmp("print", ptr) == 0) {
	    cfg.adm_method = ADV_PRINT;
	} else if (strcmp("chat", ptr) == 0) {
	    cfg.adm_method = ADV_CHAT;
	} else if (strcmp("bigtext", ptr) == 0) {
	    cfg.adm_method = ADV_BIGTEXT;
	}
    } else {
	error("Invalid configuration option: %s\n", key);
    }
    
    return lcfg_status_ok;
}

void read_cfg(void) {
    cfg.c = lcfg_new(CONFIG_FILENAME);
    
    if (lcfg_parse(cfg.c) != lcfg_status_ok) {
	error("%s\n", lcfg_error_get(cfg.c));
	_exit(-1);
    }
    
    lcfg_accept(cfg.c, iterator, NULL);
}

void free_cfg(void) {
    lcfg_delete(cfg.c);
}
