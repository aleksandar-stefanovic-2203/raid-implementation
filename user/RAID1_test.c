#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int main(int argc, char* argv[]){
	init_raid(RAID1);

    uchar* data = malloc(BSIZE);
    data[0] = 7;
    write_raid(0, data);
    write_raid(1, data);
    data[0] = 8;
    write_raid(128, data);
    read_raid(0, data);
    printf("%d\n", data[0]);

    exit(0);
}