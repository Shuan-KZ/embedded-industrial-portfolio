#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "common.h"

#define GY39_UART    "/dev/ttySAC1"
#define US100_UART   "/dev/ttySAC1"
#define SR501_DEV    "/dev/infrared_drv"

typedef struct gy39_env_data
{
	int temperature;
	int humidity;
	int pressure;
	int altitude;
} gy39_env_data_t;

int uart_open(const char *devname, speed_t baud);
int gy39_get_light(int fd);
int gy39_get_env(int fd, gy39_env_data_t *env);
int us100_get_distance(int fd);
int sr501_open();
int sr501_get_status(int fd);
void sr501_close(int fd);

#endif
