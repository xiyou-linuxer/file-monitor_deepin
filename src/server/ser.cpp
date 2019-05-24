#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "locker.h"
#include "threadpool.h"
#include "monitor.h"

#define MAX_FD 1024       ///
#define MAX_EVENT_NUMBER 10000

extern int addfd( int epollfd, int fd ,bool one_shot);
extern int removefd(int epollfd, int fd );

void addsig( int sig, void( handler )(int),bool restart = true )
{
    struct sigaction sa;
    memset(&sa, '\0',sizeof( sa ));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
    assert(sigaction( sig, &sa, NULL ) != -1 );
}

int main(int argc,char **argv)
{
    if(argc <= 2)
    {
        printf("errno ... ...\n");
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    //忽略SIGPIPE信号 ？？
    addsig(SIGPIPE,SIG_IGN);
    //创建线程池
    threadpool< monitor >* pool = NULL;
    try
    {
        pool = new threadpool< monitor >;
    }
    catch(...)
    {
        return 1;
    }
    
    //预先为每个可能的用户分配一个monitor对象
    monitor* users = new monitor[MAX_FD];
    assert(users);
    int user_count = 0;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    struct linger tmp = { 1 , 0 };
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&tmp,sizeof(tmp));      //端口复用
 
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address ) );
    address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr );
    address.sin_port = htons(port);
    
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address ) );
    assert(ret >= 0);

    ret = listen(listenfd,5 );
    assert(ret >= 0);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    std::cout << "[listen fd]:" << ret << std::endl; 
    addfd( epollfd, listenfd, false );
    monitor::m_epollfd = epollfd;
    
    while(true)
    {
        int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER, -1);
        if( (number < 0)&& (errno != EINTR) )
        {
            printf("epoll failure\n");
            break;
        }

        for( int i = 0; i < number;i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd,(struct sockaddr*)&client_address,
                                    &client_addrlength );
                if(connfd < 0)
                {
                    printf("errno is:%d\n",errno);
                    continue;
                }
                //(if 到达最大文件数)
                //server is busy
                //countinue
                std::cout << "[acceptfd]connfd = :" << connfd << std::endl;
                users[connfd].init(connfd, client_address);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )
            {
                //如果出现异常
                close(sockfd);  
            }
            else if(events[i].events & EPOLLIN )
            {
                std::cout <<"^^^^"<<std::endl;
                printf(">>>>[this epoll IN!]<<<<\n");
                std::cout << "^^^^"<<std::endl;
                //如果是EPOLLIN 读
                if(users[sockfd].recv_masg())
                {

                    printf(">>[after recv][epoll in]recv_masg return\n");
                    // std::cout << ">>[before append][epool in]>>sockfd :" << sockfd << std::endl;
                    // pool->append( users + sockfd );
                }
                else
                {
                    printf("read false\n");
                    users[sockfd].close_mon();
                }
                
            }
            else if(events[i].events & EPOLLOUT )
            {
                //EPOLLOUT 写
            }
            else
            { }
            
        }
    }
    close(epollfd);
    close(listenfd);
    return 0;

}