#pragma once
#ifndef MAIN_HPP
#define MAIN_HPP
/* code */
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <semaphore.h>
#include <stdio.h>
#include <string>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <string.h>
#include "ip.hpp"
#include "get_message.h"
using namespace std;

#define SHM_NAME_SEM "/memmap_sem"
char SHM_NAME[256];

int array_index = 0;
int temp_flag = 0;

struct filename_fd_desc {
    int fd;			// 文件描述符号
    char name[32];		// 文件名
    char base_name[128];	// 文件的绝对路径为了获取文件的文件描述符号
};

/*消息队列的结构题*/
struct msgmbuf {
    long long mtype;
    char mtext[1024];
};

struct data {
    long left;
    long right;
    int count;			//心跳包标志
    int sign;
    char mac[18];
    char file_name[256];
    int length;
    char file_contents[4096];
    char events[256];
};
struct recv_data {
    char count;			//心跳包标志
    char sign;
    char mac[18];
    char file_name[256];
    char file_contents[4096];
    char events[256];
    char length[256];
};
int readn(int fd, void *vptr, int n)
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
		nread = 0;	/* and call read() again */
	    else
		return -1;
	} else if (nread == 0) {
	    break;		/* EOF */
	}

	nleft -= nread;
	ptr += nread;
    }
    return (n - nleft);
}

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

//和hook程序通信服务器端是不是连同
void Send_keep_alive(char flag)
{
    int msg_id, msg_flags;
    struct msqid_ds msg_info;
    struct msgmbuf msg_mbuf;
    key_t key = 1000;

    msg_flags = IPC_CREAT;
    msg_mbuf.mtype = 0;
    memset(msg_mbuf.mtext, 0, sizeof(msg_mbuf.mtext));
    msg_mbuf.mtext[0] = flag;
    msg_id = msgget(key, msg_flags | 0666);
    if (-1 == msg_id) {
	return;
    }
    msg_mbuf.mtype = 1;
    msgsnd(msg_id, (void *) &msg_mbuf, 1024, IPC_NOWAIT);
}

class do_thing {
  private:
    /*获取文件路径长度 */
    static int Send_img(int sockfd, struct data *open_file, int t) {
	int ret = -1;
	int msg_flags, msg_id;
	int flag = 0;
	key_t key;
	struct msgmbuf msg_mbuf;

	 memset(msg_mbuf.mtext, '\0', sizeof(msg_mbuf.mtext));
	 key = t;
	 msg_flags = IPC_CREAT;
	 msg_id = msgget(key, msg_flags | 0666);

	/*创建一个消息队列 */
	if (-1 == msg_id) {
	    cout << "message failed !" << endl;
	}
	ret = msgrcv(msg_id, (void *) &msg_mbuf, 1024, 0, IPC_NOWAIT);

	if (-1 == ret) {
	    return -1;
	} else {
	    cout << "receive message[]   " << msg_mbuf.mtext << endl;
	    int temp = 0;
	    int g = 0;

	    for (g = strlen(msg_mbuf.mtext); g >= 0; g--) {
		if (msg_mbuf.mtext[g] == '/') {
		    break;
		}
	    }
	    for (; g < strlen(msg_mbuf.mtext); g++) {
		SHM_NAME[temp] = msg_mbuf.mtext[g];
		temp++;
	    }
	    if (strlen(msg_mbuf.mtext) > 0) {
		strcpy(open_file->file_name, msg_mbuf.mtext);
		open_file->length = msg_mbuf.mtype;
		return 1;
	    }
	}

	return 0;
    }


