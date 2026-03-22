# RAID layer (xv6 kernel extension)

This project represents the design and implementation of **RAID0**, **RAID1**, **RAID0_1**, **RAID4**, and **RAID5** structures on the **xv6 operating system**, supporting safe and efficient concurrent access by multiple processes through locking mechanisms.

The normal xv6 file system still lives on the first VirtIO block device (`fs.img`). A configurable set of additional VirtIO disks (`disk_0.img`, `disk_1.img`, …) implements the RAID array. RAID metadata (initialized flag and active level) is stored in **block 0** of `fs.img`, separate from the file-system image.

## Hardware model

| VirtIO ID | QEMU backing | Role |
|-----------|----------------|------|
| 0 | `fs.img` | Boot disk, `mkfs` file system, RAID superblock in sector block 0 |
| 1 … `DISKS` | `disk_0.img` … `disk_{DISKS-1}.img` | RAID data disks (numbered **1 … `DISKS`** in the API) |

Disk size for each RAID image is set at compile time via `DISK_SIZE` (bytes); the number of RAID disks is `DISKS` (default **4**).

## RAID levels

Levels are selected with `init_raid` using `enum RAID_TYPE` in [`user/user.h`](user/user.h#L26):

| Name | Value | Description |
|------|-------|-------------|
| `RAID0` | 0 | Striping across all disks |
| `RAID1` | 1 | Mirroring: first half / second half of disks are copies |
| `RAID0_1` | 2 | Striped pairs (requires at least 4 disks, even `DISKS`) |
| `RAID4` | 3 | Block-level striping with **dedicated parity** on the last disk |
| `RAID5` | 4 | Block-level striping with **rotating parity** per stripe |

**Constraints**

- `DISKS` must be greater than 1.
- `RAID1` requires an **even** `DISKS`.
- `RAID0_1` requires `DISKS` ≥ 4 and **even** `DISKS`.
- `init_raid` fails if RAID was already initialized (`-1`), the level is invalid (`-2`), `DISKS` is too small (`-3`), or parity/mirror layout constraints fail (`-4`).

**Logical capacity** (in `BSIZE` blocks), when RAID is initialized:

- **RAID0:** `(DISK_SIZE / BSIZE) * DISKS`
- **RAID1 / RAID0_1:** `(DISK_SIZE / BSIZE) * (DISKS / 2)`
- **RAID4 / RAID5:** `(DISK_SIZE / BSIZE) * (DISKS - 1)` (one disk’s worth used for parity)

Use `info_raid` to query block count, block size, and disk count at runtime.

## System calls

All user APIs are declared in [`user/user.h`](user/user.h#L27-L33) and implemented in [`kernel/sysraid.c`](kernel/sysraid.c) / [`kernel/raid.c`](kernel/raid.c). Syscall numbers (`SYS_init_raid`, …) are defined in [`kernel/syscall.h`](kernel/syscall.h#L23-L29).

| Call | Purpose |
|------|---------|
| `init_raid(level)` | One-time setup; writes superblock on `fs.img` block 0 |
| `read_raid(blkn, buf)` | Read one `BSIZE` block at logical index `blkn` |
| `write_raid(blkn, buf)` | Write one `BSIZE` block |
| `disk_fail_raid(diskn)` | Simulate failure: mark disk offline and overwrite its blocks with zeros |
| `disk_repaired_raid(diskn)` | Mark disk online and **rebuild** its content (if possible) |
| `info_raid(&blkn, &blks, &diskn)` | Return total logical blocks, block size, and `DISKS` |
| `destroy_raid()` | Clear RAID superblock and tear down in-kernel state |

**Typical errors:** not initialized (`-1`), bad block index (`-2`), operation impossible because required disks are failed (`-3`), or invalid disk number (`-2` on fail/repair).

User buffers must hold at least `BSIZE` bytes for read/write.

## Build and run

Prerequisites match the main [README](README): RISC-V toolchain and `qemu-system-riscv64`.

```bash
make qemu
```

This creates `disk_0.img`, `disk_1.img`, etc. `DISK_SIZE` is the **byte** size of each RAID disk; the default **131072** is **128 KiB** per disk (see [Makefile](Makefile#L60)).

Override at build time:

```bash
make qemu DISKS=6 DISK_SIZE=262144
```

`make clean` removes the generated `disk_*.img` files as well as objects.

## Tests

Example user programs linked into `fs.img`:

- [`user/accuracy_test.c`](user/accuracy_test.c) — stress tests all RAID levels with concurrent readers/writers and fail/repair scenarios.
- [`user/basic_test.c`](user/basic_test.c) — simple read/write and single-disk fail/repair (optional level from argv).
- [`user/RAID1_test.c`](user/RAID1_test.c) — small `RAID1` smoke test.

Run inside QEMU from the shell after boot (for example `$ accuracy_test` or `$ basic_test 3`).

## Implementation files

- [`kernel/raid.c`](kernel/raid.c) — mapping of logical blocks to disks, parity, locking, fail/repair.
- [`kernel/sysraid.c`](kernel/sysraid.c) — syscall glue (`copyin`/`copyout`, `kalloc` buffers).
- [`kernel/virtio_disk.c`](kernel/virtio_disk.c) — `read_block` / `write_block` on VirtIO IDs 1 … `DISKS`.
- [`kernel/defs.h`](kernel/defs.h) — `VIRTIO0_ID`, `VIRTIO_RAID_DISK_START` / `END`.

For upstream xv6 background, see the repository [README](README).
