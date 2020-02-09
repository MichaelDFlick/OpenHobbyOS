#include "mbr.h"

#include "blkdev.h"
#include "console.h"
#include "memory.h"
#include "string.h"

bool mbr_read(u32 blkdev_id, mbr_partition_info_t *partitions) {
    if (!partitions) {
        return false;
    }
    
    memset(partitions, 0, sizeof(mbr_partition_info_t) * MBR_PARTITION_COUNT);
    
    if (!blkdev_present(blkdev_id)) {
        return false;
    }
    
    mbr_t mbr;
    if (blkdev_read(blkdev_id, 0, 1, &mbr) != 0) {
        return false;
    }
    
    if (mbr.signature != MBR_BOOT_SIGNATURE) {
        return false;
    }
    
    for (int i = 0; i < MBR_PARTITION_COUNT; i++) {
        mbr_partition_t *p = &mbr.partitions[i];
        
        if (p->type == MBR_TYPE_EMPTY) {
            continue;
        }
        
        partitions[i].present = true;
        partitions[i].type = p->type;
        partitions[i].lba_start = p->lba_start;
        partitions[i].sector_count = p->sector_count;
    }
    
    return true;
}

bool mbr_write(u32 blkdev_id, const mbr_partition_t *partitions) {
    if (!blkdev_present(blkdev_id) || !partitions) {
        return false;
    }

    mbr_t mbr;
    memset(&mbr, 0, sizeof(mbr));
    mbr.signature = MBR_BOOT_SIGNATURE;
    memcpy(mbr.partitions, partitions, sizeof(mbr_partition_t) * MBR_PARTITION_COUNT);

    return blkdev_write(blkdev_id, 0, 1, &mbr) == 0;
}

bool mbr_is_valid(u32 blkdev_id) {
    if (!blkdev_present(blkdev_id)) {
        return false;
    }
    
    u8 sector[512];
    if (blkdev_read(blkdev_id, 0, 1, sector) != 0) {
        return false;
    }
    
    u16 signature = sector[510] | (sector[511] << 8);
    return signature == MBR_BOOT_SIGNATURE;
}
