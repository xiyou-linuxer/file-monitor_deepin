#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <thread>
#include <mutex>
#include "../monitor/monitor.h"
#include "../RAII/locker.h"
#include "../threadpool/threadpool.h"
#include "../log/log.h"

 #include <sys/file.h>
int setnonblocking(int fd)
{
    int old_option = fcntl(fd ,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL ,new_option);
    return old_option;
}
void addfd( int epollfd,int fd ,bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN  /*| EPOLLET*/   | EPOLLRDHUP;    //去掉了ET

    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    else
    {
        //std::cout <<"[addfd]ONESHOT OFF" <<std::endl;
    }
    
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd ,&event );
}
void removefd(int epollfd, int fd)
{
    epoll_ctl( epollfd, EPOLL_CTL_DEL , fd ,0 );
    close(fd);
}
void modfd(int epollfd, int fd,int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd ,EPOLL_CTL_MOD ,fd ,&event );
}
void monitor::close_mon( bool real_close )
{
    if(real_close && (m_sockfd != -1) )
    {
        removefd( m_epollfd,m_sockfd );
        m_sockfd = -1;
        delete masg; //回收消息体
        delete rebag;
        delete heart;
    }
}

int monitor::m_epollfd = -1;

void monitor::init(int sockfd, const sockaddr_in& addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    addfd( m_epollfd, sockfd, true);


    init();
}
void monitor::init()
{
    std::cout << "[private:init]this is init" << std::endl;

    masg->left = 0;
    masg->right = 0;
    masg->sign = 0;
    masg->lenth = 0;
    memset(masg->mac_addr,'\0',MAC_ADD_SIZE);
    memset(masg->file_name,'\0',FILE_NAME_LEN);
    memset(masg->data,'\0',BUFF_SIZE);
    memset(masg->event,'\0',EVENT_BUFF);
    memset(backup_f_name,'\0',BACKUP_F_NAME);
    std::cout << " sizeof TAG " << sizeof(TAG) << std::endl;
    pthread_t id;
    int ret = pthread_create(&id, NULL, send_heart, (void*)this);
    if(ret != 0)
    {
        std::cout << "Can not create thread!";
        exit(1);
    }  
}
void monitor::process()
{

     while (1)
     {   
        printf("..[recv_masg]>>While bigan\n");
        int count = 0;
        std::cout << "..[recv_masg]>>sizeof(TAG) :"<< sizeof(TAG) << std::endl;
        count =recv(m_sockfd, masg+count, sizeof(TAG)-count, MSG_WAITALL);
        std::cout << "..[recv_masg]>>recv_count = " << count << std::endl;
        if(count==-1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK  )
            {
                std::cout << "errno : " << errno << std::endl;
                break;
            }
            return ;
        }
        if(count == 0)
        {
            std::cout << "client exit" << std::endl;
            break;
        }
         if(masg->sign == 0)
        {    
            backup();
        }
         else if(masg->sign == 1)
         recover();
    }  
}
void monitor::backup()
{   
    pthread_t pth;
    pth =  pthread_self();
    LOG_DEBUG("##[back up]>>PTHREAD ID = %d\n",pth);
    LOG_DEBUG("##[back up]>>IN backup() before memecpy\n",0);
    transf_file_name();
    mkdir(masg->mac_addr,S_IRWXU);
    memcpy(backup_f_name,masg->mac_addr,MAC_ADD_SIZE);
    strcat(backup_f_name,"/");
    strcat(backup_f_name,masg->file_name);
    printf("##[back up]>>IN backup before open()\n");
    if((fd = open(backup_f_name,O_CREAT/*|O_APPEND*/|O_WRONLY,S_IRUSR|S_IWUSR))==-1)
    { 
        LOG_ERROR("[backup]open errno!open return -1 |file_name:%s|",backup_f_name);
        perror("open");
    }else{                                
        LOG_DEBUG("[backup]open success!|file_name:%s|",backup_f_name);
        printf(">>backip_open_success\n");
    }
    
    lseek(fd,masg->left,SEEK_SET);
    int count =write(fd,masg->data,masg->right-masg->left);
    
    printf(" masglenth = %d\n",masg->lenth);
    printf(">>write_succes\n");
    close(fd);
}
void monitor::recover()
{
    int num = 0;

    memcpy(backup_f_name,masg->mac_addr,MAC_ADD_SIZE);
    strcat(backup_f_name,"/");
    transf_file_name(); //目录内容转换
    strcat(backup_f_name,masg->file_name);
    std::cout << "@@[recover]backup_file_name:" << backup_f_name << std::endl;

    if((fd = open(backup_f_name,O_RDONLY,S_IRUSR|S_IWUSR))==-1)
    {
        LOG_ERROR("[recover]open errno!open return -1 |file_name:%s|",backup_f_name);
    }else{
        LOG_DEBUG("[recover]open success |file_name:&s|",backup_f_name);
    }
    rebag->sign = '1';
    rebag->count = '0';
    strcpy(rebag->event,masg->event);
    strcpy(rebag->file_name,masg->file_name);
    strcpy(rebag->mac_addr,masg->mac_addr);
    while(true)
    {
        memset(rebag->data,'\0',sizeof(rebag->data));
        int count =read(fd,rebag->data,BUFF_SIZE - 1); //循环读
        strcat(rebag->data,"\0");

        if(count == 0)
        {
            break;
        }
        recover_file_name();
        
        std::mutex mtx;
        mtx.lock();
        send(m_sockfd,rebag,sizeof(RECOVER),0);
        mtx.unlock();
    }

    close(fd);
}
void monitor::transf_file_name()
{
    for(auto &c : masg->file_name)
    {
        if(c == '/')
        {
            c = '@';
        }
    }
}
void monitor::recover_file_name()
{
    for(auto &c : rebag->file_name)
    {
        if(c == '@')
        {
            c = '/';
        }
    }
}
unsigned long monitor::get_file_size()
{
    unsigned long filesize = -1;
    struct stat statbuff;
    if (stat(backup_f_name, &statbuff) < 0)
    {
        return filesize;
    }
    else
    {
        filesize = statbuff.st_size;
    }
    return filesize;
}
void* send_heart(void* arg)
{   
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);    //手动设置停止点
    std::cout << "The heartbeat sending thread started." << std::endl;
    monitor* c = (monitor*)arg;
    while(1) 
    {
        memset(c->heart->mac_addr,'\0',18);
        memset(c->heart->file_name,'\0',256);
        memset(c->heart->data,'\0',4096);
        memset(c->heart->event,'\0',256);
        c->heart->sign = '4';
        c->heart->count = '0';  
        send(c->m_sockfd, c->heart, c->re_num, 0);
        sleep(3);     // 定时3秒
    }
}
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