// this file is for operating within bss segment
#ifndef _BSS_H_
#define _BSS_H_

#include <limits.h>

#define	OFFSET_ADMIN_PASS_KEY	0x0814ca44
#define	OFFSET_ADMIN_PASS_VAL	0x0814ca52
#define	OFFSET_CAMERA_PASS_KEY	0x0814ca44
#define	OFFSET_CAMERA_PASS_VAL	0x0814ca53

char admin_pwd[NAME_MAX + 1];
char camera_pwd[NAME_MAX + 1];

#endif
