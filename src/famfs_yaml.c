// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023-2024 Micron Technology, Inc.  All rights reserved.
 */

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "famfs_meta.h"

#define ASSERT_NE_GOTO(rc, val, bad_target) {	\
	if (rc == val) {			\
	    line = __LINE__;			\
	    goto bad_target;			\
	}					\
}

/**
 * __famfs_emit_yaml_ext_list()
 *
 * Dump the bulk of the file metadata. Calls a helper for the extent list
 *
 * @emitter:  libyaml emitter struct
 * @event:    libyaml event structure
 * @fm:       famfs_file_meta struct
 */
int
__famfs_emit_yaml_ext_list(
	yaml_emitter_t               *emitter,
	yaml_event_t                 *event,
	const struct famfs_file_meta *fm)
{
	char strbuf[160];
	int i, rc;
	int line;

	/* The extent list */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"simple_ext_list",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* Start event for the sequence of extents */
	rc = yaml_sequence_start_event_initialize(event, NULL, NULL,
						  1, YAML_BLOCK_SEQUENCE_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* The extents */
	for (i = 0; i < fm->fm_nextents; i++) {
		/* YAML_MAPPING_START_EVENT: Start of extent */
		rc = yaml_mapping_start_event_initialize(event, NULL, NULL, 1,
							 YAML_BLOCK_MAPPING_STYLE);
		ASSERT_NE_GOTO(rc, 0, err_out);
		rc = yaml_emitter_emit(emitter, event);
		ASSERT_NE_GOTO(rc, 0, err_out);

		/* Offset */
		rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"offset",
						  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		ASSERT_NE_GOTO(rc, 0, err_out);
		rc = yaml_emitter_emit(emitter, event);
		ASSERT_NE_GOTO(rc, 0, err_out);

		sprintf(strbuf, "0x%llx", fm->fm_ext_list[i].se.se_offset);
		rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
						  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		ASSERT_NE_GOTO(rc, 0, err_out);
		rc = yaml_emitter_emit(emitter, event);
		ASSERT_NE_GOTO(rc, 0, err_out);

		/* Length */
		rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"length",
						  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		ASSERT_NE_GOTO(rc, 0, err_out);
		rc = yaml_emitter_emit(emitter, event);
		ASSERT_NE_GOTO(rc, 0, err_out);

		sprintf(strbuf, "0x%llx", fm->fm_ext_list[i].se.se_len);
		rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
						  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		ASSERT_NE_GOTO(rc, 0, err_out);
		rc = yaml_emitter_emit(emitter, event);
		ASSERT_NE_GOTO(rc, 0, err_out);

		/* YAML_MAPPING_END_EVENT: End of extent */
		rc = yaml_mapping_end_event_initialize(event);
		ASSERT_NE_GOTO(rc, 0, err_out);
		rc = yaml_emitter_emit(emitter, event);
		ASSERT_NE_GOTO(rc, 0, err_out);
	}

	/* End event for the sequence of events */
	rc = yaml_sequence_end_event_initialize(event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	return 0;
err_out:
	fprintf(stderr, "%s: fail line %d rc %d errno %d problem (%s)\n",
		__func__, line, rc, errno, emitter->problem);
	perror("");

	return -1;
}


/**
 * __famfs_emit_yaml_file_section()
 *
 * Dump the bulk of the file metadata. Calls a helper for the extent list
 *
 * @emitter:  libyaml emitter struct
 * @event:    libyaml event structure
 * @fm:       famfs_file_meta struct
 */
int
__famfs_emit_yaml_file_section(
	yaml_emitter_t               *emitter,
	yaml_event_t                 *event,
	const struct famfs_file_meta *fm)
{
	char strbuf[160];
	int rc;
	int line;

	/* Relative path */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"path",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)fm->fm_relpath,
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* size */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"size",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	sprintf(strbuf, "%lld", fm->fm_size);
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* flags */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"flags",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	sprintf(strbuf, "%d", fm->fm_flags);
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* mode */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"mode",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	sprintf(strbuf, "0%o", fm->fm_mode);
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* uid */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"uid",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	sprintf(strbuf, "%d", fm->fm_uid);
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* gid */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"gid",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	sprintf(strbuf, "%d", fm->fm_gid);
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* nextents */
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)"nextents",
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	sprintf(strbuf, "%d", fm->fm_nextents);
	rc = yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)strbuf,
					  -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(emitter, event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* Drop in the extent list */
	__famfs_emit_yaml_ext_list(emitter, event, fm);

	return 0;
err_out:
	fprintf(stderr, "%s: fail line %d rc %d errno %d problem (%s)\n",
		__func__, line, rc, errno, emitter->problem);
	perror("");
	assert(0);

	return -1;
}

