#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


int main()
{
    //只写方式打开文件
    int fd = open("1.txt", O_WRONLY);
    if(fd == -1)
    {
        perror("open 1.txt error");
        exit(1);
    }
    printf("open 1.txt success\n");

    //往文件中写入数据
    char buf[10] = {"hello"};
    int ret = write(fd, buf, 5);
    if(ret == -1)
    {
        perror("write error");
        exit(1);
    }
    printf("write %d bytes datas\n", ret);

    //关闭文件
    close(fd);

    return 0;
}





