// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2026 Micron Technology, Inc.  All rights reserved.
 *
 * famfs configuration system - git-style hierarchical configuration
 * Uses libyaml for parsing (already a famfs dependency)
 */

#ifndef __maybe_unused
#define __maybe_unused __attribute__((__unused__))
#endif

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <linux/limits.h>

#include "famfs_config.h"
#include "famfs_lib.h"

/*
 * Helper: get home directory (thread-safe)
 */
static int get_home_dir(char *buf, size_t bufsize)
{
	const char *home = getenv("HOME");
	struct passwd pw;
	struct passwd *result;
	char pwbuf[1024];
	size_t len;

	if (home) {
		len = strlen(home);
		if (len >= bufsize)
			return -ENAMETOOLONG;
		memcpy(buf, home, len + 1);
		return 0;
	}

	/* Fall back to passwd database (thread-safe version) */
	if (getpwuid_r(getuid(), &pw, pwbuf, sizeof(pwbuf), &result) != 0 ||
	    !result)
		return -ENOENT;

	len = strlen(pw.pw_dir);
	if (len >= bufsize)
		return -ENAMETOOLONG;

	memcpy(buf, pw.pw_dir, len + 1);
	return 0;
}

/*
 * Helper: parse integer with validation
 */
static int parse_int(const char *val, int *out, int min_val, int max_val)
{
	char *endptr;
	long v;

	errno = 0;
	v = strtol(val, &endptr, 0);
	if (errno != 0 || endptr == val || *endptr != '\0')
		return -EINVAL;
	if (v < min_val || v > max_val)
		return -ERANGE;

	*out = (int)v;
	return 0;
}

/*
 * Helper: parse u64 with validation
 */
static int parse_u64(const char *val, u64 *out, u64 min_val, u64 max_val)
{
	char *endptr;
	unsigned long long v;

	errno = 0;
	v = strtoull(val, &endptr, 0);
	if (errno != 0 || endptr == val || *endptr != '\0')
		return -EINVAL;
	if (v < min_val || v > max_val)
		return -ERANGE;

	*out = (u64)v;
	return 0;
}

/*
 * Validate config file ownership and permissions
 * System config: must be owned by root
 * User config: must be owned by current user
 */
static int validate_config_file(const char *filepath, enum famfs_config_scope scope)
{
	struct stat st;

	if (stat(filepath, &st) != 0)
		return (errno == ENOENT) ? 1 : -errno;

	if (!S_ISREG(st.st_mode))
		return -EINVAL;

	if (scope == FAMFS_CONFIG_SCOPE_SYSTEM) {
		if (st.st_uid != 0) {
			fprintf(stderr,
				"%s: system config %s not owned by root (uid=%d)\n",
				__func__, filepath, st.st_uid);
			return -EPERM;
		}
	} else if (scope == FAMFS_CONFIG_SCOPE_USER) {
		if (st.st_uid != getuid()) {
			fprintf(stderr,
				"%s: user config %s not owned by current user\n",
				__func__, filepath);
			return -EPERM;
		}
	}

	return 0;
}

const char *famfs_config_scope_name(enum famfs_config_scope scope)
{
	switch (scope) {
	case FAMFS_CONFIG_SCOPE_SYSTEM:
		return "system";
	case FAMFS_CONFIG_SCOPE_USER:
		return "user";
	case FAMFS_CONFIG_SCOPE_LOCAL:
		return "local";
	default:
		return "unknown";
	}
}

void famfs_config_init(struct famfs_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
}

int famfs_config_get_path(enum famfs_config_scope scope,
			  const char *shadow_path,
			  char *path_out, size_t path_size)
{
	char home[PATH_MAX];
	int rc;

	switch (scope) {
	case FAMFS_CONFIG_SCOPE_SYSTEM:
		rc = snprintf(path_out, path_size, "%s", FAMFS_SYSTEM_CONFIG_PATH);
		break;

	case FAMFS_CONFIG_SCOPE_USER:
		rc = get_home_dir(home, sizeof(home));
		if (rc < 0) {
			fprintf(stderr, "%s: cannot determine home directory\n",
				__func__);
			return rc;
		}
		rc = snprintf(path_out, path_size, "%s/%s/%s",
			      home, FAMFS_USER_CONFIG_DIR, FAMFS_CONFIG_FILENAME);
		break;

	case FAMFS_CONFIG_SCOPE_LOCAL:
		if (!shadow_path) {
			fprintf(stderr, "%s: shadow_path required for local scope\n",
				__func__);
			return -EINVAL;
		}
		rc = snprintf(path_out, path_size, "%s/%s",
			      shadow_path, FAMFS_LOCAL_CONFIG_NAME);
		break;

	default:
		fprintf(stderr, "%s: invalid scope %d\n", __func__, scope);
		return -EINVAL;
	}

	if (rc >= (int)path_size)
		return -ENAMETOOLONG;

	return 0;
}

