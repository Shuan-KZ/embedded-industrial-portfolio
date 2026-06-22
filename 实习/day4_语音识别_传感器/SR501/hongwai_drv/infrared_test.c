#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int main(void)
{
	int fd;
	int ret;
	int infrared_flag=0; 
	//打开设备文件---/dev/infrared_drv
	fd = open("/dev/infrared_drv", O_RDWR); 
	if(fd < 0)
	{
		perror("open /dev/infrared_drv");
		return -1;	
	}
	while(1)
	{	
		ret = read(fd,&infrared_flag,4);		
		if (ret < 0)
		{
			perror("read /dev/key_drv error\n");
			return -1;
		}

		printf("infrared flag = %d\n",infrared_flag);
		usleep(300*1000);
			
	}
	close(fd);	
	return 0;
}

