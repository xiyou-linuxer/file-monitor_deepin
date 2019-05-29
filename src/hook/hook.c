#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

char  SHM_NAME[1024];
#define SHM_NAME_SEM "/memmap_sem"
#define FILE_PATH "/etc/file_watch/init.conf"
char inotify_name[1024];
int per_flag = 0;
struct msgmbuf
{
    long long  mtype;
    char mtext[1024];
};
char filename_path[200];
typedef int (*orig_open_f_type)(const char *pathname, int flags,...);
typedef int (*orig_close_f_type)(int fd);
typedef int (*orig_read_f_type)(int fildes, void *buf, size_t nbyte);
typedef int (*orig_readlink)(const char *restrict path, char *restrict buf,size_t bufsize);
typedef int (*orig_f_ftruncate)(int fd, off_t length);

int writen(int fd, const void *vptr, int n);
int readn(int fd, void *vptr, int n);
void get_path() {

  orig_open_f_type orig_open;
  orig_open = (orig_open_f_type)dlsym(RTLD_NEXT, "open");

  //打开配置文件
  int fd = orig_open(FILE_PATH, O_RDWR);
  //读取配置文件
  char c[200];
  bzero(c, sizeof(c));
  readn(fd, c, sizeof(c));
  
  const int j = strlen(c);

  for (int i = 0; i < j; i++) {
    if (c[i] == '\n')
      c[i] = 0;
  }
  for (int i = 0; i < j - 1; i++) {
    if (strncmp(&c[i], "path:", 5) == 0) {
      strcpy(filename_path, &c[i + 5]);
      continue;
    }
  }
  //关闭配置文件
 
  orig_close_f_type orig_close;
  orig_close = (orig_close_f_type)dlsym(RTLD_NEXT, "close");
  orig_close(fd);
}

unsigned long get_file_size(const char *path) {
  unsigned long filesize = -1;
  struct stat statbuff;
  if (stat(path, &statbuff) < 0) {
    return filesize;
  } else {
    filesize = statbuff.st_size;
  }
  return filesize;
}
void set_map(const char *pathname)
{

    int temp = 0;
    int g = 0;
    for(g = strlen(pathname);g >= 0;g--)
    {
        if (pathname[g] == '/')
        {
            break;
        }
    }
    for( ;g <strlen(pathname);g++)
    {
        SHM_NAME[temp] = pathname[g];
        temp++;
    }

    int fd;
    sem_t *sem;
    
    mode_t old_umask = umask(0);
    fd = shm_open(SHM_NAME, O_RDWR | O_CREAT , 0666);
    sem = sem_open(SHM_NAME_SEM, O_RDWR | O_CREAT, 0666, 0);

    umask(old_umask);
    if (fd < 0 || sem == SEM_FAILED)
    {
        perror("shm_open or sem_open failed...\n");
        perror(strerror(errno));
        return ;
    }
    orig_f_ftruncate orig_ftruncate;
    orig_ftruncate = (orig_f_ftruncate)dlsym(RTLD_NEXT, "ftruncate");
    int ftr_fd = orig_ftruncate(fd, 1024 * 1024 * 1024);
    if (ftr_fd == -1)
    {
        perror(strerror(errno));
    }
    char *memPtr;
    memPtr = (char *)mmap(NULL,1024 * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED , fd, 0);
    if (memPtr == (char *)-1)
    {
        perror(strerror(errno));
    }
    orig_open_f_type orig_open;
    orig_open = (orig_open_f_type)dlsym(RTLD_NEXT, "open");
    
    char buf[1024];
    int test_fd = orig_open(pathname, O_RDONLY);
    unsigned long long number = get_file_size(pathname);
    unsigned long long numbern = 0;
    unsigned long long number_temp = 0;
    memset(buf,'\0',sizeof(buf));

    while (numbern != number)
    {
        memset(buf,'\0',sizeof(buf));
        number_temp = readn(test_fd,buf,sizeof(buf));
        numbern += number_temp;
        strcat(buf,"\0");
        if (memmove(memPtr ,buf,sizeof(buf)) == (void *)-1)
        {
            perror(strerror(errno));
        }
        memPtr += number_temp;
    }
    sem_post(sem);
    sem_close(sem);
}
void send_link(const char * pathname,int t)
{
    int msg_id, msg_flags;
    struct msqid_ds msg_info;
    struct msgmbuf msg_mbuf;
    key_t key = t;
    msg_flags = IPC_CREAT;
    msg_mbuf.mtype = 0;
    memset(msg_mbuf.mtext, 0, sizeof(msg_mbuf.mtext));
    strcpy(msg_mbuf.mtext,pathname);
    msg_id = msgget(key, msg_flags | 0666);
    if (-1 == msg_id)
    {
        return ;
    }
    msg_mbuf.mtype = get_file_size(pathname);
    msgsnd(msg_id, (void*)&msg_mbuf, 1024, IPC_NOWAIT);
}
int recv_keep_valie()
{
    int ret = -1;
    int msg_flags, msg_id;
    int flag = 0;
    key_t key;
    struct msgmbuf msg_mbuf;
    memset(msg_mbuf.mtext, '\0', sizeof(msg_mbuf.mtext));
    key = 1000;
    msg_flags = IPC_CREAT;
    msg_id = msgget(key, msg_flags | 0666);
    /*创建一个消息队列 */
    if (-1 == msg_id)
    {
        return -1;
    }
    ret = msgrcv(msg_id, (void *)&msg_mbuf, 1024, 0, IPC_NOWAIT);
    if (-1 == ret)
    {
        return -1;
    }
    if (strcmp(msg_mbuf.mtext, "1") == 0)
    {
        flag = msgctl(msg_id, IPC_RMID, NULL);
        return 1;
    }
    if(strcmp(msg_mbuf.mtext,"0") == 0)
    {
        flag = msgctl(msg_id, IPC_RMID, NULL);
        return 0;
    }
    return -1;
}