/*
 * Parse the 'interleave' section of config YAML
 */
static int
parse_interleave_section(yaml_parser_t *parser, struct famfs_config *cfg,
			 int verbose)
{
	yaml_event_t event;
	char *current_key = NULL;
	int done = 0;
	int rc = 0;

	/* Expect YAML_MAPPING_START_EVENT */
	if (!yaml_parser_parse(parser, &event)) {
		fprintf(stderr, "%s: yaml parser error\n", __func__);
		return -EIO;
	}
	if (event.type != YAML_MAPPING_START_EVENT) {
		fprintf(stderr, "%s: expected mapping start, got %d\n",
			__func__, event.type);
		yaml_event_delete(&event);
		return -EINVAL;
	}
	yaml_event_delete(&event);

	while (!done) {
		if (!yaml_parser_parse(parser, &event)) {
			fprintf(stderr, "%s: yaml parser error\n", __func__);
			free(current_key);
			return -EIO;
		}

		switch (event.type) {
		case YAML_SCALAR_EVENT:
			if (!current_key) {
				/* This is a key */
				current_key = strdup((char *)event.data.scalar.value);
				if (!current_key) {
					yaml_event_delete(&event);
					return -ENOMEM;
				}
			} else {
				/* This is a value */
				char *val = (char *)event.data.scalar.value;
				u64 tmpval;

				if (strcmp(current_key, "nbuckets") == 0) {
					if (parse_u64(val, &tmpval, 1, 1024) == 0) {
						cfg->nbuckets = tmpval;
						cfg->nbuckets_set = true;
						if (verbose)
							printf("  config: interleave.nbuckets = %llu\n",
							       (unsigned long long)cfg->nbuckets);
					} else if (verbose) {
						fprintf(stderr,
							"  config: invalid nbuckets '%s'\n",
							val);
					}
				} else if (strcmp(current_key, "nstrips") == 0) {
					if (parse_u64(val, &tmpval, 1, 256) == 0) {
						cfg->nstrips = tmpval;
						cfg->nstrips_set = true;
						if (verbose)
							printf("  config: interleave.nstrips = %llu\n",
							       (unsigned long long)cfg->nstrips);
					} else if (verbose) {
						fprintf(stderr,
							"  config: invalid nstrips '%s'\n",
							val);
					}
				} else if (strcmp(current_key, "chunk_size") == 0) {
					char *endptr;
					s64 mult;

					errno = 0;
					tmpval = strtoull(val, &endptr, 0);
					if (errno == 0 && endptr != val) {
						mult = get_multiplier(endptr);
						if (mult > 0)
							tmpval *= mult;
						if (tmpval >= 4096 &&
						    tmpval <= (1ULL << 30)) {
							cfg->chunk_size = tmpval;
							cfg->chunk_size_set = true;
							if (verbose)
								printf("  config: interleave.chunk_size = %llu\n",
								       (unsigned long long)cfg->chunk_size);
						} else if (verbose) {
							fprintf(stderr,
								"  config: chunk_size out of range\n");
						}
					} else if (verbose) {
						fprintf(stderr,
							"  config: invalid chunk_size '%s'\n",
							val);
					}
				}
				/* Unknown keys silently ignored for forward compat */

				free(current_key);
				current_key = NULL;
			}
			break;

		case YAML_MAPPING_END_EVENT:
			done = 1;
			break;

		default:
			break;
		}

		yaml_event_delete(&event);
	}

	if (current_key)
		free(current_key);

	return rc;
}

/*
 * Parse the 'core' section of config YAML
 */
