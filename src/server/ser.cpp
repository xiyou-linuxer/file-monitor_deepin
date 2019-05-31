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
#include <mutex>
#include "RAII/locker.h"
#include "threadpool/threadpool.h"
#include "monitor/monitor.h"

#include "log/log.cpp"
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
    Log::get_instance()->init("var/log/mylog.log",8192,2000000,0);      //日志

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

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if(listenfd < 0)
    {
        LOG_ERROR("Creat Socket Failed",0);
        Log::get_instance()->flush();
        exit(1);
    }
    struct linger tmp = { 1 , 0 };
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&tmp,sizeof(tmp));      //端口复用
 
    struct sockaddr_in address;
    bzero(&address, sizeof(address ) );
    address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr );
    address.sin_port = htons(port);
    
    if(-1 == (bind(listenfd, (struct sockaddr*)&address, sizeof(address))))
    {

        LOG_ERROR( "Server Bind Failed!",0);
        Log::get_instance()->flush();
        exit(1);
    }

    if(-1 == listen(listenfd, 50))
    {
        LOG_ERROR("Server Listen Failed! [fd=%d]",listenfd);
        Log::get_instance()->flush();
        exit(1);
    }

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd( epollfd, listenfd, false );
    monitor::m_epollfd = epollfd;
    
    while(true)
    {
        int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER, -1);
        if( (number < 0)&& (errno != EINTR) )
        {
            LOG_ERROR("epoll Failed! [fd=%d]",listenfd);
            Log::get_instance()->flush();
            
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
                    LOG_ERROR(" accept errno [fd=%d]",listenfd);
                    Log::get_instance()->flush();
                    continue;
                }
                users[connfd].init(connfd, client_address);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )
            {
                //如果出现异常
                LOG_ERROR("EPOLLREHUO|EPOLLHUP|EPOLLERR  sockfd =%d",sockfd);
                Log::get_instance()->flush();
                pthread_cancel((users+sockfd)->id);
                close(sockfd);  
            }
            else if(events[i].events & EPOLLIN )
            {
                LOG_INFO("deal with the client(%d)",sockfd);
                Log::get_instance()->flush();
                pool->append( users + sockfd );
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
