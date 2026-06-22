//////////////////////////////////////////////////////////////////
//
//  Copyright(C), 2013-2017, GEC Tech. Co., Ltd.
//
//  File name: GPLE/common.c
//
//  Author: GEC
//
//  Date: 2017-01
//  
//  Description: 通用函数汇总
//
//
//////////////////////////////////////////////////////////////////

#include "common.h"

/********************* 通用函数列表 *******************/

int Open(const char* pathname, int flag)
{
	int fd;
	while ((fd = open(pathname, flag)) == -1 && errno == EINTR);

	if (fd == -1)
	{
		perror("open() failed");
		exit(0);
	}

	return fd;
}

ssize_t Write(int fildes, const void* buf, size_t nbyte)
{
	ssize_t n;
	size_t total = 0;

	char* tmp = (char*)buf;
	while (nbyte > 0)
	{
		while ((n = write(fildes, tmp, nbyte)) == -1 &&
			errno == EINTR);

		if (n == -1)
		{
			perror("write() error");
			exit(0);
		}

		nbyte -= n;
		tmp += n;
		total += n;
	}

	return total;
}

ssize_t Read(int fildes, void* buf, size_t nbyte)
{
	ssize_t n;
	while ((n = read(fildes, buf, nbyte)) == -1
		&& errno == EINTR);
	//n=read(fildes, buf, nbyte);
	//printf("read : n=%d\n",n);
	if (n == -1)
	{
		perror("read() failed");
		exit(0);
	}

	return n;
}


/********************* 网络函数列表 *******************/

int Connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
	int ret = connect(sockfd, addr, addrlen);
	if (ret == -1)
	{
		perror("connect() failed");
		exit(0);
	}

	return ret;
}

int Socket(int domain, int type, int protocol)
{
	int sockfd = socket(domain, type, protocol);
	if (sockfd == -1)
	{
		perror("socket() error");
	}

	return sockfd;
}

int Setsockopt(int sockfd, int level, int optname,
	const void* optval, socklen_t optlen)
{
	int retval = setsockopt(sockfd, level, optname, optval, optlen);
	if (retval == -1)
	{
		perror("setsockopt() error");
	}

	return retval;
}

int init_tcp_socket(const char* ubuntu_ip)
{
	struct sockaddr_in sin;
	int sockfd = Socket(AF_INET, SOCK_STREAM, 0);

	int on = 1;
	Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(1));

	//连接到虚拟机中
	bzero((char*)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	inet_pton(AF_INET, ubuntu_ip, &sin.sin_addr);
	sin.sin_port = htons(DEF_PORT);

	Connect(sockfd, (struct sockaddr*)&sin, sizeof(sin));
	printf("connected.\n");

	return sockfd;
}


void send_pcm(int sockfd, char* pcmfile)
{
	// 打开PCM文件
	int fd = Open(pcmfile, O_RDONLY);

	// 取得PCM数据的大小
	off_t pcm_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	// 读取PCM数据
	char* pcm = calloc(1, pcm_size);
	Read(fd, pcm, pcm_size);

	// 将PCM发送给语音识别引擎系统
	int m = Write(sockfd, pcm, pcm_size);
	printf("%d bytes has been write into socket!\n", m);

	free(pcm);
}

xmlChar* wait4id(int sockfd)
{
	char* xml = calloc(1, XMLSIZE);

	// printf("wait4id: 1\n");
	// 从ubuntu接收XML结果
	int n = Read(sockfd, xml, XMLSIZE);
	printf("%d bytes has been recv from ubuntu.\n", n);

	// 将XML写入本地文件 XMLFILE 中
	FILE* fp = fopen(XMLFILE, "w");
	if (fp == NULL)
	{
		perror("fopen() failed");
		exit(0);
	}

	size_t m = fwrite(xml, 1, n, fp);
	if (m != n)
	{
		perror("fwrite() failed");
		exit(0);
	}

	fflush(fp);

	return parse_xml(XMLFILE);
}


/********************* XML函数列表 *******************/

xmlChar* __get_cmd_id(xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar* key, * id;
	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"cmd")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			printf("cmd: %s\n", key);
			xmlFree(key);

			id = xmlGetProp(cur, (const xmlChar*)"id");
			printf("id: %s\n", id);

			xmlFree(doc);
			return id;
		}
		cur = cur->next;
	}

	xmlFree(doc);
	return NULL;
}

xmlChar* parse_xml(char* xmlfile)
{
	xmlDocPtr doc;
	xmlNodePtr cur1, cur2;

	doc = xmlParseFile(xmlfile);
	if (doc == NULL)
	{
		fprintf(stderr, "Document not parsed successfully. \n");
		return NULL;
	}

	cur1 = xmlDocGetRootElement(doc);
	if (cur1 == NULL)
	{
		fprintf(stderr, "empty document\n");
		xmlFreeDoc(doc);
		return NULL;
	}

	if (xmlStrcmp(cur1->name, (const xmlChar*)"nlp"))
	{
		fprintf(stderr, "document of the wrong type, root node != nlp");
		xmlFreeDoc(doc);
		return NULL;
	}

	cur1 = cur1->xmlChildrenNode;

	while (cur1 != NULL)
	{
		if ((!xmlStrcmp(cur1->name, (const xmlChar*)"result")))
		{
			cur2 = cur1->xmlChildrenNode;
			while (cur2 != NULL)
			{
				if ((!xmlStrcmp(cur2->name, (const xmlChar*)"confidence")))
				{
					xmlChar* key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					if (atoi((char*)key) < 30)
					{
						xmlFree(doc);
						fprintf(stderr, "sorry, I'm NOT sure what you say.\n");
						return NULL;
					}
				}

				if ((!xmlStrcmp(cur2->name, (const xmlChar*)"object")))
				{
					return __get_cmd_id(doc, cur2);
				}
				cur2 = cur2->next;
			}
		}
		cur1 = cur1->next;
	}

	xmlFreeDoc(doc);
	return NULL;
}


