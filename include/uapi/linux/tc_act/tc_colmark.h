/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __LINUX_TC_COLMARK_H
#define __LINUX_TC_COLMARK_H

#include <linux/pkt_cls.h>

#define TCA_ACT_COLMARK		27

#define COLMARK_F_MODE		0x1
#define COLMARK_F_MARKER	0x2
#define COLMARK_F_MTYPE		0x4

enum tc_drop_precedence {
	NO_MARKING,
	INTERNAL = 1,
	DEI = 2,
	PCP_8P0D = 3,
	PCP_7P1D = 4,
	PCP_6P2D = 5,
	PCP_5P3D = 6,
	DSCP_AF = 7,
};

enum tc_meter_type {
	srTCM,
	trTCM,
};

struct tc_colmark {
	tc_gen;
};

enum {
	TCA_COLMARK_UNSPEC,
	TCA_COLMARK_TM,
	TCA_COLMARK_PARMS,
	TCA_COLMARK_MODE,
	TCA_COLMARK_DROP_PRECEDEMCE,
	TCA_COLMARK_METER_TYPE,
	TCA_COLMARK_PAD,
	__TCA_COLMARK_MAX
};
#define TCA_COLMARK_MAX (__TCA_COLMARK_MAX - 1)

#endif
