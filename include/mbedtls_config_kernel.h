/* Standalone minimal config for kernel builds — defines only what
 * the kernel actually needs (AES-XTS, PBKDF2-SHA256), nothing more. */

/* Platform */
#define MBEDTLS_HAVE_ASM

/* Features the kernel needs */
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_XTS
#define MBEDTLS_MD_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_HKDF_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_ERROR_C
#define MBEDTLS_ERROR_STRERROR_DUMMY

/* Provide our own mbedtls_platform_zeroize (kernel has no explicit_bzero) */
#define MBEDTLS_PLATFORM_ZEROIZE_ALT