/**
 * famfs_emit_file_yaml()
 *
 * @fm:    famfs_file_meta structure
 * @outp:  FILE stream structure for output
 */
int
famfs_emit_file_yaml(
	const struct famfs_file_meta *fm,
	FILE *outp)
{
	yaml_emitter_t emitter;
	yaml_event_t event;
	int line;
	int rc;

	if (!yaml_emitter_initialize(&emitter)) {
		fprintf(stderr, "Failed to initialize emitter\n");
		return -1;
	}

	yaml_emitter_set_output_file(&emitter, outp);

	/* Start stream */
	if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING)) {
		fprintf(stderr, "yaml_stream_start_event_initialize() failed\n");
		goto err_out;
	}
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* Start Document */
	rc = yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* YAML_MAPPING_START_EVENT:  Start Mapping */
	rc = yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* Key: file */
	rc = yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"file",
				     -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* Start Mapping for file */
	rc = yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);


	__famfs_emit_yaml_file_section(&emitter, &event, fm);

	/* End for section indented under "file:" */
	rc = yaml_mapping_end_event_initialize(&event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out); /* boom */

	/* End Mapping */
	rc = yaml_mapping_end_event_initialize(&event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* End Document */
	rc = yaml_document_end_event_initialize(&event, 0);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	/* End Stream */
	rc = yaml_stream_end_event_initialize(&event);
	ASSERT_NE_GOTO(rc, 0, err_out);
	rc = yaml_emitter_emit(&emitter, &event);
	ASSERT_NE_GOTO(rc, 0, err_out);

	yaml_emitter_delete(&emitter);
	return 0;
err_out:
	fprintf(stderr, "%s: fail line %d rc %d errno %d problem (%s)\n",
		__func__, line, rc, errno, emitter.problem);
	perror("");

	yaml_emitter_delete(&emitter);
	return -1;
}

/* Read back in */

const char *
yaml_event_str(int event_type)
{
	switch (event_type) {
	case YAML_NO_EVENT:
		return "YAML_NO_EVENT";
	case YAML_STREAM_START_EVENT:
		return "YAML_STREAM_START_EVENT";
	case YAML_STREAM_END_EVENT:
		return "YAML_STREAM_END_EVENT";
	case YAML_DOCUMENT_START_EVENT:
		return "YAML_DOCUMENT_START_EVENT";
	case YAML_DOCUMENT_END_EVENT:
		return "YAML_DOCUMENT_END_EVENT";
	case YAML_ALIAS_EVENT:
		return "YAML_ALIAS_EVENT";
	case YAML_SCALAR_EVENT:
		return "YAML_SCALAR_EVENT";
	case YAML_SEQUENCE_START_EVENT:
		return "YAML_SEQUENCE_START_EVENT";
	case YAML_SEQUENCE_END_EVENT:
		return "YAML_SEQUENCE_END_EVENT";
	case YAML_MAPPING_START_EVENT:
		return "YAML_MAPPING_START_EVENT";
	case YAML_MAPPING_END_EVENT:
		return "YAML_MAPPING_END_EVENT";
	}
	return "BAD EVENT TYPE";
}

/* Get the next yaml event. If its type != ev_type, set rc=-1 and goto bad_target */
#define GET_YAML_EVENT_OR_GOTO(PARSER, EV, ev_type, rc, bad_target, verbose) {	\
	if (!yaml_parser_parse(PARSER, EV)) {						\
		fprintf(stderr, "%s:%d yaml parser error\n", __func__, __LINE__);	\
		rc = -1;								\
		goto bad_target;							\
	}										\
	if ((EV)->type != ev_type) {							\
		fprintf(stderr, "%s:%d: expected event type: %s but found %s\n", 	\
			__func__, __LINE__,						\
			yaml_event_str(ev_type),					\
			yaml_event_str((EV)->type));					\
		rc = -1;								\
		yaml_event_delete(EV);							\
		goto bad_target;							\
	} else if (verbose > 1)								\
		printf("%s: %s (%s)\n", __func__, yaml_event_str((EV)->type),		\
		       (ev_type == YAML_SCALAR_EVENT) ? (char *)((EV)->data.scalar.value) : ""); \
}

#define GET_YAML_EVENT(PARSER, EV, rc, bad_target, verbose) {				\
	if (!yaml_parser_parse(PARSER, EV)) {						\
		fprintf(stderr, "%s:%d yaml parser error\n", __func__, __LINE__); 	\
		rc = -1;								\
		goto bad_target;							\
	} else if (verbose > 1)								\
		printf("%s: %s (%s)\n", __func__, yaml_event_str((EV)->type), 		\
		       ((EV)->type == YAML_SCALAR_EVENT) ? (char *)((EV)->data.scalar.value) : ""); \
}

