#include "main.hpp"
#include <stdio.h>
Inotify main_important ;

struct filename_fd_desc FileArray[main_important.array_length] ;
struct epoll_event Epollarray[main_important.epoll_number] ;
int array_index = 0;
const char * base_dir;
static int handle_events(int epollfd,int fd,int argc,char * argv[])
{
    char buf[2048];
    int i;
    ssize_t len;
    char * ptr;
    const struct inotify_event *event;
    memset(buf,0,sizeof(buf));

    len = read(fd,buf,sizeof(buf));

    for(ptr = buf ; ptr < buf + len;ptr += sizeof(struct inotify_event) + event->len)
    {
        event = (const struct inotify_event *) ptr;
        if(event->len)
        {
            if(event->mask & IN_OPEN)
            {
                cout << "OPEN file    " << event->name << endl;
            }
            if(event->mask & IN_CLOSE_NOWRITE)
            {
                cout << "CLOSE_NOWRITE   " << event->name << endl;
            }
            if(event->mask & IN_CLOSE_WRITE)
                cout << "IN_CLOSE_WRITE   " <<event->name <<endl;
            if (event->mask & IN_CREATE)
            { /* 如果是创建文件则打印文件名 */
                cout<< "ggg " << endl;
                sprintf(FileArray[array_index].name,"%s",event->name);
                sprintf(FileArray[array_index].base_name,"%s%s",base_dir,event->name);
                cout << "ppppp" << endl;
                int temp_fd = open(FileArray[array_index].base_name,O_RDWR);
                if(temp_fd == -1)
                {
                    return -1;
                }
               // cout << "create file" << endl;
                FileArray[array_index].fd =temp_fd;
                main_important.addfd(epollfd,temp_fd,false);
                array_index++;
                cout << "add file   " << event->name<< endl;
            }
            if (event->mask & IN_DELETE)
            { /* 如果是删除文件也是打印文件名 */
                for (i = 0; i < main_important.array_length; i++)
                {
                    if (!strcmp(FileArray[i].name, event->name))
                    {
                        main_important.rm_fd(epollfd, FileArray[i].fd);
                        FileArray[i].fd = 0;
                        memset(FileArray[i].name, 0, sizeof(FileArray[i].name));
                        memset(FileArray[i].base_name, 0, sizeof(FileArray[i].base_name));
                        printf("delete file to epoll %s\n", event->name);
                        break;
                    }
                }
                    cout << "delete file   " << event->name << endl;
            }
        }
    }
}
int main(int argc , char** argv)
{
    int fd,i,epollfd,wd;
    char readbuf[1024];
    epollfd =epoll_create(8);
    
    fd = inotify_init();
    base_dir = argv[1];
    if(argc < 2)
    {
        cout << "error argc too simple" << endl;
        return 1;
    }

    for(i = 1 ; i < argc ;i++)
    {
        wd = inotify_add_watch(fd,argv[i],IN_OPEN |IN_CLOSE |IN_CREATE | IN_DELETE);
    }
    main_important.addfd(epollfd,fd,false);
    while(1)
    {
        int ret = epoll_wait(epollfd,Epollarray,32,-1);
        // cout << ret << endl;
        for( i = 0; i<ret ;i++)
        {
            if(Epollarray[i].data.fd == fd)
            {
                if (-1 == (handle_events(epollfd,fd,argc,argv)))
                {
                    return -1;
                }
            }
            else
            {
               int  readlen = read(Epollarray[i].data.fd,readbuf,1024);
               readbuf[readlen] = '\0';
               //cout << readbuf[readlen] << endl;
            }
        }
    }
    close(fd);
    return 0;
}