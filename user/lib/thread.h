#ifndef OHOS_USER_THREAD_H
#define OHOS_USER_THREAD_H

typedef struct {
    unsigned int stack_size;
    unsigned int guard_size;
    unsigned int priority;
} thread_attr_t;

#endif