static int
parse_core_section(yaml_parser_t *parser, struct famfs_config *cfg,
		   int verbose)
{
	yaml_event_t event;
	char *current_key = NULL;
	int done = 0;
	int rc = 0;

	/* Expect YAML_MAPPING_START_EVENT */
	if (!yaml_parser_parse(parser, &event)) {
		fprintf(stderr, "%s: yaml parser error\n", __func__);
		return -EIO;
	}
	if (event.type != YAML_MAPPING_START_EVENT) {
		fprintf(stderr, "%s: expected mapping start, got %d\n",
			__func__, event.type);
		yaml_event_delete(&event);
		return -EINVAL;
	}
	yaml_event_delete(&event);

	while (!done) {
		if (!yaml_parser_parse(parser, &event)) {
			fprintf(stderr, "%s: yaml parser error\n", __func__);
			free(current_key);
			return -EIO;
		}

		switch (event.type) {
		case YAML_SCALAR_EVENT:
			if (!current_key) {
				current_key = strdup((char *)event.data.scalar.value);
				if (!current_key) {
					yaml_event_delete(&event);
					return -ENOMEM;
				}
			} else {
				char *val = (char *)event.data.scalar.value;
				int tmpval;

				if (strcmp(current_key, "verbose") == 0) {
					if (parse_int(val, &tmpval, 0, 10) == 0) {
						cfg->verbose = tmpval;
						cfg->verbose_set = true;
						if (verbose)
							printf("  config: core.verbose = %d\n",
							       cfg->verbose);
					} else if (verbose) {
						fprintf(stderr,
							"  config: invalid verbose '%s'\n",
							val);
					}
				} else if (strcmp(current_key, "threadct") == 0) {
					if (parse_int(val, &tmpval, 1, 1024) == 0) {
						cfg->threadct = tmpval;
						cfg->threadct_set = true;
						if (verbose)
							printf("  config: core.threadct = %d\n",
							       cfg->threadct);
					} else if (verbose) {
						fprintf(stderr,
							"  config: invalid threadct '%s'\n",
							val);
					}
				}

				free(current_key);
				current_key = NULL;
			}
			break;

		case YAML_MAPPING_END_EVENT:
			done = 1;
			break;

		default:
			break;
		}

		yaml_event_delete(&event);
	}

	if (current_key)
		free(current_key);

	return rc;
}

int famfs_config_load_file(struct famfs_config *cfg, const char *filepath,
			   int scope, int verbose)
{
	yaml_parser_t parser;
	yaml_event_t event;
	FILE *fp;
	int done = 0;
	int rc = 0;
	char *current_section = NULL;

	/* Validate file ownership if scope is specified */
	if (scope >= 0 && scope < FAMFS_CONFIG_SCOPE_COUNT) {
		rc = validate_config_file(filepath, (enum famfs_config_scope)scope);
		if (rc != 0)
			return rc;
	}

	fp = fopen(filepath, "r");
	if (!fp) {
		if (errno == ENOENT)
			return 1; /* File doesn't exist - not an error */
		fprintf(stderr, "%s: failed to open %s: %s\n",
			__func__, filepath, strerror(errno));
		return -errno;
	}

	if (verbose)
		printf("Loading config: %s\n", filepath);

	if (!yaml_parser_initialize(&parser)) {
		fprintf(stderr, "%s: failed to initialize yaml parser\n", __func__);
		fclose(fp);
		return -EIO;
	}

	yaml_parser_set_input_file(&parser, fp);

	/* Process YAML events */
	while (!done) {
		if (!yaml_parser_parse(&parser, &event)) {
			fprintf(stderr, "%s: yaml parse error in %s: %s\n",
				__func__, filepath, parser.problem);
			rc = -1;
			break;
		}

		switch (event.type) {
		case YAML_STREAM_END_EVENT:
			done = 1;
			break;

		case YAML_SCALAR_EVENT:
			/* Top-level scalar is a section name */
			if (current_section)
				free(current_section);
			current_section = strdup((char *)event.data.scalar.value);
			if (!current_section) {
				rc = -ENOMEM;
				done = 1;
				break;
			}

			if (strcmp(current_section, "interleave") == 0) {
				rc = parse_interleave_section(&parser, cfg, verbose);
				if (rc)
					done = 1;
			} else if (strcmp(current_section, "core") == 0) {
				rc = parse_core_section(&parser, cfg, verbose);
				if (rc)
					done = 1;
			}
			/* Unknown sections silently skipped */
			break;

		default:
			break;
		}

		yaml_event_delete(&event);
	}

	if (current_section)
		free(current_section);

	yaml_parser_delete(&parser);
	fclose(fp);
	return rc;
}

