/* file-mmu.c: tagfs MMU-based file operations
 *
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */

/*
 * NOTE! This filesystem is probably most useful
 * not as a real filesystem, but as an example of
 * how virtual filesystems can be written.
 *
 * It doesn't get much simpler than this. Consider
 * that this file implements the full semantics of
 * a POSIX-compliant read-write filesystem.
 *
 * Note in particular how the filesystem does not
 * need to implement any data structures of its own
 * to keep track of the virtual data: using the VFS
 * caches is sufficient.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include "tagfs.h"
#include <linux/sched.h>
#include <linux/dax.h>

#include "internal.h"

MODULE_LICENSE("GPL v2");

#ifndef CONFIG_MMU
#error "Tagfs requires a kernel with CONFIG_MMU enabled"
#endif

#ifndef CONFIG_DAX
#error "Tagfs requires a kernel with CONFIG_DAX enabled"
#endif

#ifndef CONFIG_FS_DAX
#error "Tagfs requires a kernel with CONFIG_FS_DAX enabled"
#endif

static unsigned long tagfs_mmu_get_unmapped_area(struct file *file,
		unsigned long addr, unsigned long len, unsigned long pgoff,
		unsigned long flags)
{
	return current->mm->get_unmapped_area(file, addr, len, pgoff, flags);
}

const struct file_operations tagfs_file_operations = {
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	.fsync		= noop_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
	.llseek		= generic_file_llseek,
	.get_unmapped_area	= tagfs_mmu_get_unmapped_area,
};

const struct inode_operations tagfs_file_inode_operations = {
	.setattr	= simple_setattr,
	.getattr	= simple_getattr,
};
