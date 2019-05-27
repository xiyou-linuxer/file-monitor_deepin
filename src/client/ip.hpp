#pragma once
#ifndef IP_HPP
#define IP_HPP
/* code */
#include <net/if.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
 using namespace std;
 class IP
 {
 public:
	 char real_mac[20];
	 char real_ip[30];

	 IP()
	 {
		 memset(real_mac, 0, sizeof(real_mac));
		 memset(real_ip, 0, sizeof(real_ip));
	 }
	 ~IP()
	 {
		 memset(real_mac, 0, sizeof(real_mac));
		 memset(real_ip, 0, sizeof(real_ip));
	 }

 public:
	 int getLocalInfo()
	 {
		 int fd;
		 int interfaceNum = 0;
		 struct ifreq buf[16];
		 struct ifconf ifc;
		 struct ifreq ifrcopy;
		 char mac[20] = {0};
		 char ip[32] = {0};
		 char broadAddr[32] = {0};
		 char subnetMask[32] = {0};
		 if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		 {
			 perror("socket");

			 close(fd);
			 return -1;
		 }
		 ifc.ifc_len = sizeof(buf);
		 ifc.ifc_buf = (caddr_t)buf;
		 if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
		 {
			 interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
			 while (interfaceNum-- > 0)
			 {
				 //ignore the interface that not up or not runing
				 ifrcopy = buf[interfaceNum];
				 if (ioctl(fd, SIOCGIFFLAGS, &ifrcopy))
				 {
					 printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);

					 close(fd);
					 return -1;
				 }
				 //get the mac of this interface
				 if (!ioctl(fd, SIOCGIFHWADDR, (char *)(&buf[interfaceNum])))
				 {
					 memset(mac, 0, sizeof(mac));
					 snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
							  (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[0],
							  (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[1],
							  (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[2],

							  (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[3],
							  (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[4],
							  (unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[5]);
					
					 if (strcmp(mac, "00:00:00:00:00:00") != 0)
					 {
						strcpy(real_mac, mac);
						printf("device mac: %s\n", mac);
					 }
				 }
				 //get the IP of this interface
				 if (!ioctl(fd, SIOCGIFADDR, (char *)&buf[interfaceNum]))
				 {
					 snprintf(ip, sizeof(ip), "%s",
							  (char *)inet_ntoa(((struct sockaddr_in *)&(buf[interfaceNum].ifr_addr))->sin_addr));
					 if (strcmp(ip, "127.0.0.1") != 0)
					{
						strcpy(real_ip, ip);
						printf("device ip: %s\n", ip);
					}
				 }
			 }
		 }
		 close(fd);
		 return 0;
	 }
};
#endif // IP_HPP