int famfs_config_load(struct famfs_config *cfg, const char *shadow_path,
		      int verbose)
{
	char path[PATH_MAX];
	int rc;
	int i;

	famfs_config_init(cfg);

	/* Load configs in priority order (system -> user -> local) */
	for (i = 0; i < FAMFS_CONFIG_SCOPE_COUNT; i++) {
		/* Skip local scope if no shadow_path provided */
		if (i == FAMFS_CONFIG_SCOPE_LOCAL && !shadow_path)
			continue;

		rc = famfs_config_get_path((enum famfs_config_scope)i,
					   shadow_path, path, sizeof(path));
		if (rc == 0)
			famfs_config_load_file(cfg, path, i, verbose);
		/* Errors loading individual files are not fatal */
	}

	return 0;
}

void famfs_config_apply_interleave(const struct famfs_config *cfg,
				   struct famfs_interleave_param *ip)
{
	if (cfg->nbuckets_set)
		ip->nbuckets = cfg->nbuckets;
	if (cfg->nstrips_set)
		ip->nstrips = cfg->nstrips;
	if (cfg->chunk_size_set)
		ip->chunk_size = cfg->chunk_size;
}

/*
 * Ensure config directory exists (TOCTOU-safe)
 */
static int ensure_config_dir(enum famfs_config_scope scope,
			     const char *shadow_path)
{
	char path[PATH_MAX];
	char home[PATH_MAX];
	struct stat st;
	int rc;

	if (scope == FAMFS_CONFIG_SCOPE_SYSTEM)
		return 0; /* /etc should exist */

	if (scope == FAMFS_CONFIG_SCOPE_LOCAL) {
		/* shadow_path directory should already exist */
		if (!shadow_path)
			return -EINVAL;
		if (stat(shadow_path, &st) == 0 && S_ISDIR(st.st_mode))
			return 0;
		return -ENOENT;
	}

	/* User scope - use mkdir directly to avoid TOCTOU race */
	rc = get_home_dir(home, sizeof(home));
	if (rc < 0)
		return rc;

	if (snprintf(path, sizeof(path), "%s/%s", home, FAMFS_USER_CONFIG_DIR)
	    >= (int)sizeof(path))
		return -ENAMETOOLONG;

	if (mkdir(path, 0700) < 0) {
		if (errno == EEXIST) {
			/* Check it's actually a directory */
			if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
				return 0;
			return -ENOTDIR;
		}
		return -errno;
	}

	return 0;
}

/* Context for YAML config set operation */
struct config_set_ctx {
	yaml_parser_t *parser;
	yaml_emitter_t *emitter;
	yaml_event_t *event;
	const char *param;
	const char *value;
	bool in_target_section;
	bool key_written;
};

/* Helper to emit a YAML scalar */
static void emit_scalar(yaml_emitter_t *emitter, yaml_event_t *event,
			const char *val)
{
	yaml_scalar_event_initialize(event, NULL, NULL,
				     (yaml_char_t *)val, -1, 1, 1,
				     YAML_PLAIN_SCALAR_STYLE);
	yaml_emitter_emit(emitter, event);
}

/* Helper to emit a key-value pair */
static void emit_key_value(yaml_emitter_t *emitter, yaml_event_t *event,
			   const char *k, const char *v)
{
	emit_scalar(emitter, event, k);
	emit_scalar(emitter, event, v);
}

/* Process a scalar event within a section mapping */
static void process_section_scalar(struct config_set_ctx *ctx, char **curr_key,
				   const char *sval)
{
	if (!*curr_key) {
		*curr_key = strdup(sval);
		if (!*curr_key)
			return;
		/* Replace our key? */
		if (ctx->in_target_section &&
		    strcmp(*curr_key, ctx->param) == 0) {
			emit_key_value(ctx->emitter, ctx->event,
				       ctx->param, ctx->value);
			ctx->key_written = true;
			/* Skip old value */
			yaml_event_t skip;

			yaml_parser_parse(ctx->parser, &skip);
			yaml_event_delete(&skip);
		} else {
			emit_scalar(ctx->emitter, ctx->event, *curr_key);
		}
	} else {
		emit_scalar(ctx->emitter, ctx->event, sval);
		free(*curr_key);
		*curr_key = NULL;
	}
}

