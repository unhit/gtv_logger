#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "code/server/server.h"
#include "code/qcommon/qcommon.h"
#include "detour.h"

#define	VERSION				"0.6"

#define	MAX_NAME_LEN			32
#define MAX_CMD_LEN			32
#define	MAX_ARG_LEN			256
#define	MAX_USERINFO_LEN		1024
#define	MAX_IP_LEN			21

// offsets of hooked functions, get offsets by running: objdump -x gtv.run
#define OFFSET_SV_USERINFOCHANGED	0x0805ea24
#define	OFFSET_SV_EXECUTECLIENTCOMMAND	0x0805ef10
#define	OFFSET_Q_STRCAT			0x08085340
#define	OFFSET_CMD_ARGV			0x080690fc
#define	OFFSET_SV_DROPCLIENT		0x0805f30c
#define	OFFSET_SERVER_ADDSTRCMD		0x08054b2c

// .rodata offsets
#include "rodata.h"

// .bss offsets -- DEPRECATED
#include "bss.h"
extern char admin_pwd[];
extern char camera_pwd[];

#include "print.h"
// from print.c
extern FILE *logger;
extern char str_tstamp[];

#include "config.h"
// from config.c
extern struct configuration cfg;

#include "adv.h"
// from adv.c
extern int adv_terminate;
extern void (*orig_SV_SendServerCommand)(client_t *, const char *, ...);

// these variables will be accessed very often,
// no need to keep them private
char g_command[MAX_CMD_LEN + 1];
char g_name[MAX_NAME_LEN + 1];
char g_argument[MAX_ARG_LEN + 1];
char g_userinfo[MAX_USERINFO_LEN + 1];
char g_ip[MAX_IP_LEN + 1];
pthread_t adv;

// hooks and corresponding original function pointers
void hook_SV_UserinfoChanged(client_t *);
void (*orig_SV_UserinfoChanged)(client_t *);
void hook_SV_ExecuteClientCommand(client_t *, char *, qboolean);
void (*orig_SV_ExecuteClientCommand)(client_t *, char *, qboolean);
void hook_Q_strcat(char *, int, const char *);
void (*orig_Q_strcat)(char *, int, const char *);
char * hook_Cmd_Argv(int);
char * (*orig_Cmd_Argv)(int);
void hook_SV_DropClient(client_t *, const char *);
void (*orig_SV_DropClient)(client_t *, const char *);
// unknown parameters
//void hook_server_AddStrCmd(void *, void *, void *);
//void (*orig_server_AddStrCmd)(void *, void *, void *);

// helpers
void strip_colors(char *, int);
void strip_quotes(char *, int);
void get_name(const char *);
int get_argument(const char *);
char * get_ip(const char *);
unsigned long get_id(client_t *);
int is_admin(client_t *);

// this is executed just after shared object is loaded
void __attribute__((constructor)) init() {
    print("[INFO] Loading GTV logger\n");
    read_cfg();
    print("[INFO] Intercepting functions\n");
    // teh magic happens here
    orig_SV_UserinfoChanged = (void *)detour((void (*)(client_t *))OFFSET_SV_USERINFOCHANGED, &hook_SV_UserinfoChanged, 6);
    orig_SV_ExecuteClientCommand = (void *)detour((void (*)(client_t *, char *, qboolean))OFFSET_SV_EXECUTECLIENTCOMMAND, &hook_SV_ExecuteClientCommand, 6);
    orig_Q_strcat = (void *)detour((void (*)(char *, int, const char *))OFFSET_Q_STRCAT, &hook_Q_strcat, 6);
    orig_Cmd_Argv = (void *)detour((char * (*)(int))OFFSET_CMD_ARGV, &hook_Cmd_Argv, 6);
    orig_SV_DropClient = (void *)detour((void (*)(client_t *, const char *))OFFSET_SV_DROPCLIENT, &hook_SV_DropClient, 5);
    //orig_server_AddStrCmd = (void *)detour((void (*)(void *, void *, void *))OFFSET_SERVER_ADDSTRCMD, &hook_server_AddStrCmd, 6);
    memset(admin_pwd, '\0', NAME_MAX + 1);
    memset(camera_pwd, '\0', NAME_MAX + 1);
    print("[INFO] Opening log file\n");
    logger = fopen(cfg.log_file_name, "a+");
    
    if (logger == NULL) {
	error("Could not open/create file %s for writing; exitting\n", cfg.log_file_name);
	_exit(-1);
    }
    
    if (cfg.adv_enable) {
	pthread_create(&adv, NULL, thread_adv, NULL);
    }
    
    print("[INFO] --- GTV logger started ---\n");
}

