# famfs-fuse Design Notes

The fuse (Filesystem in USEr space) maintainers have asked for famfs to be ported into fuse.
This requires that famfs-fuse files cache their entire file-to-dax metadata in their fuse/kernel
metadata. As of May 2025, famfs does this. This document is an overview of how famfs-fuse works.

As of May 2025, the famfs user space code works with both standalone and fuse-based famfs.
We will deprecate the standalone after we've proven there are no major performance (or other)
regressions.

## On-Media Format (omf)

The port of the famfs on-media format was simple. Basically nothing changed - or at least nothing
changed to accommodate fuse. (There have been some OMF changes during fuse development, but they are
not directly connected to famfs-fuse functionality.)

## Famfs Fuse Daemon User Space

The big new item in the fuse version of famfs is the famfs fuse daemon (```famfs_fused```). This is
a low-level libfuse server. It introduces two new messages and responses into the fuse protocol:
GET_FMAP and GET_DAXDEV. An fmap is the map of a file to dax device ranges, and a daxdev is (obviously(
a dax device that is or may be referenced by files.

Standalone famfs pushes all file metadata into the kernel when the log is played, but that's not how
fuse works. Fuse wants to discover the file system contents and structure through (primarily)
READDIR and LOOKUP commands - much like a conventional standalone file system.

The disconnect is that the famfs metadata exists in the famfs metadata log, which is sequential in
chronological order. To avoid an O(n^2) order when looking up files, we create the "shadow tree".
The famfs metadata log is played into the shadow tree, and READDIR and LOOKUP retrieve information
from the shadow tree.

The shadow tree is actually a tmpfs file system, and famfs leverages quite a bit from the example
passthrough_ll.c program from libfuse. The shadow tree is a "passed through" ... to a point. Directories
are passed through. With files, the name is passed through but the attributes are stored IN the file,
in YAML format, and decoded upon LOOKUP etc. by famfs_fused. This approach probably seems a bit weird,
but it gives us an elegant tree-based structure to play the log into, while giving us efficient
READDIR and LOOKUP.

The shadow-file YAML contains the full "fmap" metadata for each file.

## Famfs User Space
This repo has the user space famfs code. This code creates famfs file systems and files, mounts famfs, plays the famfs metadata
log, etc. 

| **Operation** | **Description** |
|-----------|-------------|
| ```mkfs.famfs```| Initializes a superblock and empty log on raw a devdax device. This functions identically for standalone and fuse famfs. |
| ```famfs mount``` | Verifies a valid superblock and log, mounts famfs, creates the meta files (```mkmeta```), and plays the log. The superblock and log operations are identical for standalone and fuse famfs, but the other steps differ. |
| ```famfs logplay``` | Plays the log for a famfs file system. Log access is identical for standalone and fuse famfs, but the actions taken for each log entry are different.|

### ```famfs logplay```
We discuss ```famfs logplay``` first because it's a subset of ```famfs mount```.

```famfs logplay``` operates on a mounted famfs file system. In the standalong mode,
it pushes all files and their metadata into the kernel, such that metadata for the entire
mounted file system is cached in-kernel.

In famfs-fuse, the kernel calls READDIR and LOOKUP to discover what files exist. As a result,
```famfs logplay``` plays the log into "shadow files" in the "shadow file system". As mentioned
above, directories are just passed through; the /existence/ of files is passed through, but the
metadata is decoded from YAML in each shadow file.

Note that once a famfs file system is mounted, its superblock and log are exposed as the files
```.meta/.superblock``` and ```.meta/.log```. So the log is played from a file - from the "front door"
of famfs. But the log must be played to the specific shadow file system of the mount in question.

We find the shadow file system for a given famfs mount by parsing the mount options in ```/proc/mounts```.
When we mount a famfs file system, we pass in the option ```-o shadow=<path>```, which is exposed in
```/proc/mounts```.

### ```famfs mount```

## Kernel Space

The idea is as follows:

- Adapt fuse to support a file type that sets the S_DAX flag, caches the famfs dax extent
  list in the kernel, and uses the dev_dax_iomap capabilities to allow famfs files to be
  proper fs-dax files for which read/write/fault handling takes place without upcalls.
- Adapt the famfs user space to do the following:
  - Play the log into a "shadow file system" as described below
  - Provide a famfs-fused daemon, based on a modified version of a fuse pass-through server,
    that uses the fuse_dax_iomap capabilities to provide famfs capabilities via fuse, using the 
    shadow file system as the source metadata.
  - The shadow file system may be periodically update via ```famfs logplay```, which will
    update the contents of the famfs-fuse file system.

## Pass-through Fuse Filesystem

Fuse supports pass-through file systems, and has an example pass-through file system.
We need to bring up the pass-through file system - initially unmodified.

## Super-High-Level Architecture

Add a ```famfs logplay``` option taht plays a famfs log into a regular file system, with
the following qualifications:

- Directories are created normally
- Files are created at the appropriate relative paths, but the contents of each file are YAML
  that describes the metadata of the file (dax extent list, owner, group, permissions, etc.).
  That's basically a YAML version of a famfs log entry.

Step 0 is to expose this file system via fuse pass-through. So at step 0, the YAML would be
exposed via a fuse mount.

Step N is to expose this file system via fuse pass-through, but instead of exposing the YAML
file contents, we will expose fs-dax files that /apply/ the metadata such that accessing a file
references the backing memory for the file.

See Patching Fuse below

## famfs logplay

```famfs logplay``` needs a new option (```--shadow <path>```) that plays a famfs log into a regular 
filesystem (which might be any filesystem other than famfs). Directories are created normally,
But files contain a YAML-translation of the log's description of the file, such as this:

```
file:
  path: /path/to/file
  owner: <uid>
  group: <gid>
  permissions: '0755'
  extents:
    - device_uuid: '123e4567-e89b-12d3-a456-426614174000'
      offset: 0
      length: 2097152
    - device_uuid: '123e4567-e89b-12d3-a456-426614174000'
      offset: 4194304
      length: 8388608
    - device_uuid: '123e4567-e89b-12d3-a456-426614174001'
      offset: 16777216
      length: 33554432
```

Note that devices don't have UUIDs yet. This is an OMF change (on-media-format)
and thus needs to be pushed cautiously. Also: CXL DCDs provide a UUID for each 
allocation, meaning there will be a DAX device that can be found via its UUID.

A reasonable approach would involve the following (none of which has to be done 
immediately):

- Add a device uuid to the superblock
- If the the dax device is a DCD allocation (i.e. tagged capacity), the tag
  will be used as the device uuid. Otherwise generate a UUID.
- In the future, when we support famfs instances that span multiple DAX devices,
  each will have a superblock that includes the file system UUID and the specific
  device UUID, but only one of the superblocks will be the /primary/ superblock.
- Additional device UUIDs will be added to a file system via a new type of log
  entry that indicates that a new device UUID is now part of the file system.

## Patching Fuse

The patching of fuse to support famfs fs-dax files is TBD...
