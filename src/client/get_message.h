#pragma once
#ifndef GET_MASSAGE_H
#define GET_MASSAGE_H
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#define FILE_PATH "/etc/file_watch/init.conf"

char filename_path[256];
int reads(int fd, void *vptr, int n);
void get_ip_addr(char ip[], int *port)
{
    //打开配置文件
    int fd = open(FILE_PATH,O_RDONLY);

    if (fd < 0) {
	perror("read error");
	exit(0);
    }
    //读取配置文件
    char c[200];

    bzero(c, sizeof(c));
    read(fd, c, sizeof(c));

    const int j = strlen(c);

    for (int i = 0; i < j; i++) {
	if (c[i] == '\n' || c[i] == '\r' || c[i] == '\t')
	    c[i] = 0;
    }
    for (int i = 0; i < j - 1; i++) {
	if (strncmp(&c[i], "addr:", 5) == 0) {
	    strcpy(ip, &c[i + 5]);
	    continue;
	}
	if (strncmp(&c[i], "port:", 5) == 0) {
	    *port = atoi(&c[i + 5]);
	    continue;
	}
	if (strncmp(&c[i], "path:", 5) == 0) {
	    strcpy(filename_path, &c[i + 5]);
	    continue;
	}
    }
    //关闭配置文件
    close(fd);

}

int reads(int fd, void *vptr, int n)
{

    int nleft;
    int nread;
    char *ptr;

    ptr = (char *) vptr;
    nleft = n;

    while (nleft > 0) {
	nread = read(fd, ptr, nleft);
	if (nread < 0) {
	    if (errno == EINTR)
		nread = 0;
	    else
		return -1;
	} else if (nread == 0) {
	    break;
	}

	nleft -= nread;
	ptr += nread;
    }
    return (n - nleft);
}

#endif				//GET_MASSAGE_H
