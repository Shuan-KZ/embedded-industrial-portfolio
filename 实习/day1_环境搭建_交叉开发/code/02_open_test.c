#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


int main()
{
    //只读方式打开文件
    int fd = open("1.txt", O_RDONLY);
    if(fd == -1)
    {
        perror("open 1.txt error");
        exit(1);
    }
    printf("open 1.txt success\n");

    //关闭文件
    close(fd);

    return 0;
}





