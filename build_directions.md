# Build Directions for FUSE DAX fmap Testing

Three repos must be built: linux kernel, libfuse, and famfs.

## Current Branches (--after / BPF struct_ops testing)

| Repo | Path | Branch |
|------|------|--------|
| linux | `/home/gourry/git/linux` | `famfs_hax` |
| libfuse | `/home/gourry/git/libfuse` | `famfs_hacks` |
| famfs | `/home/gourry/git/famfs` | `famfs_hacks` |

## Prerequisites

- gcc, make, flex, bison, libelf-dev (kernel build)
- meson, ninja (libfuse build)
- cmake (famfs build)
- ndctl, libdaxctl-devel (runtime)
- clang, llvm, bpftool (BPF programs, --after mode only)
- fio (stress testing)
- virtme-ng (`vng`) for VM testing with QEMU NVDIMM

## 1. Linux Kernel (`/home/gourry/git/linux`)

```bash
cd /home/gourry/git/linux
git checkout famfs_hax

# Ensure required configs are set:
#   CONFIG_FUSE_FS=y
#   CONFIG_FUSE_DAX_FMAP=y
#   CONFIG_FUSE_DAX_FMAP_BPF=y
#   CONFIG_DEV_DAX=y
#   CONFIG_DEV_DAX_FSDEV=y

# Fix CRLF if present in Kconfig (known issue on famfs_hax branch)
sed -i 's/\r$//' drivers/dax/Kconfig

make -j$(nproc)
```

### Branch: `famfs_hax` (tip `cbb83e2040db`)

BPF struct_ops, GET_FMAP/GET_DAXDEV opcodes, ops_name INIT negotiation,
rewritten fuse_dax_fmap.c for the new protocol.

## 2. libfuse (`/home/gourry/git/libfuse`)

libfuse is built out-of-tree into the famfs debug directory. The famfs repo
has a `libfuse` symlink pointing to `/home/gourry/git/libfuse`.

```bash
cd /home/gourry/git/libfuse
git checkout famfs_hacks

cd /home/gourry/git/famfs/debug/libfuse

# Full reconfigure (needed after branch switch)
meson setup --wipe . /home/gourry/git/libfuse

# Build just the library (examples may fail to compile)
ninja lib/libfuse3.so.3.18.0
```

The library is installed at `debug/libfuse/lib/libfuse3.so.4`.

### Branch: `famfs_hacks` (tip `d0f6b23`)

Has `FUSE_CAP_IOMAP`, ops_name in INIT, GET_FMAP/GET_DAXDEV dispatch.

## 3. famfs (`/home/gourry/git/famfs`)

```bash
cd /home/gourry/git/famfs
git checkout famfs_hacks

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

### Branch: `famfs_hacks` (tip `df45a8c`)

BPF wire format (`dax_fmap_wire.h`), ops_name negotiation, `FUSE_CAP_IOMAP`.

## 4. BPF Programs (--after mode only)

```bash
clang -O2 -g -target bpf \
    -I/home/gourry/git/linux/include/uapi \
    -I/home/gourry/git/linux/include \
    -c fuse_dax_simple.bpf.c -o /tmp/fuse_dax_simple.bpf.o
```

The BPF .o must be compiled against the same kernel headers it will run on.
On the `famfs_hax` kernel, the `fuse_dax_fmap_ops` struct uses callback
names `dax_fmap_parse` and `iomap_begin`.

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
