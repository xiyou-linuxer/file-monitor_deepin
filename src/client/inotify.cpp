#include "ini.h"
#include "ip.hpp"
#include "main.hpp"
#include "get_message.h"
struct recv_data head_file;	/*心跳包和recv中的函数 */

void *heart_handler( int sockfd, int keep_alive_flag)
{
	while (1) {
	if (keep_alive_flag == 1) {
		if (head_file.count == '3')	// 3s*5没有收到心跳包，判定服务端掉线
	    {
		cout << "The server has be offline.\n";
		close(sockfd);
		Send_keep_alive('0');
		keep_alive_flag = 0;
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
		    keep_alive_flag = 1;
		}
	    } else if (head_file.count < '3' && head_file.count >= '0') {
			head_file.count ++;
		}
		sleep(3);		// 定时三秒
	}
    }
}

void Recv_file(int sockfd, int keep_alive_flag)
{
    if (keep_alive_flag == 1) {
	int count = 0;

	while (1) {
	    int res = recv(sockfd, &head_file, sizeof(struct recv_data), MSG_WAITALL);
		if (res < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
		    continue;
		cout << strerror(errno) << endl;
	    }
	    if (res == 0) {
		Send_keep_alive('0');
		keep_alive_flag = 0;
		perror("The server has be offline.");
		exit(-1);
		}
	    if (head_file.sign == '4') {	/*心跳包的标志位 */
		head_file.count = '0';
	    } else {
		count += strlen(head_file.file_contents);
		ofstream out;

		if (count <= 4096) {
		    out.open(head_file.file_name, ios::trunc);
		    out << head_file.file_contents;
		    out.close();
		} else {
		    out.open(head_file.file_name, ios::app | ios::out);
		    out.seekp(count, ios::beg);
		    out << head_file.file_contents;
		    out.close();
		}
	    }
	}
    }
}

int handle_events(int epollfd, int fd, int argc, struct filename_fd_desc *FileArray, int sockfd)
{
    int i, k;
    ssize_t len;
    char *ptr;
    char buffer[1024];
    char buffer_temp[1024];
    char buf[2048];
    struct inotify_event *events;
    do_thing *temp = new do_thing();

    memset(buf, 0, sizeof(buf));
    len = read(fd, buf, sizeof(buf));
    for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + events->len) {
	events = (struct inotify_event *) ptr;
	memset(buffer, '\0', sizeof(buffer));
	memset(buffer_temp, '\0', sizeof(buffer_temp));
	if (events->len) {
	    if (events->mask & IN_OPEN) {
		strcpy(buffer, "open file");
	    }
	    if (events->mask & IN_CLOSE_NOWRITE) {
		strcpy(buffer_temp, "close file");
	    }
	    if (events->mask & IN_CLOSE_WRITE) {
		strcpy(buffer_temp, "close file");
	    }
	    if (events->mask & IN_CREATE) {	/* 如果是创建文件则打印文件名 */
		sprintf(FileArray[array_index].name, "%s", events->name);
		sprintf(FileArray[array_index].base_name, "%s%s", filename_path, events->name);
		int temp_fd = open(FileArray[array_index].base_name, O_RDWR);
		if(temp < 0) 
		{
			perror("create file add error");
		}
		FileArray[array_index].fd = temp_fd;
		addfd(epollfd, temp_fd, false);
		array_index++;
	    }
	    if (events->mask & IN_DELETE) {	/* 如果是删除文件也是打印文件名 */
		for (i = 0; i < 128; i++) {
		    if (!strcmp(FileArray[i].name, events->name)) {
			rm_fd(epollfd, FileArray[i].fd);
			FileArray[i].fd = 0;
			memset(FileArray[i].name, 0, sizeof(FileArray[i].name));
			memset(FileArray[i].base_name, 0, sizeof(FileArray[i].base_name));
			break;
		    }
		}
	    }
	}
	if ((strcmp(buffer, "open file") == 0)) {
	    strcat(buffer, events->name);
	    thread t1(temp->Open_task, sockfd, buffer, events->name);

	    t1.join();
	}
	if ((strcmp(buffer_temp, "close file") == 0)) {
	    strcat(buffer_temp, events->name);
	    thread t2(temp->Close_task, sockfd, buffer_temp);

	    t2.join();
	}
    }
}

int main(int argc, char **argv)
{
    char ip[32];
    int port = 0;

    get_ip_addr(ip, &port);
    int keep_alive_flag = 1;
    struct sockaddr_in server_address;
    Inotify main_important;	/*main中的函数 */
    struct filename_fd_desc FileArray[main_important.array_length];
    struct epoll_event Epollarray[main_important.epoll_number];

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &server_address.sin_addr) == -1) {
	perror("error ip");
    }
    server_address.sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
	perror("sockfd error");
    }
    int fd, i, epollfd, wd;
    char readbuf[1024];

    epollfd = epoll_create(8);
    fd = inotify_init();

    wd = inotify_add_watch(fd, filename_path, IN_OPEN | IN_CLOSE | IN_CREATE | IN_DELETE);
    main_important.Printdir(filename_path, 0, fd);
    
    addfd(epollfd, fd, false);
    Send_keep_alive('1');
    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
	close(sockfd);
	Send_keep_alive('0');
	keep_alive_flag = 0;
    }
    thread t2(heart_handler, sockfd, keep_alive_flag);
    thread t1(Recv_file, sockfd, keep_alive_flag);
    while (1) {
	int ret = epoll_wait(epollfd, Epollarray, 32, -1);

	for (i = 0; i < ret; i++) {
	    if (Epollarray[i].data.fd == fd) {
		if (-1 == (handle_events(epollfd, fd, argc, FileArray, sockfd))) {
		    return -1;
		}
	    } else {
		int readlen = read(Epollarray[i].data.fd, readbuf, 1024);

		readbuf[readlen] = '\0';
	    }
	}
    }
    t1.join();
    t2.join();
    close(fd);
    return 0;
}
