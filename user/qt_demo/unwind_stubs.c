typedef unsigned int uintptr_t;
typedef long time_t;

struct _Unwind_Exception { unsigned long long exception_class; void *exception_cleanup; unsigned private_1, private_2; };
struct _Unwind_Context { unsigned opaque; };

void _Unwind_Resume(struct _Unwind_Exception *e) { while (1) __asm__("hlt"); }
void _Unwind_DeleteException(struct _Unwind_Exception *e) {}
uintptr_t _Unwind_GetLanguageSpecificData(struct _Unwind_Context *c) { return 0; }
uintptr_t _Unwind_GetRegionStart(struct _Unwind_Context *c) { return 0; }
void _Unwind_SetGR(struct _Unwind_Context *c, int r, uintptr_t v) {}
void _Unwind_SetIP(struct _Unwind_Context *c, uintptr_t v) {}
uintptr_t _Unwind_GetIPInfo(struct _Unwind_Context *c, int *e) { *e = 0; return 0; }
uintptr_t _Unwind_GetDataRelBase(struct _Unwind_Context *c) { return 0; }
uintptr_t _Unwind_GetTextRelBase(struct _Unwind_Context *c) { return 0; }
uintptr_t _Unwind_RaiseException(struct _Unwind_Exception *e) { return 0; }
uintptr_t _Unwind_Resume_or_Rethrow(struct _Unwind_Exception *e) { return 0; }
uintptr_t _Unwind_GetGR(struct _Unwind_Context *c, int r) { return 0; }
uintptr_t _Unwind_GetCFA(struct _Unwind_Context *c) { return 0; }

long pathconf(const char *path, int name) { return -1; }
