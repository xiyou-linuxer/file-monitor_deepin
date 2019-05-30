#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <thread>
#include <mutex>
#include "monitor.h"
#include "threadpool.h"

#include "log/log.h"

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
    std::cout <<"[addfd]ET OFF" <<std::endl;
     
    //one_shot = 0;
    if(one_shot)
    {
        std::cout <<"[addfd]ONESHOT ON" <<std::endl;
        event.events |= EPOLLONESHOT;
    }
    else
    {
        std::cout <<"[addfd]ONESHOT OFF" <<std::endl;
    }
    
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd ,&event );
    //setnonblocking( fd );
    std::cout <<"nonblocking OFF" <<std::endl;
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
//bool monitor::recv_masg()
void monitor::process()
{

     while (1)
     {   
        printf("..[recv_masg]>>While bigan\n");
        int count = 0;
        std::cout << "..[recv_masg]>>sizeof(TAG) :"<< sizeof(TAG) << std::endl;
        count =recv(m_sockfd, masg+count, sizeof(TAG)-count, MSG_WAITALL);
        //count =recv(m_sockfd, masg, sizeof(TAG), 0);
       // strcat(read_buf,masg->data);
        std::cout << "..[recv_masg]>>recv_count = " << count << std::endl;
        if(count==-1)
        {
            //std::cout<< strerror(errno) << std::endl;
            if(errno == EAGAIN || errno == EWOULDBLOCK  )
            {
                std::cout << "errno : " << errno << std::endl;
               // return true;
                break;
            }
            //close_mon();
            // break;
            return ;
        }
        if(count == 0)
        {
            std::cout << "client exit" << std::endl;
            //pthread_cancel(id);
            //close_mon();
            break;
            //return ;
        }
        std::cout << "[recv_masg]>>recv succses!\n" << std::endl;
        std::cout << "[recv_masg]>>masg begin:" << std::endl;
        std::cout << "[recv_masg]data :"<< masg->data << std::endl;
        std::cout << "[recv_masg]event :"<< masg->event << std::endl;
        std::cout << "[recv_masg]filename :"<< masg->file_name << std::endl;
        std::cout << "[recv_masg]lenth :"<< masg->lenth << std::endl;
        std::cout << "[recv_masg]mac_addr :"<< masg->mac_addr << std::endl;
        std::cout << "[recv_masg]sign :"<< masg->sign << std::endl;
        std::cout << "[recv_masg]>>masg end\n" << std::endl;
         if(masg->sign == 0)
        {    
            backup();
        }
        //return 1;   
         else if(masg->sign == 1)
         recover();
        //return 0; 
    }  
       // return true;
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
    // std::mutex mtx;
    // mtx.lock();
    if((fd = open(backup_f_name,O_CREAT/*|O_APPEND*/|O_WRONLY,S_IRUSR|S_IWUSR))==-1)
    { 
       // LOG_ERROR("[backup]open errno!open return -1 |file_name:%s|",backup_f_name);
        perror("open");
    }else{                                
       // LOG_DEBUG("[backup]open success!|file_name:%s|",backup_f_name);
        printf(">>backip_open_success\n");
    }
    // off_t offset = 0;
    // offset = masg->f_sign * 4095 ;
    //printf("left    %d   rigth %d \n",masg->left,masg->right);
    
    lseek(fd,masg->left,SEEK_SET);
    int count =write(fd,masg->data,masg->right-masg->left);
    
    printf(" masglenth = %d\n",masg->lenth);
    printf(">>write_succes\n");
    close(fd);
    //mtx.unlock();
    //strcat(masg->data,"\0");
    // int offset;

    
    // ///*根据标志位，改变文件偏移量直接写入
    // if(masg->f_sign)   //f_sign从0开始
    // std::cout << "[back up]f_sign" <<masg->f_sign << std::endl;
    // offset = masg->f_sign * 4096 + 1 ;
    // std::ofstream out;
    // out.open(backup_f_name,std::ios::app);
    // out.seekp( offset, std::ios::beg);
    // // std::cout << "-------------masg->f_sign ====" << masg->f_sign <<std::endl;
    // // std::cout << "-------------out.tellp()======" << out.tellp() << std::endl;
    // // std::cout << "-------------offset ===========" << offset << std::endl;
    // //..............................*/
    // out << "|------------------------" << masg->f_sign << "----------------START--------|"<< std::endl;
    // out << masg->data;
    // out << "|-------------------------" << masg->f_sign << "---------------END----------|"<< std::endl;

    // out.close();
}
void monitor::recover()
{
    int num = 0;

    memcpy(backup_f_name,masg->mac_addr,MAC_ADD_SIZE);
    strcat(backup_f_name,"/");
    transf_file_name(); //目录内容转换
    //memcpy(backup_f_name,masg->file_name,FILE_NAME_LEN);
    strcat(backup_f_name,masg->file_name);
    std::cout << "@@[recover]backup_file_name:" << backup_f_name << std::endl;

    if((fd = open(backup_f_name,O_RDONLY,S_IRUSR|S_IWUSR))==-1)
    {
        LOG_ERROR("[recover]open errno!open return -1 |file_name:%s|",backup_f_name);
        //perror("open");
    }else{
        LOG_DEBUG("[recover]open success |file_name:&s|",backup_f_name);
        //printf("[recover]recover_open_success\n");
    }
    rebag->sign = '1';
    rebag->count = '0';
    strcpy(rebag->event,masg->event);
    strcpy(rebag->file_name,masg->file_name);
    strcpy(rebag->mac_addr,masg->mac_addr);
    while(true)
    {
        //memset(masg->data,'\0',sizeof(BUFF_SIZE));
       memset(rebag->data,'\0',sizeof(rebag->data));
      //  memset(rebag,'\0',sizeof(RECOVER));
        ////////////////////
    // rebag->sign = '1';
    // rebag->count = '0';
    // strcpy(rebag->event,masg->event);
    // strcpy(rebag->file_name,masg->file_name);
    // strcpy(rebag->mac_addr,masg->mac_addr);

        ////////////////////
        //int count =readn(fd,masg->data,BUFF_SIZE-1); //循环读
        int count =read(fd,rebag->data,BUFF_SIZE - 1); //循环读
        strcat(rebag->data,"\0");

        if(count == 0)
        {
            std::cout << "count ============= 0" << std::endl;
            break;
        }
        recover_file_name();
        //printf("count_write = %d\n masglenth = %d\n",count,masg->lenth);
        
        std::cout << ">>read succses!\n" << std::endl;
        std::cout << "@@[recover]bag:" << std::endl;
        // std::cout << ">>send>>left :"<< masg->left << std::endl;
        // std::cout << ">>send>>right :"<< masg->right << std::endl;
        std::cout << ">>send>>data :"<< rebag->data << std::endl;
        std::cout << ">>send>>event :"<< rebag->event << std::endl;
        std::cout << ">>send>>filename :"<< rebag->file_name << std::endl;
        //std::cout << ">>send>>lenth :"<< masg->lenth << std::endl;
        std::cout << ">>send>>mac_addr :"<< rebag->mac_addr << std::endl;
        std::cout << ">>send>>sign :"<< rebag->sign << std::endl;
        std::cout << "@@[recover]bag end\n" << std::endl;

        //send(m_sockfd,masg,sizeof(TAG),0);
        std::mutex mtx;
        mtx.lock();
        send(m_sockfd,rebag,sizeof(RECOVER),0);
        mtx.unlock();
//        sleep(3);
        
        ++num;
        // masg->lenth = 0;
        // send(m_sockfd,masg,sizeof(TAG),0);
        // if(count == 0)
        // {
        //     std::cout << "count ============= 0" << std::endl;
        //     break;
        // }

    }
    std::cout << "---------------------------num ========  " << num << std::endl;

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
// thread function
void* send_heart(void* arg)
{   
    //pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);    //手动设置停止点
    std::cout << "The heartbeat sending thread started." << std::endl;
    monitor* c = (monitor*)arg;
 //   int count = 0;  // 测试
    while(1) 
    {
        //memset(c->heart,'\0',sizeof(c->heart));   
        // c->heart->left = 0;
        // c->heart->right = 0;
        // c->heart->lenth = 0;
        memset(c->heart->mac_addr,'\0',18);
        memset(c->heart->file_name,'\0',256);
        memset(c->heart->data,'\0',4096);
        memset(c->heart->event,'\0',256);
        c->heart->sign = '4';
        c->heart->count = '0';  
        std::cout << "send_hread fd = " << c->m_sockfd << std::endl;
       // send(c->m_sockfd, c->heart, 4644, 0);
        //c->mtx.lock();
        send(c->m_sockfd, c->heart, c->re_num, 0);
        //c->mtx.unlock();
        std::cout << "send_heart " << std::endl;
        sleep(3);     // 定时3秒
 
//        ++count;      // 测试：发送15次心跳包就停止发送
//        if(count > 15)
//            break;
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