// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINAWA_CYCLE_TIME_H__
#define __ORG_KERNEL_HINAWA_CYCLE_TIME_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_CYCLE_TIME		(hinawa_cycle_time_get_type())

typedef struct fw_cdev_get_cycle_timer2 HinawaCycleTime;

GType hinawa_cycle_time_get_type() G_GNUC_CONST;

HinawaCycleTime *hinawa_cycle_time_new();

void hinawa_cycle_time_get_system_time(const HinawaCycleTime *self, gint64 *tv_sec,
				       gint32 *tv_nsec);

void hinawa_cycle_time_get_clock_id(const HinawaCycleTime *self, gint *clock_id);

void hinawa_cycle_time_get_fields(const HinawaCycleTime *self, guint16 fields[3]);

void hinawa_cycle_time_get_raw(const HinawaCycleTime *self, guint32 *raw);

void hinawa_cycle_time_compute_tstamp(const HinawaCycleTime *self, guint tstamp, guint isoc_cycle[2]);

void hinawa_cycle_time_parse_tstamp(guint tstamp, guint isoc_cycle[2]);

G_END_DECLS

#endif
