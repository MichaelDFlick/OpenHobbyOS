# Virtual Filesystem

OpenHobbyOS has a virtual filesystem layer that provides a unified interface for accessing different filesystem types. All file operations go through the VFS layer which dispatches to the appropriate filesystem implementation.

## VFS Node Types

- Regular files (backed by initrd or ext2)
- Directories
- Block devices (/dev/sda, /dev/fb0, etc.)
- Character devices (/dev/null, /dev/tty, /dev/keyboard, /dev/mouse)
- Virtual filesystems (/proc/uptime, /proc/meminfo, /proc/cpuinfo, /proc/loadavg, /proc/mounts)

## Path Resolution

The VFS resolves paths starting from the root directory. Path resolution handles:

- Absolute paths (starting with /)
- Relative paths (relative to current working directory)
- Dot (.) and dot-dot (..) components
- Symlinks (basic support)

## File Operations

The VFS provides standard file operations:

- open, close, read, write, lseek
- stat, fstat, access
- mkdir, rmdir, link, unlink, rename
- chmod, chown
- dup, dup2, fcntl, ioctl
- getdents (directory listing)

## Initrd Overlay

The initrd serves as an overlay filesystem. Files in the initrd are available at boot without needing a disk. The VFS checks the initrd first, then falls back to ext2 if the file is not found in the initrd.

## Device Files

Device files provide access to hardware through the VFS:

- /dev/null - Discards all writes, returns EOF on read
- /dev/tty - Current controlling terminal
- /dev/fb0 - Framebuffer device (mappable)
- /dev/keyboard - PS/2 keyboard input
- /dev/mouse - PS/2 mouse input
- /dev/net - Network device

## Proc Files

The /proc virtual filesystem provides system information:

- /proc/uptime - System uptime
- /proc/loadavg - Load average
- /proc/meminfo - Memory statistics
- /proc/cpuinfo - CPU information
- /proc/mounts - Mounted filesystems
- /proc/net/route - Network routing table