// this is executed just before shared object is unloaded
void __attribute__((destructor)) finish() {
    if (cfg.adv_enable) {
	print("[INFO] Stopping advertiser\n");
	adv_terminate = 1;
    }
    
    print("[INFO] Freeing memory\n");
    free(orig_SV_UserinfoChanged);
    free(orig_SV_ExecuteClientCommand);
    free(orig_Q_strcat);
    free(orig_Cmd_Argv);
    free(orig_SV_DropClient);
    //free(orig_server_AddStrCmd);
    free_cfg();
    print("[INFO] --- GTV logger stopped ---\n");
    fclose(logger);
    logger = NULL;
    print("[INFO] Unloading GTV logger " VERSION " by fruk <tm[at]ols.vectranet.pl> - kthxbai!\n");
}

//////////////
/// HOOKS
//////////////

// handle userinfo string change
void hook_SV_UserinfoChanged(client_t *cl) {
    unsigned char *p = (unsigned char *)cl;
    // cl->userinfo didn't work out, they must have changed that struct.
    // after some research, i found userinfo string at 556 byte.
    char *userinfo = (char *)(p + 556);
    
    // check if ip is present in userinfo,
    // it seems it's there only when client is connecting
    char *ip_ptr = get_ip(userinfo);
    
    memset(g_userinfo, '\0', MAX_USERINFO_LEN + 1);
    strncpy(g_userinfo, userinfo, MAX_USERINFO_LEN);
    
    if (ip_ptr == NULL) {
	print("[USERINFO] (%ld) %s\n", get_id(cl), g_userinfo);
    } else {
	print("[CONNECT] (%ld) %s\n", get_id(cl), g_userinfo);
    }
    
    // call original function
    orig_SV_UserinfoChanged(cl);
}

