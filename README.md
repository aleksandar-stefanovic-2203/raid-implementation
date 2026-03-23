# RAID extension for xv6-riscv (patch-only distribution)

This project represents the design and implementation of **RAID0**, **RAID1**, **RAID0+1**, **RAID4**, and **RAID5** structures on the **xv6 operating system**, supporting safe and efficient concurrent access by multiple processes through locking mechanisms.

This project contains:
- a patch file that applies the RAID extension to upstream xv6-riscv
- core RAID implementation sources (for reference)
- user-space RAID tests

The goal is to keep this repository lightweight while still making the implementation easy to build and verify.

## Repository layout

- `patches/0001-raid-core-add-software-RAID-layer.patch` - patch to apply on top of upstream xv6-riscv
- `implementation/raid.c` - RAID core logic reference
- `implementation/sysraid.c` - syscall wrapper logic reference
- `tests/basic_test.c` - simple functional test
- `tests/accuracy_test.c` - larger stress/consistency test
- `tests/random.h` - helper definitions for tests

## RAID levels

The implementation provides:

| Name | Description |
|------|-------------|
| `RAID0` | Striping across all disks |
| `RAID1` | Mirroring: first half / second half of disks are copies |
| `RAID0+1` | Striped pairs (requires at least 4 disks, even `DISKS`) |
| `RAID4` | Block-level striping with **dedicated parity** on the last disk |
| `RAID5` | Block-level striping with **rotating parity** per stripe |

## How to test against upstream xv6-riscv

This patch is tested against upstream commit:

`5474d4bf72fd95a6e5c735c2d7f208f58990ceab`

1. Clone upstream xv6-riscv:

```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
```

2. Checkout the compatible upstream commit:

```bash
cd xv6-riscv
git checkout 5474d4bf72fd95a6e5c735c2d7f208f58990ceab
```

3. Apply the patch from this repository:

```bash
git apply /absolute/path/to/raid-implementation/patches/0001-raid-core-add-software-RAID-layer.patch
```

4. Build and run:

```bash
make qemu
```

5. In the xv6 shell, run RAID tests:

```bash
basic_test
accuracy_test
```

## Notes

- The patch updates kernel and user-space code needed for RAID support and testing.
- The patch also updates build settings (including RAID disk images and compile-time RAID parameters).
- If `git apply` reports conflicts, make sure you are using an upstream revision compatible with the patch.

## License

This work is intended to be applied to [xv6-riscv](https://github.com/mit-pdos/xv6-riscv), which is distributed under the MIT license.
