#ifndef _SCULL_P_H_
#define _SCULL_P_H_

#include <linux/cdev.h>

#define SCULLP_BUFFER_SIZE 4000


int scull_p_init(dev_t firstdev);
void scull_p_cleanup(void);

#endif