// handle user commands
void hook_SV_ExecuteClientCommand(client_t *cl, char *s, qboolean clientOK) {
    unsigned char *p = (unsigned char *)cl;
    char *userinfo = (char *)(p + 556);
    unsigned long *admin = (unsigned long *)(p + 0x3c);
    int i, arg_len;

    char *cmd_ptr = s;
    memset(g_command, '\0', MAX_CMD_LEN + 1);
    memset(g_name, '\0', MAX_NAME_LEN + 1);
    memset(g_argument, '\0', MAX_ARG_LEN + 1);
    
    for (i = 0; *cmd_ptr != ' ' && i < MAX_CMD_LEN; cmd_ptr++, i++)
	g_command[i] = *cmd_ptr;
	
    // handle chat messages
    if (strncmp(g_command, "say", MAX_CMD_LEN) == 0) {
	get_name(userinfo);
	arg_len = get_argument(cmd_ptr + 1);
	strip_quotes(g_argument, arg_len);	
	print("[CHAT] (%ld) <%s>: \"%s\"\n", get_id(cl), g_name, g_argument);
	
	// extended admin chat
	if (is_admin(cl) && cfg.adm_enable) {
	    switch (cfg.adm_method) {
		case ADV_CHAT:
		    orig_SV_SendServerCommand(NULL, "chat \"^3(^1ADMIN:^2%s^3) %s\"", g_name, g_argument);
		    break;
		case ADV_PRINT:
		    orig_SV_SendServerCommand(NULL, "print \"^3(^1ADMIN:^2%s^3) %s\"", g_name, g_argument);
		    break;
		case ADV_BIGTEXT:
		    orig_SV_SendServerCommand(NULL, "cp \"^3(^1ADMIN:^2%s^3) %s\"", g_name, g_argument);
		    break;
	    }
	}
    } else if (strncmp(g_command, "gtv_camera", MAX_CMD_LEN) == 0) {
	// log cameramen
	get_name(userinfo);
	arg_len = get_argument(cmd_ptr + 1);
	
	if (strncmp(g_argument, camera_pwd, NAME_MAX) == 0) {
	    print("[CAMERA] (%ld) Login successful: <%s>\n", get_id(cl), g_name);
	} else {
	    print("[CAMERA] (%ld) Login failed: <%s>\n", get_id(cl), g_name);
	}
    } else if (strncmp(g_command, "gtv_ban", MAX_CMD_LEN) == 0 && is_admin(cl)) {
	// log bans
	get_name(userinfo);
	arg_len = get_argument(cmd_ptr + 1);
	print("[BAN] (%ld) <%s>: %s\n", get_id(cl), g_name, g_argument);
    }
    
    // let admins change some values
    if (is_admin(cl)) {
	arg_len = get_argument(cmd_ptr + 1);
	
	// en/disable advertiser
	if (strncmp(g_command, "gtv_adv_enable", MAX_CMD_LEN) == 0) {
	    if (strncmp(g_argument, "1", MAX_ARG_LEN) == 0) {
		if (cfg.adv_enable) {
		    orig_SV_SendServerCommand(cl, "chat \"^1ERROR: ^3Advertiser is already ^2ENABLED\"");
		} else {
		    cfg.adv_enable = 1;
		    adv_terminate = 0;
		    pthread_create(&adv, NULL, thread_adv, NULL);
		    orig_SV_SendServerCommand(cl, "chat \"^3Advertiser is now ^2ENABLED\"");
		}
	    } else if (strncmp(g_argument, "0", MAX_ARG_LEN) == 0) {
		if (cfg.adv_enable) {
		    cfg.adv_enable = 0;
		    adv_terminate = 1;
		    orig_SV_SendServerCommand(cl, "chat \"^3Advertiser is now ^1DISABLED\"");
		} else {
		    orig_SV_SendServerCommand(cl, "chat \"^1ERROR: ^3Advertiser is already ^1DISABLED\"");
		}
	    }
	// change adv method
	} else if (strncmp(g_command, "gtv_adv_method", MAX_CMD_LEN) == 0) {
	    if (strncmp(g_argument, "print", MAX_ARG_LEN) == 0) {
		cfg.adv_method = ADV_PRINT;
		orig_SV_SendServerCommand(cl, "chat \"^3Advertise method is now set to ^2print\"");
	    } else if (strncmp(g_argument, "bigtext", MAX_ARG_LEN) == 0) {
		cfg.adv_method = ADV_BIGTEXT;
		orig_SV_SendServerCommand(cl, "chat \"^3Advertise method is now set to ^2bigtext\"");
	    } else if (strncmp(g_argument, "chat", MAX_ARG_LEN) == 0) {
		cfg.adv_method = ADV_CHAT;
		orig_SV_SendServerCommand(cl, "chat \"^3Advertise method is now set to ^2chat\"");
	    }
	// change adv interval
	} else if (strncmp(g_command, "gtv_adv_interval", MAX_CMD_LEN) == 0) {
	    int interval = atoi(g_argument);
	    
	    if (interval > 0) {
		cfg.adv_interval = interval;
		orig_SV_SendServerCommand(cl, "chat \"^3Advertise interval is now set to ^2%d\"", interval);
	    }
	// change adv message
	} else if (strncmp(g_command, "gtv_adv_message", MAX_CMD_LEN) == 0) {
	    strncpy(cfg.adv_message, g_argument, NAME_MAX);
	    orig_SV_SendServerCommand(cl, "chat \"^3Advertise message is now set to: %s\"", cfg.adv_message);
	// en/disable admin chat
	} else if (strncmp(g_command, "gtv_adm_enable", MAX_CMD_LEN) == 0) {
	    if (strncmp(g_argument, "1", MAX_ARG_LEN) == 0) {
		cfg.adm_enable = 1;
		orig_SV_SendServerCommand(cl, "chat \"^3Extended admin chat is now ^1enabled\"");
	    } else if (strncmp(g_argument, "0", MAX_ARG_LEN) == 0) {
		cfg.adm_enable = 0;
		orig_SV_SendServerCommand(cl, "chat \"^3Extended admin chat is now ^1disabled\"");
	    }
	// change admin chat method
	} else if (strncmp(g_command, "gtv_adm_method", MAX_CMD_LEN) == 0) {
	    if (strncmp(g_argument, "print", MAX_ARG_LEN) == 0) {
		cfg.adm_method = ADV_PRINT;
		orig_SV_SendServerCommand(cl, "chat \"^3Extended admin chat method is now set to ^2print\"");
	    } else if (strncmp(g_argument, "bigtext", MAX_ARG_LEN) == 0) {
		cfg.adm_method = ADV_BIGTEXT;
		orig_SV_SendServerCommand(cl, "chat \"^3Extended admin chat method is now set to ^2bigtext\"");
	    } else if (strncmp(g_argument, "chat", MAX_ARG_LEN) == 0) {
		cfg.adm_method = ADV_CHAT;
		orig_SV_SendServerCommand(cl, "chat \"^3Extended admin chat method is now set to ^2chat\"");
	    }
	}
    }
    
    unsigned long tmp = *admin;
    // call original function
    asm("pushal");
    orig_SV_ExecuteClientCommand(cl, s, clientOK);
    asm("popal");
        
    // user became an admin
    if (strncmp(g_command, "gtv_admin", MAX_CMD_LEN) == 0) {
	// log admins
	get_name(userinfo);
	
	if (is_admin(cl) && tmp == 0) {
	    print("[ADMIN] (%ld) Login successful: <%s>\n", get_id(cl), g_name);
	} else {
	    print("[ADMIN] (%ld) Login failed: <%s>\n", get_id(cl), g_name);
	}
    }    
}

