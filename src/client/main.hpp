#ifndef MAIN_HPP_
#define MAIN_HPP_
/* code */
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <semaphore.h>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fstream>
using namespace std;

struct filename_fd_desc
{
    int fd;              // 文件描述符号
    char name[32];       // 文件名
    char base_name[128]; // 文件的绝对路径为了获取文件的文件描述符号
};
struct data
{
    int sign ;
    char mac[18];
    char file_name[256];
    int length;
    char file_contents[1024];
    char events[256];
};
class Inotify
{
    public:
    int addfd(int epollfd, int fd, bool enable_et)
    {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN;
        int result = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        return result;
    }
    void rm_fd(int epollfd, int fd)
    {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    }

  public:
    static const int epoll_number  = 32;
    static const int array_length  = 128; 
    static const int buffer_size  = 1024;
    static const int name_length  = 128; 
    //要求监听文件的打开/关闭
    //epoll来对于文件内容改变的监控
    //定义一个数组用来存放对应文件的文件描述符号和文件名
};
int readn(int fd, void *vptr, int n)
{
    int nleft;
    int nread;
    char *ptr;

    ptr = (char *)vptr;
    nleft = n;

    while (nleft > 0)
    {
        nread = read(fd, ptr, nleft);
        if (nread < 0)
        {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return -1;
        }
        else if (nread == 0)
        {
            break; /*	EOF */
        }

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft);
}

int writen(int fd, const void *vptr, int n)
{
    int nleft;
    int nwritten;
    const char *ptr;

    ptr = (const char *)vptr;
    nleft = n;

    while (nleft > 0)
    {
        nwritten = write(fd, ptr, nleft);
        if (nwritten <= 0)
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }

        nleft -= nwritten;
        ptr += nwritten;
    }

    return n;
}

#endif //MAIN_H_
