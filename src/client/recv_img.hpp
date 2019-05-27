#ifndef _RECV_IMG_HPP
#define _RECV_IMG_HPP
/* code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
struct msgmbuf {
  int mtype;
  char mtext[1024];
};


#endif //_RECV_IMG.HPP
