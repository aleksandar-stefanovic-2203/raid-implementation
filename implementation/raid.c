#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

static int diskWorking[DISKS+1];
static struct sleeplock* diskLock[DISKS+1];

int init_raid(int raid){
	if(raid < 0 || raid > 4) return -2;
	if(DISKS <= 1) return -3;
	if((raid == 1 && DISKS % 2 != 0) ||
	   (raid == 2 && (DISKS < 4 || DISKS % 2 != 0)))
	{
		return -4;
	}
	struct buf* b = bread(VIRTIO0_ID, 0);
	int isInit = b->data[0];
	if(isInit == 1) {
		brelse(b);
		return -1;
	}
	b->data[0] = 1;
	b->data[1] = raid;
	bwrite(b);
	brelse(b);
	for(int i = 1; i <= DISKS; i++) {
        diskWorking[i] = 1;
        diskLock[i] = kalloc();
        initsleeplock(diskLock[i], "");
    }
    return 0;
}

int disk_fail_raid(int diskNo){
	if(diskNo < 1 || diskNo > DISKS) return -2;
	struct buf* b = bread(VIRTIO0_ID, 0);
	int isInit = b->data[0];
	brelse(b);
	if(isInit == 0) return -1;

    acquiresleep(diskLock[diskNo]);

	diskWorking[diskNo] = 0;
	int block_number_per_disk = DISK_SIZE / BSIZE;
	uchar* data = kalloc();
	for(int i = 0; i < block_number_per_disk; i++){
		write_block(diskNo, i, data);
	}
	kfree(data);

    releasesleep(diskLock[diskNo]);

	return 0;
}

int restore_data(int raid, int diskNo) {
    uchar* data, *buf;
    int block_number_per_disk = DISK_SIZE / BSIZE;
	switch(raid) {
		case 0:
		return 0;

		case 1: case 2:
		int diskPairNo = (diskNo - 1 + (DISKS / 2)) % DISKS + 1;
		if(diskWorking[diskPairNo] == 0) return -2;
		data = kalloc();
		for(int i = 0; i < block_number_per_disk; i++){
			read_block(diskPairNo, i, data);
			write_block(diskNo, i, data);
		}
		kfree(data);

		return 0;

        case 3: case 4:
        data = kalloc();
        for(int i = 0; i < BSIZE; i++) {
            data[i] = 0;
        }
        buf = kalloc();
        for(int k = 0; k < block_number_per_disk; k++){
            for(int j = 1; j <= DISKS; j++){
                if(j == diskNo) continue;
                read_block(j, k, buf);
                for(int i = 0; i < BSIZE; i++) {
                    data[i] ^= buf[i];
                }
            }
            write_block(diskNo, k, data);
            for(int i = 0; i < BSIZE; i++) {
                data[i] = 0;
            }
        }
        kfree(buf);
        kfree(data);
        return 0;

		default:
		return -3;
	}
}

int min(int a, int b){
    return a < b ? a : b;
}

int max(int a, int b){
    return a > b ? a : b;
}

void acquire_or_release_disk_locks(int raid, int diskNo, int diskPairNo, int op){
    if(raid == 1 || raid == 2){
        op == 1 ? acquiresleep(diskLock[min(diskNo, diskPairNo)]) : releasesleep(diskLock[min(diskNo, diskPairNo)]);
        op == 1 ? acquiresleep(diskLock[max(diskNo, diskPairNo)]) : releasesleep(diskLock[max(diskNo, diskPairNo)]);
    } else if(raid == 3 || raid == 4){
        for(int i = 1; i <= DISKS; i++){
            op == 1 ? acquiresleep(diskLock[i]) : releasesleep(diskLock[i]);
        }
    } else op == 1 ? acquiresleep(diskLock[diskNo]) : releasesleep(diskLock[diskNo]);
}