/* Process the contents of a YAML section mapping */
static void process_section_mapping(struct config_set_ctx *ctx)
{
	char *curr_key = NULL;
	bool section_done = false;

	yaml_mapping_start_event_initialize(ctx->event, NULL, NULL, 1,
					    YAML_BLOCK_MAPPING_STYLE);
	if (!yaml_emitter_emit(ctx->emitter, ctx->event)) {
		fprintf(stderr, "%s: yaml emitter error\n", __func__);
		return;
	}

	while (!section_done) {
		yaml_event_t inner;

		if (!yaml_parser_parse(ctx->parser, &inner)) {
			section_done = true;
			break;
		}

		if (inner.type == YAML_MAPPING_END_EVENT) {
			if (ctx->in_target_section && !ctx->key_written) {
				emit_key_value(ctx->emitter, ctx->event,
					       ctx->param, ctx->value);
				ctx->key_written = true;
			}
			yaml_mapping_end_event_initialize(ctx->event);
			yaml_emitter_emit(ctx->emitter, ctx->event);
			section_done = true;
		} else if (inner.type == YAML_SCALAR_EVENT) {
			char *sval = (char *)inner.data.scalar.value;

			process_section_scalar(ctx, &curr_key, sval);
		}
		yaml_event_delete(&inner);
	}

	/* Free any leaked curr_key on early exit */
	free(curr_key);
}

int famfs_config_set(const char *key, const char *value,
		     enum famfs_config_scope scope, const char *shadow_path)
{
	char path[PATH_MAX];
	char tmppath[PATH_MAX + 8];
	FILE *fp_in = NULL;
	FILE *fp_out = NULL;
	yaml_parser_t parser;
	yaml_emitter_t emitter;
	yaml_event_t event;
	char *section = NULL;
	char *param = NULL;
	char *dot;
	int rc;
	bool key_written = false;
	bool parser_initialized = false;
	bool emitter_initialized = false;

	/* Parse "section.param" key format */
	section = strdup(key);
	if (!section)
		return -ENOMEM;

	dot = strchr(section, '.');
	if (!dot) {
		fprintf(stderr, "%s: invalid key format '%s' (expected section.param)\n",
			__func__, key);
		free(section);
		return -EINVAL;
	}
	*dot = '\0';
	param = dot + 1;

	rc = ensure_config_dir(scope, shadow_path);
	if (rc < 0) {
		free(section);
		return rc;
	}

	rc = famfs_config_get_path(scope, shadow_path, path, sizeof(path));
	if (rc < 0) {
		free(section);
		return rc;
	}

	snprintf(tmppath, sizeof(tmppath), "%s.tmp", path);

	/* Open output file */
	fp_out = fopen(tmppath, "w");
	if (!fp_out) {
		fprintf(stderr, "%s: cannot create %s: %s\n",
			__func__, tmppath, strerror(errno));
		free(section);
		return -errno;
	}

	/* Initialize emitter */
	if (!yaml_emitter_initialize(&emitter)) {
		fprintf(stderr, "%s: failed to initialize yaml emitter\n", __func__);
		fclose(fp_out);
		unlink(tmppath);
		free(section);
		return -1;
	}
	emitter_initialized = true;
	yaml_emitter_set_output_file(&emitter, fp_out);

	/* Start YAML document */
	yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
	yaml_emitter_emit(&emitter, &event);
	yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
	yaml_emitter_emit(&emitter, &event);
	yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
	yaml_emitter_emit(&emitter, &event);

	/* Try to read existing file */
	fp_in = fopen(path, "r");
	if (fp_in) {
		if (!yaml_parser_initialize(&parser)) {
			fclose(fp_in);
			fp_in = NULL;
		} else {
			parser_initialized = true;
			yaml_parser_set_input_file(&parser, fp_in);
		}
	}

