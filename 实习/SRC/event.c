#include "event.h"

/*
	功能: 获取触摸点的坐标数据
	参数:
		x: 保存横坐标数据
		y: 保存纵坐标数据
*/
void get_event_xy(int* x, int* y)
{
	//打开触摸屏
	int fd_ev = open("/dev/input/event0", O_RDONLY);
	if(fd_ev == -1)
	{
		perror("open event0 error");
		exit(1);
	}
	printf("open event0 success\n");

	struct input_event data; //定义一个结构体变量用来存储输入事件的数据

	int ret;
	while(1)
	{
		ret = read(fd_ev, &data, sizeof(data));
		if(ret == -1)
		{
			perror("read ev error");
			exit(1);
		}

		//事件类型为触摸屏事件
		//标识符为获取横坐标数据
		if(data.type == EV_ABS && data.code == ABS_X)
		{
			*x = data.value;
		}

		
		//事件类型为触摸屏事件
		//标识符为获取纵坐标数据
		if(data.type == EV_ABS && data.code == ABS_Y)
		{
			*y = data.value;
		}

		//当手指离开屏幕的时候，返回此时的坐标数据
		if(data.type == EV_KEY && data.code == BIN_TOUCH && data.value == 0)
		{
			close(fd_ev);

			return ;
		}
	}
}


/*
	功能: 获取滑动方向
	返回值:
		返回滑动方向
*/
int get_event_direction()
{
	//打开触摸屏
	int fd_ev = open("/dev/input/event0", O_RDONLY);
	if(fd_ev == -1)
	{
		perror("open event0 error");
		exit(1);
	}
	printf("open event0 success\n");

	struct input_event data; //定义一个结构体变量用来存储输入事件的数据

	int x0 = -1, y0 = -1; //保存第一次触摸点的坐标
	int x1 = -1, y1 = -1; //保存后续触摸点的坐标

	int ret;
	while(1)
	{
		ret = read(fd_ev, &data, sizeof(data));
		if(ret == -1)
		{
			perror("read ev error");
			exit(1);
		}

		//事件类型为触摸屏事件
		//标识符为获取横坐标数据
		if(data.type == EV_ABS && data.code == ABS_X)
		{
			if(x0 == -1) //第一次获取坐标数据
			{
				x0 = data.value;
			}
			else //后续的坐标获取
			{
				x1 = data.value;
			}
		}

		
		//事件类型为触摸屏事件
		//标识符为获取纵坐标数据
		if(data.type == EV_ABS && data.code == ABS_Y)
		{
			if(y0 == -1) //第一次获取坐标数据
			{
				y0 = data.value;
			}
			else //后续的坐标获取
			{
				y1 = data.value;
			}
		}

		//当手指离开屏幕的时候，返回此时的滑动方向
		if(data.type == EV_KEY && data.code == BIN_TOUCH && data.value == 0)
		{
			printf("x0 = %d, y0 = %d, x1 = %d, y1 = %d\n", x0, y0, x1, y1);

			//说明触摸点获取错误, 重新开始循环，此次滑动认为是失败的
			if(x0 == -1 || x1 == -1 || y0 == -1 || y1 == -1)
				continue;
	
			int x = x1-x0; //取横向位移距离
			int y = y1-y0; //取纵向位移距离

			if(abs(x) > abs(y)) //横向位移大于纵向位移 左右滑动
			{
				//滑动距离超过50个触摸点坐标
				if(x > 50) //右滑
				{
					close(fd_ev);
					return RIGHT;
				}
				else if(x < -50) //左滑
				{
					close(fd_ev);
					return LEFT;
				}
			}
			else //横向位移小于等于纵向位移 上下滑动
			{
				//滑动距离超过50个触摸点坐标
				if(x > 50) //下滑
				{
					close(fd_ev);
					return DOWN;
				}
				else if(x < -50) //上滑
				{
					close(fd_ev);
					return UP;
				}
			}
		}
	}
}


/*
	功能: 手动控制电子相册
*/
void hand_photos()
{
	char* bmps[3] = {"1.bmp", "2.bmp", "3.bmp"};

	int i = 0;
	int dir = -1;

	lcd_drwa_bmp(0, 0, bmps[i]);
	while(1)
	{
		//获取滑动方向
		dir = get_event_direction();

		if(dir == LEFT) //下一张
		{
			i++;
			if(i > 2)
				i = 0;
		}
		else if(dir == RIGHT) //上一张
		{
			i--;
			if(i < 0)
				i = 2;
		}
		else if(dir == UP) //上滑结束程序
		{
			exit(1);
		}

		lcd_drwa_bmp(0, 0, bmps[i]);
	}
}







