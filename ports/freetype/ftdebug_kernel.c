#include <freetype/freetype.h>
#include <freetype/internal/ftdebug.h>

#include <stdarg.h>
#include <serial.h>
#include <format.h>

#define FT_TRACE_MAX_COMPONENTS  64

static int  ft_trace_levels_array[FT_TRACE_MAX_COMPONENTS];
int*        ft_trace_levels = ft_trace_levels_array;

FT_BASE_DEF( void )
FT_Trace_Disable( void )
{
}

FT_BASE_DEF( void )
FT_Trace_Enable( void )
{
}

FT_BASE_DEF( void )
ft_debug_init( void )
{
    int  i;
    for ( i = 0; i < FT_TRACE_MAX_COMPONENTS; i++ )
        ft_trace_levels[i] = 3;
}
