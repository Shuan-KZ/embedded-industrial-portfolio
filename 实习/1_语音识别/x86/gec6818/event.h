#ifndef		__EVENT_H__
#define		__EVENT_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>


#define  BIN_TOUCH 0x14a //触摸屏按键

#define	 UP		1
#define	 DOWN	2
#define	 LEFT	3
#define	 RIGHT	4

void get_event_xy(int* x, int* y);
int get_event_direction();




#endif




