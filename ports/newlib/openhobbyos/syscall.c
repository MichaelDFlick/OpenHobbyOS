#include <sys/syscall.h>
#include <stdarg.h>

long syscall(long number, ...)
{
    va_list ap;
    long a1, a2, a3, a4, a5, a6;
    long result;

    va_start(ap, number);
    a1 = va_arg(ap, long);
    a2 = va_arg(ap, long);
    a3 = va_arg(ap, long);
    a4 = va_arg(ap, long);
    a5 = va_arg(ap, long);
    a6 = va_arg(ap, long);
    va_end(ap);

    __asm__ volatile ("push %%ebp\n"
                      "mov %7, %%ebp\n"
                      "int $0x80\n"
                      "pop %%ebp\n"
                      : "=a"(result)
                      : "a"((int)number), "b"((int)a1), "c"((int)a2),
                        "d"((int)a3), "S"((int)a4), "D"((int)a5),
                        "r"((int)a6)
                      : "memory", "cc");
    return result;
}