	if (fp_in && parser_initialized) {
		struct config_set_ctx ctx = {
			.parser = &parser,
			.emitter = &emitter,
			.event = &event,
			.param = param,
			.value = value,
			.in_target_section = false,
			.key_written = false,
		};
		char *current_section = NULL;
		bool done = false;

		while (!done) {
			yaml_event_t map_event;

			if (!yaml_parser_parse(&parser, &event)) {
				done = true;
				break;
			}

			switch (event.type) {
			case YAML_STREAM_END_EVENT:
				done = true;
				break;

			case YAML_SCALAR_EVENT:
				if (current_section != NULL)
					break;

				/* Section name */
				current_section = strdup((char *)event.data.scalar.value);
				if (!current_section) {
					done = true;
					break;
				}
				ctx.in_target_section =
					(strcmp(current_section, section) == 0);

				emit_scalar(&emitter, &event, current_section);

				if (!yaml_parser_parse(&parser, &map_event) ||
				    map_event.type != YAML_MAPPING_START_EVENT) {
					yaml_event_delete(&map_event);
					free(current_section);
					current_section = NULL;
					break;
				}

				process_section_mapping(&ctx);
				yaml_event_delete(&map_event);
				free(current_section);
				current_section = NULL;
				break;

			default:
				break;
			}

			yaml_event_delete(&event);
		}
		key_written = ctx.key_written;
	}

