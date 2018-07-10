#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H

#define major(dev) ((int)(((unsigned int)(dev) >> 8) & 0xff))
#define minor(dev) ((int)((dev) & 0xff))
#define makedev(maj, min) ((dev_t)(((maj) & 0xff) << 8 | ((min) & 0xff)))

#endif