// it's a nice place to hack .rodata
void hook_Q_strcat(char *dest, int size, const char *src) {
    char *arg = (char *)src;
    
    // finding these offsets is easy, but boring as hell... anyone?
    switch ((unsigned long)src) {
	case OFFSET_MSG_WELCOME:
	    arg = cfg.msg_welcome;
	    break;
	case OFFSET_MSG_EXAMPLE:
	    arg = cfg.msg_example;
	    break;
    }
    
    orig_Q_strcat(dest, size, arg);
}

char * hook_Cmd_Argv(int arg) {
    char *p = orig_Cmd_Argv(arg);
    char *val;
    
    // get admin password. it's a tricky one, hopefully portable
    if (admin_pwd[0] == '\0') {
	if (p == (void *)OFFSET_ADMIN_PASS_KEY && strncmp(p, "gtv_adminPass", NAME_MAX) == 0) {
	    asm("pushal");
	    val = orig_Cmd_Argv(2);
	    asm("popal");
	    if (val == (void *)OFFSET_ADMIN_PASS_VAL) {
		strncpy(admin_pwd, val, NAME_MAX);
		debug("Admin password intercepted: %s\n", admin_pwd);
	    }
	}
    }

    // get camera password
    if (camera_pwd[0] == '\0') {
	if (p == (void *)OFFSET_CAMERA_PASS_KEY && strncmp(p, "gtv_cameraPass", NAME_MAX) == 0) {
	    asm("pushal");
	    val = orig_Cmd_Argv(2);
	    asm("popal");
	    if (val == (void *)OFFSET_CAMERA_PASS_VAL) {
		strncpy(camera_pwd, val, NAME_MAX);
		debug("Camera password intercepted: %s\n", camera_pwd);
	    }
	}
    }
    
    return p;
}

