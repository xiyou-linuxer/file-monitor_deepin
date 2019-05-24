#include "ini.h"
#include "ip.hpp"
#include "main.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/types.h>
char SHM_NAME [256];
#define SHM_NAME_SEM "/memmap_sem"

int array_index = 0;
const char *base_dir;
int temp_flag = 0;
Inotify main_important;

/*消息队列的结构题*/
struct msgmbuf {
    long long mtype;
    char mtext[1024];
};

/*把目录的子目录中添加到监听队列之中*/
void printdir(char *dir, int depth, int fd)
{
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
    //切换到这个目录
    chdir(dir);
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

	    //继续递归调用        printdir(enty->d_name,depth+4);
	}
    }
    //切换到上一及目录
    chdir("..");
    //关闭文件指针
    closedir(Dp);
}

void transFile(int sockfd ,struct data * open_file)
{
   cout  << open_file->file_contents ;
    // int res = send(sockfd, open_file, sizeof(struct data), 0);
    // if (res == -1)
    // {
    //     cout << strerror(errno) << endl;
    // }
}
    /*获取共享内存*/
int get_shm(int sockfd, struct data *open_file, char *name)
{
    cout << "SHM_NAME   "  << SHM_NAME << endl;
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
    memPtr = (char *) mmap(NULL, fileStat.st_size, PROT_READ , MAP_SHARED, fd, 0);
    /*映射到文件之后 */
    close(fd);
    cout << " length  " << open_file->events  << "  ssssssss"<< endl;
    long temp = 0;
    memset(open_file->file_contents, '\0',
           sizeof(open_file->file_contents));
    //多线程发送
    int n = (open_file->length / 1024 + 4); /*设置10分*/
    size_t percent = 1024;
    struct data blocks[n];
    char file_name_test[2];
    for(int temp = 0 ; temp < n ;temp++)
    {
        memset(file_name_test,'\0',sizeof(file_name_test));
        file_name_test[0] = '@';
        file_name_test[1] = temp +'0';
        strcpy(blocks[temp].file_name,open_file->file_name);
        strcat(blocks[temp].file_name,file_name_test);
        strcpy(blocks[temp].mac,open_file->mac);
        blocks[temp].length = open_file->length;
        strcpy(blocks[temp].events,open_file->events);
        memmove(blocks[temp].file_contents , memPtr+temp*percent ,sizeof(blocks[temp].file_contents));      
    }
    thread t[n];
    for (int i = 0; i < n; ++i)
    {
        t[i] = thread(transFile,sockfd,&blocks[i]);
    }
    std::for_each(t, t + n, [](thread &t) { t.join(); });
    cout << endl;
    // for (int i = 0; i < (open_file->length /1024 + 4)*1024; i++) {
    //   if (i % 1024 == 0) {
    //     if (strlen(open_file->file_contents) > 0) {

    //         int res = send(sockfd, open_file, sizeof(struct data), 0);
    //         if (res == -1)
    //         {
    //             cout << strerror(errno) << endl;
    //       }
    //     }
    //     memset(open_file->file_contents, '\0',
    //            sizeof(open_file->file_contents));
    //     temp = 0;
    //   }
    //   open_file->file_contents[temp] = memPtr[i];
    //   temp++;
    // }
    // if(temp > 0)
    // {
    //     int res = send(sockfd, open_file, sizeof(struct data), 0);
    //     if (res == -1)
    //     {
    //         cout << strerror(errno) << endl;
    //     }
    // }
    sem_post(sem);
    sem_close(sem);
    munmap(memPtr, fileStat.st_size);
    shm_unlink(SHM_NAME);
}
void send_keep_alive(char flag)
{

    int msg_id, msg_flags;
    struct msqid_ds msg_info;
    struct msgmbuf msg_mbuf;
    key_t key = 1000;
    msg_flags = IPC_CREAT;
    msg_mbuf.mtype = 0;
    memset(msg_mbuf.mtext, 0, sizeof(msg_mbuf.mtext));
    msg_mbuf.mtext[0]= flag;
    //  printf("pathname    %s\n",msg_mbuf.mtext);
    msg_id = msgget(key, msg_flags | 0666);
    if (-1 == msg_id)
    {
        return;
    }
    msg_mbuf.mtype = 1;
    msgsnd(msg_id, (void *)&msg_mbuf, 1024, IPC_NOWAIT);
}
/*获取文件路径长度*/
int  send_img(int sockfd, struct data *open_file,int t)
{
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
    ret = msgrcv(msg_id, (void *)&msg_mbuf, 1024, 0,IPC_NOWAIT);
    if (-1 == ret) {
    return -1;
    } else {
	cout << "receive message[]   " << msg_mbuf.mtext << endl;
    int temp = 0;
    int g = 0;
    for (g = strlen(msg_mbuf.mtext); g >= 0; g--)
    {
        if (msg_mbuf.mtext[g] == '/')
        {
            break;
        }
    }
    for (; g < strlen(msg_mbuf.mtext); g++)
    {
        SHM_NAME[temp] = msg_mbuf.mtext[g];
        temp++;
    }
    if (strlen(msg_mbuf.mtext) > 0)
    {
        strcpy(open_file->file_name, msg_mbuf.mtext);
        open_file->length = msg_mbuf.mtype;
        return 1;
    }
    }
    
    return 0; 
}