	/* If key not yet written, add new section */
	if (!key_written) {
		/* Section name */
		yaml_scalar_event_initialize(&event, NULL, NULL,
			(yaml_char_t *)section, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		yaml_emitter_emit(&emitter, &event);

		/* Section mapping */
		yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
			YAML_BLOCK_MAPPING_STYLE);
		yaml_emitter_emit(&emitter, &event);

		/* Key */
		yaml_scalar_event_initialize(&event, NULL, NULL,
			(yaml_char_t *)param, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		yaml_emitter_emit(&emitter, &event);

		/* Value */
		yaml_scalar_event_initialize(&event, NULL, NULL,
			(yaml_char_t *)value, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		yaml_emitter_emit(&emitter, &event);

		yaml_mapping_end_event_initialize(&event);
		yaml_emitter_emit(&emitter, &event);
	}

	/* End YAML document */
	yaml_mapping_end_event_initialize(&event);
	yaml_emitter_emit(&emitter, &event);
	yaml_document_end_event_initialize(&event, 1);
	yaml_emitter_emit(&emitter, &event);
	yaml_stream_end_event_initialize(&event);
	yaml_emitter_emit(&emitter, &event);

	if (parser_initialized)
		yaml_parser_delete(&parser);
	if (emitter_initialized)
		yaml_emitter_delete(&emitter);
	if (fp_in)
		fclose(fp_in);
	fclose(fp_out);

	/* Rename temp file to actual file */
	if (rename(tmppath, path) < 0) {
		fprintf(stderr, "%s: rename failed: %s\n", __func__, strerror(errno));
		unlink(tmppath);
		free(section);
		return -errno;
	}

	free(section);
	return 0;
}

int famfs_config_unset(const char *key __maybe_unused,
		       enum famfs_config_scope scope __maybe_unused,
		       const char *shadow_path __maybe_unused)
{
	/* TODO: A proper implementation would remove the key entirely */
	fprintf(stderr, "%s: not yet implemented\n", __func__);
	return -ENOTSUP;
}

int famfs_config_get(const char *key, int scope, const char *shadow_path,
		     char *value_out, size_t value_size)
{
	struct famfs_config cfg;
	char path[PATH_MAX];
	int rc;

	famfs_config_init(&cfg);

	if (scope >= 0 && scope < FAMFS_CONFIG_SCOPE_COUNT) {
		rc = famfs_config_get_path((enum famfs_config_scope)scope,
					   shadow_path, path, sizeof(path));
		if (rc < 0)
			return rc;
		rc = famfs_config_load_file(&cfg, path, scope, 0);
		if (rc != 0)
			return 1; /* Not found */
	} else {
		/* Load merged config */
		famfs_config_load(&cfg, shadow_path, 0);
	}

	/* Map key to config field */
	if (strcmp(key, "interleave.nbuckets") == 0) {
		if (!cfg.nbuckets_set)
			return 1;
		snprintf(value_out, value_size, "%llu",
			 (unsigned long long)cfg.nbuckets);
	} else if (strcmp(key, "interleave.nstrips") == 0) {
		if (!cfg.nstrips_set)
			return 1;
		snprintf(value_out, value_size, "%llu",
			 (unsigned long long)cfg.nstrips);
	} else if (strcmp(key, "interleave.chunk_size") == 0) {
		if (!cfg.chunk_size_set)
			return 1;
		snprintf(value_out, value_size, "%llu",
			 (unsigned long long)cfg.chunk_size);
	} else if (strcmp(key, "core.verbose") == 0) {
		if (!cfg.verbose_set)
			return 1;
		snprintf(value_out, value_size, "%d", cfg.verbose);
	} else if (strcmp(key, "core.threadct") == 0) {
		if (!cfg.threadct_set)
			return 1;
		snprintf(value_out, value_size, "%d", cfg.threadct);
	} else {
		return 1; /* Unknown key */
	}

	return 0;
}

int famfs_config_list(int scope, const char *shadow_path, bool show_origin,
		      FILE *fp)
{
	char path[PATH_MAX];
	struct famfs_config cfg;
	int i, rc;

	if (scope >= 0 && scope < FAMFS_CONFIG_SCOPE_COUNT) {
		/* List from specific scope */
		if (scope == FAMFS_CONFIG_SCOPE_LOCAL && !shadow_path) {
			fprintf(stderr, "Error: shadow_path required for local scope\n");
			return -EINVAL;
		}

		famfs_config_init(&cfg);
		rc = famfs_config_get_path((enum famfs_config_scope)scope,
					   shadow_path, path, sizeof(path));
		if (rc < 0)
			return rc;

		rc = famfs_config_load_file(&cfg, path, scope, 0);
		if (rc > 0)
			return 0; /* File doesn't exist */
		if (rc < 0)
			return rc;

		if (show_origin)
			fprintf(fp, "# %s (%s)\n", path,
				famfs_config_scope_name((enum famfs_config_scope)scope));

		if (cfg.nbuckets_set)
			fprintf(fp, "interleave.nbuckets=%llu\n",
				(unsigned long long)cfg.nbuckets);
		if (cfg.nstrips_set)
			fprintf(fp, "interleave.nstrips=%llu\n",
				(unsigned long long)cfg.nstrips);
		if (cfg.chunk_size_set)
			fprintf(fp, "interleave.chunk_size=%llu\n",
				(unsigned long long)cfg.chunk_size);
		if (cfg.verbose_set)
			fprintf(fp, "core.verbose=%d\n", cfg.verbose);
		if (cfg.threadct_set)
			fprintf(fp, "core.threadct=%d\n", cfg.threadct);
	} else {
		/* List merged from all scopes */
		for (i = 0; i < FAMFS_CONFIG_SCOPE_COUNT; i++) {
			/* Skip local scope if no shadow_path */
			if (i == FAMFS_CONFIG_SCOPE_LOCAL && !shadow_path)
				continue;

			famfs_config_init(&cfg);
			rc = famfs_config_get_path((enum famfs_config_scope)i,
						   shadow_path, path, sizeof(path));
			if (rc < 0)
				continue;

			rc = famfs_config_load_file(&cfg, path, i, 0);
			if (rc != 0)
				continue;

			if (show_origin)
				fprintf(fp, "# %s (%s)\n", path,
					famfs_config_scope_name((enum famfs_config_scope)i));

			if (cfg.nbuckets_set)
				fprintf(fp, "interleave.nbuckets=%llu\n",
					(unsigned long long)cfg.nbuckets);
			if (cfg.nstrips_set)
				fprintf(fp, "interleave.nstrips=%llu\n",
					(unsigned long long)cfg.nstrips);
			if (cfg.chunk_size_set)
				fprintf(fp, "interleave.chunk_size=%llu\n",
					(unsigned long long)cfg.chunk_size);
			if (cfg.verbose_set)
				fprintf(fp, "core.verbose=%d\n", cfg.verbose);
			if (cfg.threadct_set)
				fprintf(fp, "core.threadct=%d\n", cfg.threadct);
		}
	}

	return 0;
}
