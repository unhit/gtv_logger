#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <limits.h>
#include "lcfg_static.h"

#define	CONFIG_FILENAME			"gtv_logger.cfg"

#define	LOG_DEST_STDOUT			0x1
#define	LOG_DEST_FILE			0x2

void read_cfg(void);
void free_cfg(void);

struct configuration {
    struct lcfg *c;
    char log_file_name[NAME_MAX + 1];
    int buffered_logging;
    int log_destination;
    int strip_colors;
    char msg_welcome[NAME_MAX + 1];
    char msg_example[NAME_MAX + 1];
    int adv_enable;
    int adv_method;
    int adv_interval;
    char adv_message[NAME_MAX + 1];
    int adm_enable;
    int adm_method;
};

#endif