  public:
    /*多线程发送文件 */
    static void TransFile(struct data *open_file) {
	char ip[32];
	int port = 0;

	get_ip_addr(ip, &port);
	struct sockaddr_in server_address;

	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &server_address.sin_addr);
	server_address.sin_port = htons(port);
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);

	if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
	    close(sockfd);
	} else {
	    mutex mtx;

	    if (mtx.try_lock() && strlen(open_file->file_contents) > 0) {	// only increase if currently not locked:  
		int res = send(sockfd, open_file, sizeof(struct data), 0);

		if (res == -1) {
		    cout << strerror(errno) << endl;
		}
		mtx.unlock();
	    }
	    close(sockfd);
	}
    }
    /*获取共享内存 */
  private:
    static int Get_shm(int sockfd, struct data *open_file, char *name) {
	int fd;
	sem_t *sem;

	fd = shm_open(SHM_NAME, O_RDWR, 0666);
	//打开共享内存
	if (fd < 0) {
	    return -1;
	}
	sem = sem_open(SHM_NAME_SEM, 0);
	//信号量
	if (fd < 0 || sem == SEM_FAILED) {
	    return -1;
	}
	struct stat fileStat;

	fstat(fd, &fileStat);
	char *memPtr;

	memPtr = (char *) mmap(NULL, fileStat.st_size, PROT_READ, MAP_SHARED, fd, 0);
	/*映射到文件之后 */
	close(fd);

	long temp = 0;

	memset(open_file->file_contents, '\0', sizeof(open_file->file_contents));
	//多线程发送
	int n = (open_file->length / 4096 + 1);	/*设置10分 */
	size_t percent = 4095;
	struct data blocks[n];

	for (int temp = 0; temp < n; temp++) {
	    memset(&blocks[temp].file_contents, '\0', sizeof(blocks[temp].file_contents));
	    strcpy(blocks[temp].file_name, open_file->file_name);
	    blocks[temp].left = temp * percent;
	    strcpy(blocks[temp].mac, open_file->mac);
	    blocks[temp].length = open_file->length;
	    strcpy(blocks[temp].events, open_file->events);
	    memmove(blocks[temp].file_contents, memPtr + temp * percent, sizeof(blocks[temp].file_contents) - 1);
	    blocks[temp].right = temp * percent + strlen(blocks[temp].file_contents);
	}
	thread t1[n];

	for (int i = 0; i < n; ++i)
	    t1[i] = thread(TransFile, &blocks[i]);
      for (auto & th:t1)
	    th.join();
	sem_post(sem);
	sem_close(sem);
	munmap(memPtr, fileStat.st_size);
	shm_unlink(SHM_NAME);
    }

  public:
    static void Open_task(int sockfd, char *buffer, char *name) {

	struct data open_file;

	open_file.sign = 0;	//标志发送文件的请求
	strcpy(open_file.events, buffer);
	if (Send_img(sockfd, &open_file, 1024) == 1) {
	    cout << "open_task begin   " << endl;
	    IP real;

	    real.getLocalInfo();
	    strcpy(open_file.mac, real.real_mac);
	    /*获取ip的大小，并且发送文件内容 */
	    Get_shm(sockfd, &open_file, name);
	}
    }
  public:
    static void Close_task(int sockfd, char *buffer) {
	struct data close_file;

	close_file.sign = 1;	//标志接受文件的请求
	strcpy(close_file.events, buffer);
	if (Send_img(sockfd, &close_file, 1025) == 1) {
	    cout << "close_task begin   " << endl;
	    IP real;

	    real.getLocalInfo();
	    strcpy(close_file.mac, real.real_mac);
	    cout << "mac         " << close_file.mac << endl;
	    /*获取ip的大小，并且发送文件内容 */
	    if (send(sockfd, &close_file, sizeof(struct recv_data), 0) < 0) {
		perror("send error 298");
	    }
	}
    }
};

class Inotify {
  public:
    /*把目录递归添加到inotify去 */
    void Printdir(char *dir, int depth, int fd) {
	//打开目录指针
	DIR *Dp;

	//文件目录结构体
	struct dirent *enty;

	//详细文件信息结构体
	struct stat statbuf;

	//打开指定的目录，获得目录指针
	if (NULL == (Dp = opendir(dir))) {
	    fprintf(stderr, "can not open dir:%s\n", dir);
	    return;
	}
	//切换到这个目录 chdir(dir);

	//遍历这个目录下的所有文件
	while (NULL != (enty = readdir(Dp))) {
	    //通过文件名，得到详细文件信息
	    lstat(enty->d_name, &statbuf);
	    //判断是不是目录
	    if (S_ISDIR(statbuf.st_mode)) {
		//当前目录和上一目录过滤掉
		if (0 == strcmp(".", enty->d_name) || 0 == strcmp("..", enty->d_name)) {
		    continue;
		}
		//输出当前目录名
		int wd = inotify_add_watch(fd, enty->d_name, IN_OPEN | IN_CLOSE | IN_CREATE | IN_DELETE);

		//继续递归调用        Printdir(enty->d_name,depth+4);
	    }
	}
	//切换到上一及目录
	chdir("..");
	//关闭文件指针
	closedir(Dp);
    }

  public:
    static const int epoll_number = 32;
    static const int array_length = 128;
    static const int buffer_size = 1024;
    static const int name_length = 128;

    //要求监听文件的打开/关闭
    //epoll来对于文件内容改变的监控
    //定义一个数组用来存放对应文件的文件描述符号和文件名

};


#endif				//MAIN_H_
