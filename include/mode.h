/* Tethys, mode.h -- mode processing
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_MODE_H__
#define __INC_MODE_H__

#define MAX_MODES 256

typedef struct u_listent u_listent;
typedef struct u_mode_info u_mode_info;
typedef struct u_mode_ctx u_mode_ctx;
typedef struct u_mode_stacker u_mode_stacker;
typedef struct u_modes u_modes;

#define MAXBANLIST  50

struct u_listent {
	char mask[256];
	char setter[256];
	u_ts_t time;
	mowgli_node_t n;
};

#include "msg.h"

typedef enum u_mode_type {
	MODE_EXTERNAL,
	MODE_STATUS,
	MODE_FLAG,
	MODE_LIST,
} u_mode_type;

#define MODE_OPER_ONLY         0x0001
#define MODE_UNSET_ONLY        0x0002

struct u_mode_info {
	char ch;
	u_mode_type type;
	ulong flags;

	union {
		int (*fn)(u_modes*, int on, char *arg);
		ulong data;
	} arg;
};

struct u_mode_ctx {
	u_mode_info *infotab;

	ulong (*get_flag_bits)(u_modes*);
	bool (*set_flag_bits)(u_modes*, ulong); /* |= */
	bool (*reset_flag_bits)(u_modes*, ulong); /* &= ~ */

	void *(*get_status_target)(u_modes*, char*);
	bool (*set_status_bits)(u_modes*, void *tgt, ulong);
	bool (*reset_status_bits)(u_modes*, void *tgt, ulong);

	mowgli_list_t *(*get_list)(u_modes*, u_mode_info*);
};

struct u_mode_stacker {
	void (*start)(u_modes*);
	void (*end)(u_modes*);

	void (*put_external)(u_modes*, int on, char *param);
	void (*put_status)(u_modes*, int on, void *tgt);
	void (*put_flag)(u_modes*, int on);
	void (*put_listent)(u_modes*, int on, u_listent*);

	void (*send_list)(u_modes*);
};

#define MODE_ERR_UNK_CHAR         0x0001
#define MODE_ERR_NO_ACCESS        0x0002
#define MODE_ERR_NOT_OPER         0x0004
#define MODE_ERR_SET_UNSET_ONLY   0x0008
#define MODE_ERR_MISSING_PARAM    0x0010
#define MODE_ERR_LIST_FULL        0x0020

struct u_modes {
	u_mode_ctx *ctx;
	u_mode_stacker *stacker;
	u_sourceinfo *setter;
	void *target;
	void *access;

	u_mode_info *info;

	ulong errors;
	char unk[MAX_MODES+1];
	void *stack; /* private use, by stacker */
};

extern int u_mode_process(u_modes *m, int parc, char **parv);

#endif
