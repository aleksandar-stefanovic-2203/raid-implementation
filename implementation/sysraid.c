#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "param.h"
#include "proc.h"
#include "fs.h"

extern int init_raid(int);
extern int read_raid(int, char*);
extern int write_raid(int, char*);
extern int disk_fail_raid(int);
extern int disk_repaired_raid(int);
extern int info_raid(uint*, uint*, uint*);
extern int destroy_raid();

uint64
sys_init_raid(void)
{
    uint64 raid;
    argaddr(0, &raid);
    return init_raid(raid);
}

uint64
sys_read_raid(void)
{
	uint64 blkn, vaddrdata;
	argaddr(0, &blkn);
	argaddr(1, &vaddrdata);

	char* dataBuf = kalloc();
    int value = read_raid(blkn, dataBuf);
	if(value < 0) {
		kfree(dataBuf);
		return value;
	}

	struct proc* p = myproc();
	pagetable_t pmt = p->pagetable;

	copyout(pmt, vaddrdata, dataBuf, BSIZE);

	kfree(dataBuf);
	return 0;
}

uint64
sys_write_raid(void)
{
	uint64 blkn, vaddrdata;
	argaddr(0, &blkn);
	argaddr(1, &vaddrdata);

	char* dataBuf = kalloc();

	struct proc* p = myproc();
	pagetable_t pmt = p->pagetable;

	copyin(pmt, dataBuf, vaddrdata, BSIZE);

	int value = write_raid(blkn, dataBuf);
	kfree(dataBuf);
	if(value < 0) return value;

    return 0;
}

uint64
sys_disk_fail_raid(void)
{
	uint64 diskNo;
    argaddr(0, &diskNo);
    return disk_fail_raid(diskNo);
}

uint64
sys_disk_repaired_raid(void)
{
	uint64 diskNo;
    argaddr(0, &diskNo);
    return disk_repaired_raid(diskNo);
}

uint64
sys_info_raid(void)
{
	uint blkn = 0, blks = 0, diskn = 0;
    int value = info_raid(&blkn, &blks, &diskn);
	if(value == -1) return -1;
	uint64 vaddrblkn, vaddrblks, vaddrdiskn;

	argaddr(0, &vaddrblkn);
	argaddr(1, &vaddrblks);
	argaddr(2, &vaddrdiskn);

	struct proc* p = myproc();
	pagetable_t pmt = p->pagetable;

	copyout(pmt, vaddrblkn, (char*)&blkn, 4);
	copyout(pmt, vaddrblks, (char*)&blks, 4);
	copyout(pmt, vaddrdiskn, (char*)&diskn, 4);

	return 0;
}

uint64
sys_destroy_raid(void)
{
    return destroy_raid();
}