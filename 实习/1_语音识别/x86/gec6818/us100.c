#include <stdio.h>
#include <fcntl.h> 
#include <unistd.h>
#include <termios.h> 
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int init_serial(int fd)
{

	/*设置串口
	  波特率:9600
	  数据位:8
	  校验位:不要
	  停止位:1
	  数据流控制:无
	  */
#if 0
	struct termios
	{
		tcflag_t c_iflag;       /* input mode flags */
		tcflag_t c_oflag;       /* output mode flags */
		tcflag_t c_cflag;       /* control mode flags */
		tcflag_t c_lflag;       /* local mode flags */
		cc_t c_line;            /* line discipline */
		cc_t c_cc[NCCS];        /* control characters */
		speed_t c_ispeed;       /* input speed */
		speed_t c_ospeed;       /* output speed */
#define _HAVE_STRUCT_TERMIOS_C_ISPEED 1
#define _HAVE_STRUCT_TERMIOS_C_OSPEED 1
	};
#endif
	struct termios myserial;
	//清空结构体
	memset(&myserial, 0, sizeof (myserial));

	//O_RDWR               
	myserial.c_cflag |= (CLOCAL | CREAD);
	//设置控制模式状态，本地连接，接受使能
	//设置 数据位
	myserial.c_cflag &= ~CSIZE;   //清空数据位
	myserial.c_cflag &= ~CRTSCTS; //无硬件流控制
	myserial.c_cflag |= CS8;      //数据位:8

	myserial.c_cflag &= ~CSTOPB;//   //1位停止位
	myserial.c_cflag &= ~PARENB;  //不要校验
	//myserial.c_iflag |= IGNPAR;   //不要校验
	//myserial.c_oflag = 0;  //输入模式
	//myserial.c_lflag = 0;  //不激活终端模式

	cfsetospeed(&myserial, B9600);  //设置波特率
	cfsetispeed(&myserial, B9600);

	/* 刷新输出队列,清楚正接受的数据 */
	tcflush(fd, TCIFLUSH);

	/* 改变配置 */
	tcsetattr(fd, TCSANOW, &myserial);

}
//测距离
int us_check_dist(int fd)
{
	unsigned char cmd = 0x55;
	int res = write(fd,&cmd,sizeof(cmd));
	if(res < 0)
	{
		perror("write dist cmd");
		return -1;
	}
	
	unsigned char cmd_ack[2];
	int total_read = 0;
	while(total_read < 2)
	{
		res = read(fd,cmd_ack + total_read,sizeof(cmd_ack) - total_read);
		if(res >= 0)
		{
			total_read += res;
		}
		else
		{
			printf("read error\n");
			return -1;
		}
	}
	return cmd_ack[0]*256 + cmd_ack[1];

}


