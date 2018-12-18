extern void _Unwind_RaiseException(void);
extern void _Unwind_Resume(void);
extern void _Unwind_DeleteException(void);
extern void _Unwind_GetIPInfo(void);
extern void _Unwind_GetRegionStart(void);
extern void _Unwind_GetLanguageSpecificData(void);
extern void _Unwind_GetTextRelBase(void);
extern void _Unwind_GetDataRelBase(void);
extern void _Unwind_SetGR(void);
extern void _Unwind_SetIP(void);
extern void _Unwind_Resume_or_Rethrow(void);
extern void __divdi3(void);
extern void __moddi3(void);
extern void __udivdi3(void);
extern void __umoddi3(void);
extern void __udivmoddi4(void);
extern void __divmoddi4(void);
extern void __popcountdi2(void);
extern void __popcountsi2(void);
extern void __ctzdi2(void);

void *volatile __force_libgcc_refs[] = {
    (void *)_Unwind_RaiseException,
    (void *)_Unwind_Resume,
    (void *)_Unwind_DeleteException,
    (void *)_Unwind_GetIPInfo,
    (void *)_Unwind_GetRegionStart,
    (void *)_Unwind_GetLanguageSpecificData,
    (void *)_Unwind_GetTextRelBase,
    (void *)_Unwind_GetDataRelBase,
    (void *)_Unwind_SetGR,
    (void *)_Unwind_SetIP,
    (void *)_Unwind_Resume_or_Rethrow,
    (void *)__divdi3,
    (void *)__moddi3,
    (void *)__udivdi3,
    (void *)__umoddi3,
    (void *)__udivmoddi4,
    (void *)__divmoddi4,
    (void *)__popcountdi2,
    (void *)__popcountsi2,
    (void *)__ctzdi2,
};
