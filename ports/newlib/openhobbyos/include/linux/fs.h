#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <sys/ioctl.h>

#define BLKROSET   _IO(0x12, 93)
#define BLKROGET   _IO(0x12, 94)
#define BLKRRPART  _IO(0x12, 95)
#define BLKGETSIZE _IO(0x12, 96)
#define BLKFLSBUF  _IO(0x12, 97)
#define BLKRASET   _IO(0x12, 98)
#define BLKRAGET   _IO(0x12, 99)
#define BLKFRASET  _IO(0x12, 100)
#define BLKFRAGET  _IO(0x12, 101)
#define BLKSECTSET _IO(0x12, 102)
#define BLKSECTGET _IO(0x12, 103)
#define BLKSSZGET  _IO(0x12, 104)
#define BLKBSZGET  _IOR(0x12, 112, unsigned long long)
#define BLKBSZSET  _IOW(0x12, 113, unsigned long long)
#define BLKGETSIZE64 _IOR(0x12, 114, unsigned long long)

#endif