// handling client disconnects
void hook_SV_DropClient(client_t *cl, const char *reason) {
    unsigned char *p = (unsigned char *)cl;
    char *userinfo = (char *)(p + 556);
    get_name(userinfo);
    print("[DISCONNECT] (%d) <%s>: %s\n", get_id(cl), g_name, reason);
    orig_SV_DropClient(cl, reason);
}

/* for future use
void hook_server_AddStrCmd(void *arg1, void *arg2, void *arg3) {
    printf("addstrcmd(0x%x) = %s\n", arg3, arg3);
    orig_server_AddStrCmd(arg1, arg2, arg3);
}
*/

//////////////
/// HELPERS
//////////////

// removes "^x" pairs of characters
void strip_colors(char *name, int len) {
    char *ptr;
    int i;
    
    for (i = 0, ptr = name; *ptr && i < len; ) {
	if (*ptr != '^') {
	    name[i++] = *ptr++;
	} else {
	    ptr += 2;
	}
    }
    
    for ( ; i < len; i++) {
	name[i] = '\0';
    }
}

// removes quotes from str
void strip_quotes(char *str, int len) {
    char *ptr;
    int i;
    
    for (i = 0, ptr = str; *ptr && i < len; ) {
	if (*ptr != '\"') {
	    str[i++] = *ptr++;
	} else {
	    ptr++;
	}
    }
    
    for ( ; i < len; i++) {
	str[i] = '\0';
    }
}

// extracts command argument (eg. chat message)
int get_argument(const char *arg) {
    memset(g_argument, '\0', MAX_ARG_LEN);
    strncpy(g_argument, arg, MAX_ARG_LEN);
    
    // remove trailing \n
    int len = strlen(g_argument);
    
    if (g_argument[len - 1] == '\n') {
	g_argument[len - 1] = '\0';
	len--;
    }
    
    return len;
}

// extracts name from userinfo string
void get_name(const char *userinfo) {
    char *name_ptr = strstr(userinfo, "\\name\\");
    int i;
    
    memset(g_name, '\0', MAX_NAME_LEN + 1);
    
    if (name_ptr != NULL) {
	for (i = 0, name_ptr += 6; *name_ptr != '\\' && i < MAX_NAME_LEN; name_ptr++, i++) {
	    g_name[i] = *name_ptr;
	}
	    
	if (cfg.strip_colors) {
	    strip_colors(g_name, i);
	}
    } else {
	// set name as invalid, will think about it later
	strncpy(g_name, "(invalid)", MAX_NAME_LEN);
    }
}

// extracts ip
char * get_ip(const char *userinfo) {
    char *ip_ptr = strstr(userinfo, "\\ip\\");
    int i;
    
    memset(g_ip, '\0', MAX_IP_LEN + 1);
    
    if (ip_ptr != NULL) {
	for (i = 0, ip_ptr += 5; *ip_ptr != '\\' && i < MAX_IP_LEN; ip_ptr++, i++) {
	    g_ip[i] = *ip_ptr;
	}
    }
	
    return ip_ptr;
}

// gets id
unsigned long get_id(client_t *cl) {
    unsigned long *ptr = (unsigned long *)cl;
    return *(ptr + 1);
}

// checks if user is an admin
int is_admin(client_t *cl) {
    unsigned char *ptr = (unsigned char *)cl;
    unsigned long *admin = (unsigned long *)(ptr + 0x3c);
    
    if (*admin == 0x2) {
	return 1;
    } else {
	return 0;
    }
}
