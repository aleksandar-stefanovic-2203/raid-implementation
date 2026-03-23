#include "kernel/types.h"
#include "user/user.h"
#include "user/random.h"

int iter = 0;
int stat_m[5][2];

const int n = 8; // number of write/read procs

void write_worker(uint blocks, uint block_size, int id, int n, int raid) {
	uchar* blk = malloc(block_size);
  	for (uint i = id; i < blocks; i+=n) {
    	for (uint j = 0; j < block_size; j++) {
      		blk[j] = j + i + raid + iter;
    	}
    	write_raid(i, blk);
  	}
	free(blk);
	exit(0);
}

void read_worker(uint blocks, uint block_size, int id, int n, int raid) {
	uchar* blk = malloc(block_size);
	for (uint i = id; i < blocks; i+=n) {
    	read_raid(i, blk);
    	for (uint j = 0; j < block_size; j++) {
      		if ((uchar)(j + i + raid + iter) != blk[j]) {
        		printf("expected = %d got = %d ", j + i, blk[j]);
        		printf("Data in the block %d faulty\n", i);
        		break;
				printf("x");
				exit(1);
      		}
    	}
  	}
    free(blk);
	printf("o");
	exit(0);
}

void fail_reapair_worker(uint disk_num) {
	int diskn;
	srand(getpid());
	sleep(rand() % 10);
	for(int i = 0; i < disk_num; i++){
        diskn = 1 + rand() % disk_num;
        disk_fail_raid(diskn);
        sleep(rand() % 10);
        disk_repaired_raid(diskn);
        sleep(rand() % 10);
    }
    sleep(rand()%4);
    diskn = rand() % disk_num;
    disk_fail_raid(diskn);
    sleep(rand()%4);
    disk_repaired_raid(diskn);
	exit(0);
}

void
print_name(int raid)
{
    switch(raid) {
		case 0:
			printf("RAID0");
			break;
		case 1:
			printf("RAID1");
			break;
		case 2:
			printf("RAID0_1");
			break;
		case 3:
			printf("RAID4");
			break;
		case 4:
			printf("RAID5");
			break;
		default: return;
    }
}

void
test(int raid, int f)
{
	printf("\nTest ");
	print_name(raid);
    printf("\n");
	sleep(3);

	init_raid(raid);
  	uint disk_num, block_num, block_size;
  	info_raid(&block_num, &block_size, &disk_num);
  	uint blocks = (512 > block_num ? block_num : 512);

	//printf("- write\n");
	for(int i = 0; i < n; i++) if (fork() == 0) write_worker(blocks, block_size, i, n, raid);
	if (f && fork() == 0) fail_reapair_worker(disk_num);

	for(int i = 0; i < n + 1; i++) wait((int *) 0);

	//printf("- read\n");
	for(int i = 0; i < n; i++) if (fork() == 0) read_worker(blocks, block_size, i, n, raid);
	if (f && fork() == 0) fail_reapair_worker(disk_num);

    int stat;
	for(int i = 0; i < n + f; i++) {
        wait(&stat);
        stat_m[raid][stat]++;
    }
    stat_m[raid][0]-=f;
	destroy_raid();
}

int
main(int argc, char *argv[])
{
    for(int i = 0; i < 5; i++) stat_m[i][0] = stat_m[i][1] = 0;
	int itr_n = argc > 1 ? atoi(argv[1]) : 1;

	for(int i = 0; i < itr_n; i++) {
        printf("\n- iter %d:", ++iter);
		test(RAID0, 0);
		test(RAID1, 1);
		test(RAID0_1, 1);
		test(RAID4, 1);
		test(RAID5, 1);
	}

	sleep(3);
	printf("\n- result:\n");

    for(int i = 0; i < 5; i++) {
        print_name(i);
        printf("\t o:%d  x:%d\n", stat_m[i][0], stat_m[i][1]);
    }
    return 0;
}