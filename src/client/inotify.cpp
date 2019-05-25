#include "ini.h"
#include "ip.hpp"
#include "main.hpp"
#include "threadpool.hpp"
struct data head_file; /*心跳包和recv中的函数 */

void *heart_handler(struct data *head_file, int sockfd, int keep_alive_flag)
{
    while (1) {
	if (head_file->count == 3)	// 3s*5没有收到心跳包，判定服务端掉线
	{
	    cout << "The server has be offline.\n";
	    close(sockfd);
	    Send_keep_alive('0');
	    keep_alive_flag = 0;
	} else if (head_file->count < 5 && head_file->count >= 0) {
	    head_file->count += 1;
	}
	sleep(3);		// 定时三秒
    }
}

void Recv_file(int sockfd, int keep_alive_flag)
{
    if (keep_alive_flag == 1) {
	int count = 0;

	while (1) {
	    memset(&head_file, 0, sizeof(head_file));
	    int res = recv(sockfd, &head_file, sizeof(struct data), 0);

	    count += head_file.length;
	    if (res < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
		    continue;
		cout << strerror(errno) << endl;
	    }
	    cout << "recv_name  " << head_file.file_name << endl;
	    if (head_file.sign == 4) {	/*心跳包的标志位 */
		head_file.count = 0;
	    } else {

		ofstream out;

		if (count <= 1024) {
		    out.open(head_file.file_name, ios::trunc);
		    out << head_file.file_contents;
		    out.close();
		    count = 0;
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
int main(int argc, char **argv)
{
    
    //const char *ip = "192.168.28.164";
    const char *ip = "127.0.0.1";
    int port = 8888;
    int keep_alive_flag = 1;
    struct sockaddr_in server_address;
    Inotify main_important; /*main中的函数*/
    struct filename_fd_desc FileArray[main_important.array_length];
    struct epoll_event Epollarray[main_important.epoll_number];
 
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    int fd, i, epollfd, wd;
    char readbuf[1024];
    epollfd = epoll_create(8);
    fd = inotify_init();
    base_dir = argv[1];
    if (argc < 2) {
	cout << "error argc too simple" << endl;
	return 1;
    }
    for (i = 1; i < argc; i++) {
	wd = inotify_add_watch(fd, argv[1], IN_OPEN | IN_CLOSE | IN_CREATE | IN_DELETE);
    main_important.Printdir(argv[1], 0, fd);
    }
    addfd(epollfd, fd, false);
    Send_keep_alive('1');
    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
	close(sockfd);
	Send_keep_alive('0');
	keep_alive_flag = 0;
    }
    thread t1(Recv_file, sockfd, keep_alive_flag);
    thread t2(heart_handler, &head_file, sockfd, keep_alive_flag);

    while (1) {
	int ret = epoll_wait(epollfd, Epollarray, 32, -1);

	for (i = 0; i < ret; i++) {
	    if (Epollarray[i].data.fd == fd) {
            if (-1 == (main_important.handle_events(epollfd, fd, argc, argv, FileArray, sockfd)))
            {
                return -1;
		}
	    } else {
		int readlen = readn(Epollarray[i].data.fd, readbuf, 1024);

		readbuf[readlen] = '\0';
	    }
	}
    }
    t1.join();
    t2.join();
    close(fd);
    return 0;
}
