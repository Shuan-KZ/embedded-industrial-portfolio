#include "common.h"
#include <stdlib.h>
#include "lcd.h"
#include "event.h"

//#define REC_CMD  "./arecord -d4 -c1 -r16000 -traw -fS16_LE cmd.pcm"
#define REC_CMD  "arecord -d3 -c1 -r16000 -traw -fS16_LE cmd.pcm"
#define PCM_FILE "./cmd.pcm"
/* -d：录音时长（duration）
-c：音轨（channels）
-r：采样频率（rate）
-t：封装格式（type）
-f：量化位数（format） */

void catch (int sig)
{
	if (sig == SIGPIPE)
	{
		printf("killed by SIGPIPE\n");
		exit(0);
	}
}

int main(int argc, char const* argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s <ubuntu-IP>\n", argv[0]);
		exit(0);
	}
	
	printf("----------hahaha------------\n");
	
	signal(SIGPIPE, catch);
	// 初始化TCP客户端套接字，用于连接到语音识别服务器(即ubuntu)
	int sockfd_tcp = init_tcp_socket(argv[1]);
	// 初始化本地UDP套接字
	int sockfd_udp = init_udp_socket();

	int id_num = -1; // 识别后的指令id
	while (1)
	{
		// 1，调用arecord来录一段音频
		printf("please to start REC in 3s...\n");

		// 在程序中执行一条命令  “录音的命令”
		system(REC_CMD);

		// 2，将录制好的PCM音频发送给语音识别引擎
		send_pcm(sockfd_tcp, PCM_FILE);

		// 3，等待对方回送识别结果（字符串ID）
		xmlChar* id = wait4id(sockfd_tcp);
		if (id == NULL)
		{
			continue;
		}
		id_num = atoi((char*)id);
		if (id_num == 999)
		{
			printf("bye-bye!\n");
			exit(1);
		}
		else if (id_num == 20)
		{
			printf("hello hello-----------------!!!\n");
		}
		else if (id_num == 8)
		{
			//system("mplayer 1.mp4");
				// int fd = open("/dev/s3c2410_serial1",O_RDWR);
			int fd = open("/dev/ttySAC1",O_RDWR);
			if(fd < 0)
			{
				perror("open serial failed");
				return -1;
			}
			init_serial(fd);


			int dist;
			dist = us_check_dist(fd);
			if(dist < 0)
			{
				printf("check error\n");
			}
			printf("dist = %d mm\n",dist);
		}			
		
		printf("recv id: %d \n", id_num);
		// udp发送数据给接收端, 接收端收到数据id后，再决定执行什么功能
		// send_id(sockfd_udp, id_num);
		
	}

	return 0;
}
