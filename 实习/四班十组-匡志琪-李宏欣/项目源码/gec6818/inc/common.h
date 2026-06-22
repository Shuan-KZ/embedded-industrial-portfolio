
#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>  
#include <stdbool.h>  
#include <stdint.h>  
#include <malloc.h>  
#include <unistd.h>  
#include <stdlib.h>  
#include <string.h>  
#include <getopt.h>  
#include <fcntl.h>  
#include <ctype.h>  
#include <errno.h>  
#include <limits.h>  
#include <time.h> 
#include <locale.h>  
#include <signal.h>  
#include <pthread.h>

#include <sys/unistd.h>  
#include <sys/stat.h>  
#include <sys/types.h>  
#include <sys/socket.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <arpa/inet.h>

#include <termios.h> 

#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "jpeglib.h"

// 通用
int Open(const char *pathname, int flag);
ssize_t Read(int fildes, void *buf, size_t nbyte);
ssize_t Write(int fildes, const void *buf, size_t nbyte);

// 网络相关
#define DEF_PORT 54321
#define XMLSIZE  1024

int Socket(int domain, int type, int protocol);
int Setsockopt(int sockfd, int level, int optname,
	       const void *optval, socklen_t optlen);
int Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);


int init_tcp_socket(const char *ubuntu_ip);
void send_pcm(int sockfd, char *pcmfile);
xmlChar *wait4id(int sockfd);


// XML相关
#define XMLFILE "result.xml"
xmlChar *parse_xml(char *xmlfile);


#endif