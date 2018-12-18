# Custom FindThreads for OpenHobbyOS cross-compilation.
# The toolchain provides pthreads via newlib's libc, so we
# skip cmake's try_compile-based detection and just set the
# standard variables and imported target directly.

if(Threads_FOUND)
    return()
endif()

set(Threads_FOUND TRUE)

set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "Thread library" FORCE)
set(CMAKE_HAVE_THREADS_LIBRARY 1 CACHE INTERNAL "Threads detected" FORCE)
set(CMAKE_USE_PTHREADS_INIT 1 CACHE INTERNAL "Using pthreads" FORCE)

if(NOT TARGET Threads::Threads)
    add_library(Threads::Threads INTERFACE IMPORTED)
    set_target_properties(Threads::Threads PROPERTIES
        INTERFACE_COMPILE_OPTIONS ""
        INTERFACE_LINK_LIBRARIES ""
    )
endif()
