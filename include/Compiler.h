#ifndef EFERGY_COMPILER_H
#define EFERGY_COMPILER_H
/* XC8 C99 adaptation for Microchip TCP/IP 5.42's PICC18 code paths. */
#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __18CXX 1
#define HI_TECH_C 1
#define COMPILER_HITECH_PICC18 1
#define PTR_BASE unsigned short
#define ROM_PTR_BASE unsigned long
#define ROM const
#define rom
#define far
#define FAR
#define memcmppgm2ram(a,b,c) memcmp(a,b,c)
#define strcmppgm2ram(a,b) strcmp(a,b)
#define memcpypgm2ram(a,b,c) memcpy(a,b,c)
#define strcpypgm2ram(a,b) strcpy(a,b)
#define strncpypgm2ram(a,b,c) strncpy(a,b,c)
#define strstrrampgm(a,b) strstr(a,b)
#define strlenpgm(a) strlen(a)
#define strchrpgm(a,b) strchr(a,b)
#define strcatpgm2ram(a,b) strcat(a,b)
#define Nop() __nop()
#endif
