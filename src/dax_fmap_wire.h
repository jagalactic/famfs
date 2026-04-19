/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Wire format structs for FUSE DAX fmap BPF programs.
 *
 * These structs define the GET_FMAP response blob format that the
 * BPF dax_fmap_parse() programs read. The kernel treats this blob
 * as opaque — only BPF programs interpret it.
 *
 * The FUSE server serializes file metadata into this format, and
 * the BPF parse() callback deserializes it into a compact meta_buf
 * for the hot-path iomap_begin() callback.
 */
#ifndef DAX_FMAP_WIRE_H
#define DAX_FMAP_WIRE_H

#include <stdint.h>

/*
 * dax_simple: linear extent list
 *
 * Blob layout: dax_simple_wire_hdr + n_extents * dax_simple_wire_ext
 */
struct dax_simple_wire_hdr {
	uint64_t file_size;
	uint32_t n_extents;
	uint32_t reserved;
};

struct dax_simple_wire_ext {
	uint32_t dev_index;
	uint32_t reserved;
	uint64_t offset;
	uint64_t len;
};

/*
 * dax_interleave: striped extent list
 *
 * Blob layout: dax_ileave_wire_hdr +
 *              n_iexts * { dax_ileave_wire_iext + nstrips * dax_ileave_wire_strip }
 */
struct dax_ileave_wire_hdr {
	uint64_t file_size;
	uint32_t n_iexts;
	uint32_t reserved;
};

struct dax_ileave_wire_iext {
	uint64_t chunk_size;
	uint32_t nstrips;
	uint32_t reserved;
	uint64_t nbytes;
};

struct dax_ileave_wire_strip {
	uint32_t dev_index;
	uint32_t reserved;
	uint64_t offset;
	uint64_t len;
};

/*
 * meta_size calculations for BPF meta_buf allocation.
 * These match what the BPF programs expect.
 */
struct dax_simple_meta_hdr {
	uint32_t n_extents;
	uint32_t reserved;
};

struct dax_simple_meta_ext {
	uint32_t dev_index;
	uint32_t reserved;
	uint64_t offset;
	uint64_t len;
};

#define DAX_SIMPLE_META_SIZE(n_extents) \
	(sizeof(struct dax_simple_meta_hdr) + \
	 (n_extents) * sizeof(struct dax_simple_meta_ext))

struct dax_ileave_meta_hdr {
	uint32_t n_iexts;
	uint32_t reserved;
};

struct dax_ileave_meta_iext {
	uint64_t chunk_size;
	uint64_t nstrips;
	uint64_t nbytes;
	uint32_t strip_base;
	uint32_t reserved;
};

struct dax_ileave_meta_strip {
	uint64_t dev_index;
	uint64_t offset;
	uint64_t len;
};

#define DAX_ILEAVE_META_SIZE(n_iexts, total_strips) \
	(sizeof(struct dax_ileave_meta_hdr) + \
	 (n_iexts) * sizeof(struct dax_ileave_meta_iext) + \
	 (total_strips) * sizeof(struct dax_ileave_meta_strip))

#endif /* DAX_FMAP_WIRE_H */
