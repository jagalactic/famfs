# Build Directions for FUSE DAX fmap Testing

Three repos must be built: linux kernel, libfuse, and famfs.

## Prerequisites

- gcc, make, flex, bison, libelf-dev (kernel build)
- meson, ninja (libfuse build)
- cmake (famfs build)
- ndctl, libdaxctl-devel (runtime)
- clang, llvm, bpftool (BPF programs, --after mode only)
- fio (stress testing)
- virtme-ng (`vng`) for VM testing with QEMU NVDIMM

## 1. Linux Kernel

```bash
cd /home/gourry/git/linux

# Ensure required configs are set:
#   CONFIG_FUSE_FS=y
#   CONFIG_FUSE_DAX_FMAP=y (or CONFIG_FUSE_FAMFS_DAX=y on pre-rename branches)
#   CONFIG_FUSE_DAX_FMAP_BPF=y (--after mode)
#   CONFIG_DEV_DAX=y
#   CONFIG_DEV_DAX_FSDEV=y

# Fix CRLF if present in Kconfig (known issue on some branches)
sed -i 's/\r$//' drivers/dax/Kconfig

make -j$(nproc)
```

### Branches

- `famfs_hax`: post-BPF kernel with struct_ops, GET_FMAP/GET_DAXDEV opcodes,
  ops_name INIT negotiation. Use for --after testing.
- `mm-unstable` at `7557a50adf6c` (5 commits back from tip): pre-BPF with
  native kernel extent parsing. Use for --before testing. Requires reverting
  the 5 BPF/rename commits on top.

## 2. libfuse

libfuse is built out-of-tree into the famfs debug directory. The famfs repo
has a `libfuse` symlink pointing to `/home/gourry/git/libfuse`.

```bash
cd /home/gourry/git/famfs/debug/libfuse

# Full reconfigure (needed after branch switch)
meson setup --wipe . /home/gourry/git/libfuse

# Build just the library (examples may fail to compile)
ninja lib/libfuse3.so.3.18.0
```

The library is installed at `debug/libfuse/lib/libfuse3.so.4`.

### Branches

- `famfs_hacks`: post-BPF libfuse with `FUSE_CAP_IOMAP`, ops_name in INIT,
  GET_FMAP/GET_DAXDEV dispatch. Use for --after testing.
- `jagalactic/famfs` (`b0e27bd`): pre-BPF libfuse with `FUSE_CAP_DAX_FMAP`
  naming. Use for --before testing.

### ABI note

The UAPI protocol bit (`FUSE_DAX_FMAP`, bit 43) is the same across both
branches. libfuse maps it to different capability bits:
- `famfs_hacks`: `FUSE_CAP_IOMAP` (bit 32)
- `jagalactic/famfs`: `FUSE_CAP_DAX_FMAP` (bit 33)

famfs must be built against the matching libfuse branch.

## 3. famfs

```bash
cd /home/gourry/git/famfs

# Clean reconfigure
rm -rf debug/CMakeCache.txt debug/CMakeFiles
cmake -B debug -DCMAKE_BUILD_TYPE=Debug

# Build the three binaries (skip multichase which may fail)
cd debug
make famfs mkfs.famfs famfs_fused -j$(nproc)
```

Produces:
- `debug/famfs` — CLI tool (does not link libfuse)
- `debug/mkfs.famfs` — mkfs (does not link libfuse, execs famfs_fused internally)
- `debug/famfs_fused` — FUSE daemon (links libfuse3.so via RUNPATH)

### Branches

- `famfs_hacks`: post-BPF famfs with BPF wire format, ops_name negotiation.
  Use for --after testing.
- `4034a1f`: pre-BPF famfs with native GET_FMAP serialization,
  `FUSE_CAP_DAX_FMAP` naming. Use for --before testing.

## 4. BPF Programs (--after mode only)

```bash
clang -O2 -g -target bpf \
    -I/home/gourry/git/linux/include/uapi \
    -I/home/gourry/git/linux/include \
    -c fuse_dax_simple.bpf.c -o /tmp/fuse_dax_simple.bpf.o
```

The BPF .o must be compiled against the same kernel headers it will run on.
Callback names in the struct_ops must match the kernel's `fuse_dax_fmap_ops`
struct (e.g., `dax_fmap_parse`/`iomap_begin` on famfs_hax).

## 5. Running Tests in VM

```bash
# Boot VM with 8GB NVDIMM
vng --force --run /home/gourry/git/linux \
    --disable-microvm --force-9p --memory 8G \
    --qemu-opts="-machine nvdimm=on -m slots=4,maxmem=16G \
    -object memory-backend-file,id=nvmem0,mem-path=/tmp/pmem0.img,size=8G,align=2M \
    -device nvdimm,memdev=nvmem0,id=nv0,label-size=16M"
```

Inside the VM:

```bash
# Create sudo shim (VM runs as root, no sudo binary)
echo '#!/bin/bash' > /bin/sudo
echo 'exec "$@"' >> /bin/sudo
chmod +x /bin/sudo

# Setup namespace and driver
ndctl create-namespace -f -e namespace0.0 --mode=devdax --align=2M
echo dax0.0 > /sys/bus/dax/drivers/device_dax/unbind
echo dax0.0 > /sys/bus/dax/drivers/fsdev_dax/bind

# Set environment
export LD_LIBRARY_PATH=/home/gourry/git/famfs/debug/libfuse/lib:$LD_LIBRARY_PATH
export SCRIPTS=/home/gourry/git/famfs/scripts
export MPT=/tmp/famfs_mnt
export BIN=/home/gourry/git/famfs/debug

# Run test
cd /home/gourry/git/famfs
bash smoke/test_fuse_dax.sh --after -m fuse -d /dev/dax0.0   # or --before
```

### Driver binding note

The dax bus doesn't support `driver_override`. You must explicitly unbind
from `device_dax` then bind to `fsdev_dax`. Using `drivers_probe` will
re-bind `device_dax` first since it matches the device type.
