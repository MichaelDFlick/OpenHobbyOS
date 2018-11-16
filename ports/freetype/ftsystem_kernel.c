#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftdebug.h>
#include <freetype/internal/ftstream.h>
#include <freetype/ftsystem.h>
#include <freetype/fterrors.h>
#include <freetype/fttypes.h>
#include <stdlib.h>
#include <string.h>

FT_CALLBACK_DEF( void* )
ft_alloc( FT_Memory  memory,
          long       size )
{
    FT_UNUSED( memory );
    return ft_smalloc( (size_t)size );
}

FT_CALLBACK_DEF( void* )
ft_realloc( FT_Memory  memory,
            long       cur_size,
            long       new_size,
            void*      block )
{
    FT_UNUSED( memory );
    if (!block) return ft_smalloc((size_t)new_size);
    if (!new_size) { ft_sfree(block); return NULL; }
    void *n = ft_smalloc((size_t)new_size);
    if (n) {
        size_t copy = (size_t)cur_size < (size_t)new_size ? (size_t)cur_size : (size_t)new_size;
        memcpy(n, block, copy);
        ft_sfree(block);
    }
    return n;
}

FT_CALLBACK_DEF( void )
ft_free( FT_Memory  memory,
         void*      block )
{
    FT_UNUSED( memory );
    ft_sfree( block );
}

FT_BASE_DEF( FT_Memory )
FT_New_Memory( void )
{
    FT_Memory  mem;

    mem = (FT_Memory)malloc( sizeof ( *mem ) );
    if ( mem )
    {
        mem->user    = NULL;
        mem->alloc   = ft_alloc;
        mem->realloc = ft_realloc;
        mem->free    = ft_free;
    }

    return mem;
}

FT_BASE_DEF( void )
FT_Done_Memory( FT_Memory  memory )
{
    free( memory );
}

FT_BASE_DEF( FT_Error )
FT_Stream_Open( FT_Stream    stream,
                const char*  filepathname )
{
    FT_UNUSED( stream );
    FT_UNUSED( filepathname );
    return FT_THROW( Cannot_Open_Resource );
}