static int
famfs_parse_file_ext_list(
	yaml_parser_t *parser,
	struct famfs_file_meta *fm,
	int max_extents,
	int verbose)
{
	yaml_event_t event;
	int ext_index = 0;
	int done = 0;
	int rc = 0;
	int type;

	/* "simple_ext_list" stanza starts wtiha  YAML_SEQUENCE_START_EVENT */
	GET_YAML_EVENT_OR_GOTO(parser, &event, YAML_SEQUENCE_START_EVENT, rc, err_out, verbose);
	yaml_event_delete(&event);

	/* "simple_ext_list" stanza starts wtiha  YAML_MAPPING_START_EVENT */
	GET_YAML_EVENT_OR_GOTO(parser, &event, YAML_MAPPING_START_EVENT, rc, err_out, verbose);
	yaml_event_delete(&event);

	while (!done) {
		yaml_event_t val_event;
#define MAX_KEY 80
		char current_key[MAX_KEY];

		GET_YAML_EVENT(parser, &event, rc, err_out, verbose);
		type = event.type;

		if (type == YAML_SCALAR_EVENT)
			strncpy(current_key, (char *)event.data.scalar.value, MAX_KEY - 1);

		yaml_event_delete(&event);

		switch (type) {
		case YAML_SCALAR_EVENT:

			/* Note: this assumes that the offset always comes before the
			 * length in an extent list entry */
			if (strcmp(current_key, "offset") == 0) {
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_ext_list[ext_index].se.se_offset =
					strtoull((char *)val_event.data.scalar.value, 0, 0);
				yaml_event_delete(&val_event);

				/* Get the "length" key */
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				if (strcmp("length", (char *)val_event.data.scalar.value)) {
					fprintf(stderr, "%s: Error length didn't follow offset\n",
						__func__);
					rc = -1;
					goto err_out;
				}
				yaml_event_delete(&val_event);

				/* Get the length value */
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_ext_list[ext_index].se.se_len =
					strtoull((char *)val_event.data.scalar.value, 0, 0);

			} else {
				fprintf(stderr, "%s: Bad scalar key %s\n",
					__func__, current_key);
			}

			break;

		case YAML_MAPPING_START_EVENT:
			if (verbose > 1)
				printf("%s: extent %d is coming next\n", __func__, ext_index);
			if (ext_index >= max_extents) {
				fprintf(stderr, "%s: too many extents! (max=%d)\n",
					__func__, max_extents);
				rc = -EOVERFLOW;
				yaml_event_delete(&event);
				goto err_out;
			}
			break;
		case YAML_MAPPING_END_EVENT:
			if (verbose > 1)
				printf("%s: end of extent %d\n", __func__, ext_index);
			ext_index++;
			break;
		case YAML_SEQUENCE_END_EVENT:
			if (verbose > 1)
				printf("%s: finished with ext list (%d entries)\n",
				       __func__, ext_index);
			done = 1;
			break;
		default:
			if (verbose > 1)
				printf("%s: unexpected event %s\n",
				       __func__, yaml_event_str(event.type));
			break;
		}
		yaml_event_delete(&val_event);

	}
	GET_YAML_EVENT_OR_GOTO(parser, &event, YAML_MAPPING_END_EVENT, rc, err_out, verbose);
err_out:

	return rc;
}

