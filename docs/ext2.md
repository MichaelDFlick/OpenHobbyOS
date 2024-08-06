# ext2 Filesystem

OpenHobbyOS includes a read/write ext2 filesystem implementation. This allows the OS to read and write files on a standard ext2 partition.

## Usage

The ext2 filesystem is mounted from the first MBR partition on the boot disk. It requires the disk to be present and the partition to be formatted as ext2.

During boot, after ATA disk detection, the kernel checks for an MBR partition table. If an ext2 partition is found, it is mounted at /.

## Features

- Full ext2 block group descriptor support
- Direct and indirect block addressing (single, double, triple)
- Read and write operations
- File creation and deletion
- Directory creation and removal
- File attribute support (size, timestamps)

## Limitations

- No journaling (ext2, not ext3/ext4)
- No extended attribute support
- Block size must be 1024 bytes
- Maximum file size limited by block addressing scheme

## Integration

The ext2 driver integrates with the VFS layer. When a file is opened, the VFS checks if it exists on the ext2 filesystem and dispatches read/write operations to the ext2 driver.

Files created or modified through the ext2 filesystem persist across reboots when the disk is present.
