#include "event.h"

static int fd_ev = -1;

void event_init(void)
{
	fd_ev = open("/dev/input/event0", O_RDONLY);
	if (fd_ev == -1) {
		perror("open event0 error");
		exit(1);
	}
	printf("open event0 success\n");
}

void event_deinit(void)
{
	if (fd_ev != -1) {
		close(fd_ev);
		fd_ev = -1;
	}
}

/*
	功能: 获取滑动方向
	返回值:
		返回滑动方向
*/
int get_event_direction()
{
	struct input_event data;

	int x0 = -1, y0 = -1;
	int x1 = -1, y1 = -1;

	while (1) {
		int ret = read(fd_ev, &data, sizeof(data));
		if (ret == -1) {
			if (errno == EINTR) continue;
			perror("read ev error");
			exit(1);
		}

		if (data.type == EV_ABS && data.code == ABS_X) {
			if (x0 == -1) x0 = data.value;
			else          x1 = data.value;
		}

		if (data.type == EV_ABS && data.code == ABS_Y) {
			if (y0 == -1) y0 = data.value;
			else          y1 = data.value;
		}

		if (data.type == EV_KEY && data.code == BTN_TOUCH && data.value == 0) {
			printf("x0 = %d, y0 = %d, x1 = %d, y1 = %d\n", x0, y0, x1, y1);

			if (x0 == -1 || x1 == -1 || y0 == -1 || y1 == -1)
				continue;

			int x = x1 - x0;
			int y = y1 - y0;

			if (abs(x) > abs(y)) {
				if (x > 50)  return RIGHT;
				if (x < -50) return LEFT;
			} else {
				if (y > 50)  return DOWN;
				if (y < -50) return UP;
			}
		}
	}
}











