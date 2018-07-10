#include <sys/mman.h>
#include <unistd.h>

void *mmap64(void *addr, size_t length, int prot, int flags, int fd, long long offset)
{
    return mmap(addr, length, prot, flags, fd, (off_t)offset);
}