int disk_repaired_raid(int diskNo){
	if(diskNo < 1 || diskNo > DISKS) return -2;
	struct buf* b = bread(VIRTIO0_ID, 0);
	int isInit = b->data[0];
	int raid = b->data[1];
	brelse(b);
	if(isInit == 0) return -1;
    int diskPairNo = (diskNo - 1 + (DISKS / 2)) % DISKS + 1;

    acquire_or_release_disk_locks(raid, diskNo, diskPairNo, 1);

	if(diskWorking[diskNo] == 0){
		diskWorking[diskNo] = 1;
		int value = restore_data(raid, diskNo);

        acquire_or_release_disk_locks(raid, diskNo, diskPairNo, 2);

		return value;
	}

    acquire_or_release_disk_locks(raid, diskNo, diskPairNo, 2);

	return 0;
}

uint get_block_number(int raid) {
	int block_number_per_disk = DISK_SIZE / BSIZE, total_block_number;
	switch(raid) {
		case 0:
		total_block_number = block_number_per_disk * DISKS;
        break;

		case 1: case 2:
		total_block_number = block_number_per_disk * (DISKS / 2);
        break;

        case 3: case 4:
        total_block_number = block_number_per_disk * (DISKS - 1);
        break;

		default:
		return -1;
	}

    return total_block_number;
}

int info_raid(uint *blkn, uint *blks, uint *diskn){
	struct buf* b = bread(VIRTIO0_ID, 0);
	int isInit = b->data[0];
	int raid = b->data[1];
	brelse(b);
	if(isInit == 0) return -1;
	*blkn = get_block_number(raid);
	*blks = BSIZE;
	*diskn = DISKS;
	return 0;
}

int destroy_raid(){
	struct buf* b = bread(VIRTIO0_ID, 0);
	int isInit = b->data[0];
	if(isInit == 0) {
		brelse(b);
		return -1;
	}
	b->data[0] = 0;
	bwrite(b);
	brelse(b);
	for(int i = 1; i <= DISKS; i++) {
        acquiresleep(diskLock[i]);
        diskWorking[i] = 0;
        releasesleep(diskLock[i]);
    }
    return 0;
}

int read_raid(int blkn, char* data){
	struct buf* b = bread(VIRTIO0_ID, 0);
	int isInit = b->data[0];
	int raid = b->data[1];
	brelse(b);
	if(isInit == 0) return -1;
	if(blkn < 0 || blkn >= get_block_number(raid)) return -2;

	int diskNo, blockNo;
    int block_number_per_disk = DISK_SIZE / BSIZE;
	switch(raid) {
		case 0:
		diskNo = blkn % DISKS + 1;

        acquiresleep(diskLock[diskNo]);

		if(diskWorking[diskNo] == 0) {
            releasesleep(diskLock[diskNo]);
            return -3;
        }
		blockNo = blkn / DISKS;
		read_block(diskNo, blockNo, (uchar*)data);

        releasesleep(diskLock[diskNo]);

		return 0;

		case 1: case 2:
		diskNo = (raid == 1) ? (blkn / block_number_per_disk + 1) : (blkn % (DISKS / 2) + 1);

        acquiresleep(diskLock[diskNo]);
        acquiresleep(diskLock[diskNo + (DISKS / 2)]);

		if(diskWorking[diskNo] == 0 && diskWorking[diskNo + (DISKS / 2)] == 0){

            releasesleep(diskLock[diskNo]);
            releasesleep(diskLock[diskNo + (DISKS / 2)]);

            return -3;
        }
		blockNo = (raid == 1) ? (blkn % block_number_per_disk) : (blkn / (DISKS / 2));
		if(diskWorking[diskNo] == 1){
			read_block(diskNo, blockNo, (uchar*)data);
		} else {
			read_block(diskNo + (DISKS / 2), blockNo, (uchar*)data);
		}

        releasesleep(diskLock[diskNo]);
        releasesleep(diskLock[diskNo + (DISKS / 2)]);

		return 0;

        case 3: case 4:
        diskNo = blkn % (DISKS - 1) + 1;
        blockNo = blkn / (DISKS - 1);
        if(raid == 4){
            int parityDisk = blockNo % DISKS + 1;
            if(diskNo >= parityDisk) diskNo++;
        }

        for(int i = 1; i <= DISKS; i++) acquiresleep(diskLock[i]);

        if(diskWorking[diskNo] == 1) {
		    read_block(diskNo, blockNo, (uchar*)data);
        } else {
            for(int i = 0; i < BSIZE; i++) {
                data[i] = 0;
            }
            uchar* buf = kalloc();

            for(int j = 1; j <= DISKS; j++){
                if(j == diskNo) continue;
                read_block(j, blockNo, buf);
                for(int i = 0; i < BSIZE; i++) {
                    data[i] ^= buf[i];
                }
            }
            kfree(buf);

        }

        for(int i = 1; i <= DISKS; i++) releasesleep(diskLock[i]);

        return 0;

		default:
		return -3;
	}
}