/*open所要打开的工作函数*/
void open_task(int sockfd, char *buffer, char *name)
{

    struct data open_file;
    open_file.sign = 0;		//标志发送文件的请求
    strcpy(open_file.events, buffer);
    if(send_img(sockfd,&open_file,1024) == 1)
   {
       cout << "open_task begin   " << endl;
       IP real;
       real.getLocalInfo();
       strcpy(open_file.mac, real.real_mac);
/*获取ip的大小，并且发送文件内容 */
       get_shm(sockfd, &open_file, name);
   }
}

void close_task(int sockfd, char *buffer)
{

    struct data close_file;
    close_file.sign = 1; //标志接受文件的请求
    strcpy(close_file.events, buffer);
  // cout << "close_task begin   " << endl;
    if(send_img(sockfd, &close_file,1025) == 1)
   {
       cout << "close_task begin   " << endl;
       IP real;
       real.getLocalInfo();
       strcpy(close_file.mac, real.real_mac);
       cout << "mac         " << close_file.mac << endl;
       /*获取ip的大小，并且发送文件内容*/
       send(sockfd, &close_file, sizeof(struct data), 0);
   }
}

void recv_file(int sockfd, int keep_alive_flag)
{
   if(keep_alive_flag == 1)
    {
        struct data close_file;
        int count = 0;
        while (1)
        {
            memset(&close_file,0,sizeof(close_file));
            int res = recv(sockfd, &close_file, sizeof(struct data), 0);
            count += close_file.length;
            if(res < 0)
            {
                cout << strerror(errno) << endl;
            }
            cout << "recv_name  "<<close_file.file_name << endl;
            ofstream out;
            if(count <= 1024)
            {
                out.open(close_file.file_name, ios::trunc);
                out << close_file.file_contents;
                out.close();
                count = 0;
            }
            else
            {
                out.open(close_file.file_name,ios::app | ios::out);
                out.seekp(count,ios::beg);
                out << close_file.file_contents;
                out.close();
            }
       }
    }
}
static int handle_events(int epollfd, int fd, int argc, char *argv[], struct filename_fd_desc *FileArray, int sockfd)
{
    int i, k;
    ssize_t len;
    char *ptr;
    char buffer[1024];
    char buffer_temp[1024];
    char buf[2048];
    struct inotify_event *events;

    std::threadpool executor {20};
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
		sprintf(FileArray[array_index].base_name, "%s%s", base_dir, events->name);
		int temp_fd = open(FileArray[array_index].base_name, O_RDWR);

		if (temp_fd == -1) {
		    return -1;
		}
		// cout << "create file" << endl;
		FileArray[array_index].fd = temp_fd;
		main_important.addfd(epollfd, temp_fd, false);
		array_index++;
		cout << "add file   " << events->name << endl;
	    }
	    if (events->mask & IN_DELETE) {	/* 如果是删除文件也是打印文件名 */
		for (i = 0; i < main_important.array_length; i++) {
		    if (!strcmp(FileArray[i].name, events->name)) {
			main_important.rm_fd(epollfd, FileArray[i].fd);
			FileArray[i].fd = 0;
			memset(FileArray[i].name, 0, sizeof(FileArray[i].name));
			memset(FileArray[i].base_name, 0, sizeof(FileArray[i].base_name));
			printf("delete file to epoll %s\n", events->name);
			break;
		    }
		}
		cout << "delete file   " << events->name << endl;
	    }
	}
	if ((strcmp(buffer, "open file") == 0)) {
	    strcat(buffer, events->name);
	    executor.commit(open_task, sockfd, buffer,events->name);
    }
 
	if((strcmp(buffer_temp,"close file") == 0 ))
	{
        strcat(buffer_temp,events->name);
        executor.commit(close_task,sockfd,buffer_temp);
    }
    }
}

int main(int argc, char **argv)
{
    struct filename_fd_desc FileArray[main_important.array_length];
    struct epoll_event Epollarray[main_important.epoll_number];
    //const char *ip = "192.168.28.164";
    const char *ip = "127.0.0.1";
    int port = 8888;
    int keep_alive_flag = 1;
    struct sockaddr_in server_address;

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
	printdir(argv[1], 0, fd);
    }
    main_important.addfd(epollfd, fd, false);
    send_keep_alive('1');
    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
	close(sockfd);
//    send_keep_alive('0');
  //  keep_alive_flag = 0;
    }
   // thread t1(recv_file,sockfd,keep_alive_flag);
    while (1) {
    int ret = epoll_wait(epollfd, Epollarray, 32, -1);

    for (i = 0; i < ret; i++) {
        if (Epollarray[i].data.fd == fd) {
                   if (-1 == (handle_events(epollfd, fd, argc, argv, FileArray, sockfd)))
                  {
                    return -1;
                  }
        } else {
        int readlen = read(Epollarray[i].data.fd, readbuf, 1024);

        readbuf[readlen] = '\0';
        }
    }
    }
  //  t1.join();
    close(fd);
    return 0;
}
