#pragma once
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <iostream>


#include "locker.h"

class monitor
{
public:
    static const int BUFF_SIZE = 1024;
    static const int MAC_ADD_SIZE = 18;
    static const int FILE_NAME_LEN = 256;
    static const int BACKUP_F_NAME = 512;
    static const int EVENT_BUFF = 256;
    static const int READ_BUFF = 1204;
public:
    monitor() {}
    ~monitor() {}

private:
    typedef struct tag    //消息包
    {
        int sign = 0;   //标记位
        char mac_addr[MAC_ADD_SIZE];   //MAC地址
        char file_name[FILE_NAME_LEN];    //文件名
        int lenth;  //剩余长度
        char data[BUFF_SIZE];     //文件内容
        char event[EVENT_BUFF];    //时间内容
    }TAG;

public:
//All socket上的event都被注册到同一个epoll内核时间表中
//所以将epoll文件描述符设置为静态的
    static int m_epollfd;
//初始化新的接受链接
    void init(int sockfd,const sockaddr_in& addr);
    void close_mon(bool real_close = true );
//处理请求 入口
    void process();
    TAG* masg = new TAG;  //消息体

//读消息
    bool recv_masg();
    bool send_masg();
private:
//备份文件名称
    char backup_f_name[BACKUP_F_NAME];
    void init();//内部init
    //TAG* masg = new TAG;  //消息体
    void backup();
    void recover();

    void transf_file_name();
    void recover_file_name();
private:
//该请求链接的socket和对方的socket地址
    int m_sockfd;
    sockaddr_in m_address;
    int fd; //文件描述符
    unsigned long get_file_size(); //获取文件大小
    char read_buf[READ_BUFF]; //文件当前路径

};

