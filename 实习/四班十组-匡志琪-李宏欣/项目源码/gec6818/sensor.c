#include "sensor.h"

static int uart_config(int fd, speed_t baud)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) == -1)
	{
		perror("tcgetattr");
		return -1;
	}

	cfmakeraw(&tty);
	tty.c_cflag |= CLOCAL | CREAD;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 10;

	cfsetispeed(&tty, baud);
	cfsetospeed(&tty, baud);

	tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, &tty) == -1)
	{
		perror("tcsetattr");
		return -1;
	}

	return 0;
}

int uart_open(const char *devname, speed_t baud)
{
	int fd = open(devname, O_RDWR | O_NOCTTY);
	if (fd == -1)
	{
		perror(devname);
		return -1;
	}

	if (uart_config(fd, baud) == -1)
	{
		close(fd);
		return -1;
	}

	return fd;
}

static int read_exact(int fd, unsigned char *buf, int size)
{
	int total = 0;
	int ret;

	while (total < size)
	{
		ret = read(fd, buf + total, size - total);
		if (ret > 0)
		{
			total += ret;
			continue;
		}

		if (ret == 0)
		{
			return -1;
		}

		if (errno == EINTR)
		{
			continue;
		}

		perror("read");
		return -1;
	}

	return 0;
}

int us100_get_distance(int fd)
{
	unsigned char cmd = 0x55;
	unsigned char ack[2];

	if (write(fd, &cmd, 1) != 1)
	{
		perror("us100 write");
		return -1;
	}

	if (read_exact(fd, ack, sizeof(ack)) == -1)
	{
		fprintf(stderr, "us100 read timeout\n");
		return -1;
	}

	return ack[0] * 256 + ack[1];
}

int gy39_get_light(int fd)
{
	static unsigned char cmd[] = {0xA5, 0x51, 0xF6};
	unsigned char frame[9];
	unsigned char ch;
	int i = 0;

	if (write(fd, cmd, sizeof(cmd)) != (int)sizeof(cmd))
	{
		perror("gy39 light write");
		return -1;
	}

	while (i < 9)
	{
		if (read(fd, &ch, 1) != 1)
		{
			fprintf(stderr, "gy39 light read timeout\n");
			return -1;
		}

		if (i == 0 && ch != 0x5A)
		{
			continue;
		}

		frame[i++] = ch;
	}

	return ((int)frame[4] << 24 | (int)frame[5] << 16 | (int)frame[6] << 8 | frame[7]) / 100;
}

int gy39_get_env(int fd, gy39_env_data_t *env)
{
	static unsigned char cmd[] = {0xA5, 0x52, 0xF7};
	unsigned char frame[15];
	unsigned char ch;
	int i = 0;

	if (env == NULL)
	{
		return -1;
	}

	if (write(fd, cmd, sizeof(cmd)) != (int)sizeof(cmd))
	{
		perror("gy39 env write");
		return -1;
	}

	while (i < 15)
	{
		if (read(fd, &ch, 1) != 1)
		{
			fprintf(stderr, "gy39 env read timeout\n");
			return -1;
		}

		if (i == 0 && ch != 0x5A)
		{
			continue;
		}

		frame[i++] = ch;
	}

	env->temperature = ((int)frame[4] << 8 | frame[5]) / 100;
	env->humidity = ((int)frame[6] << 8 | frame[7]) / 100;
	env->pressure = ((int)frame[8] << 24 | (int)frame[9] << 16 | (int)frame[10] << 8 | frame[11]) / 100;
	env->altitude = ((int)frame[12] << 8 | frame[13]);

	return 0;
}

int sr501_open()
{
	int fd = open(SR501_DEV, O_RDWR);
	if (fd == -1)
	{
		perror("open " SR501_DEV);
		return -1;
	}
	printf("SR501 opened: %s\n", SR501_DEV);
	return fd;
}

int sr501_get_status(int fd)
{
	int flag = 0;
	int ret = read(fd, &flag, 4);
	if (ret < 0)
	{
		perror("read SR501");
		return -1;
	}
	return flag;
}

void sr501_close(int fd)
{
	close(fd);
}