int open(const char *pathname, int flags, ...)
{
    if(recv_keep_valie() == 0)
    {
        perror("error your no permission\n");
        return -1;
    }
    /* Some evil injected code goes here. */
    int res = 0;
    char resolved_path[128];
    va_list ap; //可变参数列表
    va_start(ap, flags);
    mode_t third_agrs = va_arg(ap, mode_t);
    if (third_agrs >= 0 && third_agrs <= 0777)
    {
       per_flag = 1;
    }
    va_end(ap);
    memset(filename_path,'\0',sizeof(filename_path));
    memset(resolved_path,'\0',sizeof(resolved_path));
    realpath(pathname,resolved_path);
    orig_open_f_type orig_open;
    orig_open = (orig_open_f_type)dlsym(RTLD_NEXT, "open");
    get_path();
    if (strncmp(filename_path, resolved_path,strlen(filename_path)-1) == 0)
    {
      if (per_flag == 0)
      res = orig_open(resolved_path, flags);
      else
      res = orig_open(resolved_path, flags, third_agrs);
      if (res > 0) 
      {
        char temp_buf[1024] ;
        char file_path[1024] ; // PATH_MAX in limits.h
        memset(file_path, '\0', sizeof(file_path));
        memset(temp_buf, '\0', sizeof(temp_buf));
        snprintf(temp_buf, sizeof(temp_buf), "/proc/self/fd/%d", res);
        orig_readlink orig_read_link;
        orig_read_link = (orig_readlink)dlsym(RTLD_NEXT, "readlink");
        orig_read_link(temp_buf, file_path, sizeof(file_path) - 1);
        send_link(file_path,1024);
        set_map(resolved_path);
        orig_close_f_type orig_close;
        orig_close = (orig_close_f_type)dlsym(RTLD_NEXT, "close");
        orig_close(res);
        FILE *fp = fopen(pathname, "w");
        char buf[20];
        strcpy(buf, "It is a secret");
        strcat(buf, "\0");
        fwrite(buf, strlen(buf), 1, fp);
        fclose(fp);
        if (per_flag == 0)
            res = orig_open(resolved_path, flags);
        else
            res = orig_open(resolved_path, flags, third_agrs);
      }
    }
    else
    {
        if (per_flag == 0)
            return orig_open(resolved_path, flags);
        else
            return orig_open(resolved_path, flags, third_agrs);
   }
   return res;
}

int close(int fd)
{ 
    if(recv_keep_valie() == 0 || fd == -1)
    {
        perror("error your no permission\n");
        return -1;
    }
    char temp_buf[1024] = {'\0'};
    char file_path[1024] = {'0'}; // PATH_MAX in limits.h
    snprintf(temp_buf, sizeof(temp_buf), "/proc/self/fd/%d", fd);
    orig_readlink orig_read_link;
    orig_read_link = (orig_readlink)dlsym(RTLD_NEXT, "readlink");
    int res = orig_read_link(temp_buf, file_path, sizeof(file_path) - 1);
    get_path();
    if (strncmp(filename_path, file_path, strlen(filename_path)-1) == 0 && fd > 2)
        send_link(file_path,1025);
    orig_close_f_type orig_close;
    orig_close = (orig_close_f_type)dlsym(RTLD_NEXT, "close");
    orig_close(fd);
}

int readn(int fd, void *vptr, int n)
{
    orig_read_f_type orig_read;
    orig_read = (orig_read_f_type)dlsym(RTLD_NEXT, "read");
    int nleft;
    int nread;
    char *ptr;

    ptr = (char *)vptr;
    nleft = n;

    while (nleft > 0)
    {
        nread = orig_read(fd, ptr, nleft);
        if (nread < 0)
        {
            if (errno == EINTR)
                nread = 0; 
            else
                return -1;
        }
        else if (nread == 0)
        {
            break; 
        }

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft);
}

