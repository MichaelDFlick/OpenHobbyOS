/* Kernel build: disable mbedtls features that need FILE/printf */
#undef MBEDTLS_FS_IO
#undef MBEDTLS_PLATFORM_C