int write_raid(int blkn, char* data){
	struct buf* b = bread(VIRTIO0_ID, 0);
	int isInit = b->data[0];
	int raid = b->data[1];
	brelse(b);
	if(isInit == 0) return -1;
	if(blkn < 0 || blkn >= get_block_number(raid)) return -2;

	int diskNo, blockNo;
    uchar* oldBlock, *parityBlock;
	int block_number_per_disk = DISK_SIZE / BSIZE;
	switch(raid) {
		case 0:
		diskNo = blkn % DISKS + 1;

        acquiresleep(diskLock[diskNo]);

		if(diskWorking[diskNo] == 0) {

            releasesleep(diskLock[diskNo]);

            return -3;
        }
		blockNo = blkn / DISKS;
		write_block(diskNo, blockNo, (uchar*)data);

        releasesleep(diskLock[diskNo]);

		return 0;

		case 1: case 2:
		diskNo = (raid == 1) ? (blkn / block_number_per_disk + 1) : (blkn % (DISKS / 2) + 1);

        acquiresleep(diskLock[diskNo]);
        acquiresleep(diskLock[diskNo + (DISKS / 2)]);

		if(diskWorking[diskNo] == 0 && diskWorking[diskNo + (DISKS / 2)] == 0) {

            releasesleep(diskLock[diskNo]);
            releasesleep(diskLock[diskNo + (DISKS / 2)]);

            return -3;
        }
		blockNo = (raid == 1) ? (blkn % block_number_per_disk) : (blkn / (DISKS / 2));
		if(diskWorking[diskNo] == 1) write_block(diskNo, blockNo, (uchar*)data);
		if(diskWorking[diskNo + (DISKS / 2)] == 1) write_block(diskNo + (DISKS / 2), blockNo, (uchar*)data);

        releasesleep(diskLock[diskNo]);
        releasesleep(diskLock[diskNo + (DISKS / 2)]);

		return 0;

        case 3: case 4:
        diskNo = blkn % (DISKS - 1) + 1;
        blockNo = blkn / (DISKS - 1);
        int parityDisk = raid == 3 ? DISKS : blockNo % DISKS + 1;
        if(diskNo >= parityDisk) diskNo++;

        oldBlock = kalloc();

        for(int i = 1; i <= DISKS; i++) acquiresleep(diskLock[i]);

        if(diskWorking[diskNo] == 1) {
            read_block(diskNo, blockNo, oldBlock);
            write_block(diskNo, blockNo, (uchar*)data);

        } else {

            for(int i = 0; i < BSIZE; i++) {
                oldBlock[i] = 0;
            }
            uchar* buf = kalloc();

            for(int j = 1; j <= DISKS; j++){
                if(j == diskNo) continue;
                read_block(j, blockNo, buf);
                for(int i = 0; i < BSIZE; i++) {
                    oldBlock[i] ^= buf[i];
                }
            }

            kfree(buf);
        }

        if(diskWorking[parityDisk] == 0){

            for(int i = 1; i <= DISKS; i++) releasesleep(diskLock[i]);

            kfree(oldBlock);
            return -3;
        }

        parityBlock = kalloc();
        read_block(parityDisk, blockNo, parityBlock);
        for(int i = 0; i < BSIZE; i++) {
            parityBlock[i] = data[i] ^ oldBlock[i] ^ parityBlock[i];
        }
        write_block(parityDisk, blockNo, parityBlock);

        for(int i = 1; i <= DISKS; i++) releasesleep(diskLock[i]);

        kfree(parityBlock);
        kfree(oldBlock);

        return 0;

		default:
		return -3;
	}
}