int
famfs_parse_file_yaml(
	yaml_parser_t *parser,
	struct famfs_file_meta *fm,
	int max_extents,
	int verbose)
{
	yaml_event_t event;
	int done = 0;
	char *current_key = NULL;
	int rc = 0;

	/* "file" stanza starts wtiha  YAML_MAPPING_START_EVENT */
	GET_YAML_EVENT_OR_GOTO(parser, &event, YAML_MAPPING_START_EVENT, rc, err_out, verbose);

	while (!done) {
		yaml_event_t val_event;

		GET_YAML_EVENT(parser, &event, rc, err_out, verbose);

		switch (event.type) {
		case YAML_SCALAR_EVENT:
			current_key = (char *)event.data.scalar.value;

			if (strcmp(current_key, "path") == 0) {
				/* TODO: check for overflow */
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				strncpy((char *)fm->fm_relpath,
					(char *)val_event.data.scalar.value,
					FAMFS_MAX_PATHLEN - verbose);
				if (verbose > 1) printf("%s: path: %s\n", __func__, fm->fm_relpath);
				yaml_event_delete(&val_event);
			} else if (strcmp(current_key, "size") == 0) {
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_size = strtoull((char *)val_event.data.scalar.value,
						       0, 0);
				yaml_event_delete(&val_event);
				if (verbose > 1) printf("%s: size: 0x%llx\n",
							__func__, fm->fm_size);
			} else if (strcmp(current_key, "flags") == 0) {
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_flags = strtoull((char *)val_event.data.scalar.value,
							0, 0);
				yaml_event_delete(&val_event);
				if (verbose > 1) printf("%s: flags: 0x%x\n",
							__func__, fm->fm_flags);
			} else if (strcmp(current_key, "mode") == 0) {
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_mode = strtoull((char *)val_event.data.scalar.value,
						       0, 0);
				yaml_event_delete(&val_event);
				if (verbose > 1) printf("%s: mode: 0%o\n", __func__, fm->fm_mode);
			} else if (strcmp(current_key, "uid") == 0) {
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_uid = strtoull((char *)val_event.data.scalar.value,
						      0, 0);
				yaml_event_delete(&val_event);
				if (verbose > 1) printf("%s: uid: %d\n", __func__, fm->fm_uid);
			} else if (strcmp(current_key, "gid") == 0) {
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_gid = strtoull((char *)val_event.data.scalar.value,
						      0, 0);
				yaml_event_delete(&val_event);
				if (verbose > 1) printf("%s: gid: %d\n", __func__, fm->fm_gid);
			} else if (strcmp(current_key, "nextents") == 0) {
				GET_YAML_EVENT_OR_GOTO(parser, &val_event, YAML_SCALAR_EVENT,
						       rc, err_out, verbose);
				fm->fm_nextents = strtoull((char *)val_event.data.scalar.value,
						      0, 0);
				yaml_event_delete(&val_event);
				if (verbose > 1) printf("%s: nextents: %d\n",
							__func__, fm->fm_nextents);
			} else if (strcmp(current_key, "simple_ext_list") == 0) {
				rc = famfs_parse_file_ext_list(parser, fm, max_extents,
					verbose);
				if (rc)
					goto err_out;
			} else {
				fprintf(stderr, "%s: Unrecognized scalar key %s\n",
					__func__, current_key);
				rc = -EINVAL;
				goto err_out;
			}
			current_key = NULL;
			break;

		case YAML_MAPPING_END_EVENT:
			if (verbose > 1)
				printf("%s: Finished with file yaml\n", __func__);
			done = 1;
			break;
		default:
			fprintf(stderr, "%s: unexpected libyaml event %s\n",
				__func__, yaml_event_str(event.type));
			break;
		}

		yaml_event_delete(&event);
	}

err_out:
	return rc;
}

int
famfs_parse_yaml(
	FILE *fp,
	struct famfs_file_meta *fm,
	int max_extents,
	int verbose)
{
	yaml_parser_t parser;
	yaml_event_t event;
	int rc = 0;

	if (!yaml_parser_initialize(&parser)) {
		fprintf(stderr, "Failed to initialize parser\n");
		return -1;
	}

	yaml_parser_set_input_file(&parser, fp);

	/* Look for YAML_STREAM_START_EVENT */
	GET_YAML_EVENT_OR_GOTO(&parser, &event, YAML_STREAM_START_EVENT, rc, err_out, verbose);
	yaml_event_delete(&event);

	/* Look for YAML_DOCUMENT_START_EVENT */
	GET_YAML_EVENT_OR_GOTO(&parser, &event, YAML_DOCUMENT_START_EVENT, rc, err_out, verbose);
	yaml_event_delete(&event);

	/* Look for YAML_MAPPING_START_EVENT */
	GET_YAML_EVENT_OR_GOTO(&parser, &event, YAML_MAPPING_START_EVENT, rc, err_out, verbose);
	yaml_event_delete(&event);

	/* Look for "file" stanza as scalar event
	 * Theoretically there could be other stanzas later, but this is the only one now
	 */
	GET_YAML_EVENT_OR_GOTO(&parser, &event, YAML_SCALAR_EVENT, rc, err_out, verbose);
	if (strcmp((char *)"file", (char *)event.data.scalar.value) == 0) {
		rc = famfs_parse_file_yaml(&parser, fm, max_extents, verbose);
		if (rc) {
			yaml_event_delete(&event);
			goto err_out;
		}
	}
	yaml_event_delete(&event);

	/* Look for YAML_DOCUMENT_END_EVENT */
	GET_YAML_EVENT_OR_GOTO(&parser, &event, YAML_DOCUMENT_END_EVENT, rc, err_out, verbose);
	yaml_event_delete(&event);

	/* Look for YAML_STREAM_END_EVENT */
	GET_YAML_EVENT_OR_GOTO(&parser, &event, YAML_STREAM_END_EVENT, rc, err_out, verbose);
	yaml_event_delete(&event);

err_out:
	yaml_parser_delete(&parser);
	return rc;
}
