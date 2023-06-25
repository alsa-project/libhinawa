// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

/**
 * HinawaCycleTime:
 * A boxed object to express data of cycle time.
 *
 * A [struct@CycleTime] expresses the value of cycle time of 1394 OHCI as well as Linux system
 * time referring to clock_id.
 */
HinawaCycleTime *hinawa_cycle_time_copy(const HinawaCycleTime *self)
{
#ifdef g_memdup2
	return g_memdup2(self, sizeof(*self));
#else
	// GLib v2.68 deprecated g_memdup() with concern about overflow by narrow conversion from
	// size_t to unsigned int in ABI for LP64 data model.
	gpointer ptr = g_malloc(sizeof(*self));
	memcpy(ptr, self, sizeof(*self));
	return ptr;
#endif
}

G_DEFINE_BOXED_TYPE(HinawaCycleTime, hinawa_cycle_time, hinawa_cycle_time_copy, g_free)

/**
 * hinawa_cycle_time_new:
 *
 * Allocate and return an instance of [struct@CycleTime].
 *
 * Returns: (transfer full): An instance of [struct@CycleTime].
 *
 * Since: 2.6.
 */
HinawaCycleTime *hinawa_cycle_time_new()
{
	return g_malloc0(sizeof(HinawaCycleTime));
}

/**
 * hinawa_cycle_time_get_system_time:
 * @self: A [struct@CycleTime].
 * @tv_sec: (out caller-allocates): The second part of timestamp.
 * @tv_nsec: (out caller-allocates): The nanosecond part of timestamp.
 *
 * Get system time with enough size of strorage. The timestamp refers to clock_id available by
 * [method@CycleTime.get_clock_id].
 *
 * Since: 2.6.
 */
void hinawa_cycle_time_get_system_time(HinawaCycleTime *self, gint64 *tv_sec, gint32 *tv_nsec)
{
	*tv_sec = self->tv_sec;
	*tv_nsec = self->tv_nsec;
}

/**
 * hinawa_cycle_time_get_clock_id:
 * @self: A [struct@CycleTime].
 * @clock_id: (out caller-allocates): The numeric ID of clock source for the reference timestamp.
 *	      One of CLOCK_REALTIME(0), CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(4) is available
 *	      UAPI of Linux kernel.
 *
 * Get the ID of clock for timestamp.
 *
 * Since: 2.6.
 */
void hinawa_cycle_time_get_clock_id(HinawaCycleTime *self, gint *clock_id)
{
	*clock_id = self->clk_id;
}

#define IEEE1394_CYCLE_TIME_SEC_MASK		0xfe000000
#define IEEE1394_CYCLE_TIME_SEC_SHIFT		25
#define IEEE1394_CYCLE_TIME_CYCLE_MASK		0x01fff000
#define IEEE1394_CYCLE_TIME_CYCLE_SHIFT	12
#define IEEE1394_CYCLE_TIME_OFFSET_MASK	0x00000fff

static guint ieee1394_cycle_time_to_sec(guint32 cycle_time)
{
	return (cycle_time & IEEE1394_CYCLE_TIME_SEC_MASK) >> IEEE1394_CYCLE_TIME_SEC_SHIFT;
}

static guint ieee1394_cycle_time_to_cycle(guint32 cycle_time)
{
	return (cycle_time & IEEE1394_CYCLE_TIME_CYCLE_MASK) >> IEEE1394_CYCLE_TIME_CYCLE_SHIFT;
}

static guint ieee1394_cycle_time_to_offset(guint32 cycle_time)
{
	return cycle_time & IEEE1394_CYCLE_TIME_OFFSET_MASK;
}

/**
 * hinawa_cycle_time_get_fields:
 * @self: A [struct@CycleTime].
 * @cycle_time: (array fixed-size=3) (out caller-allocates): The value of cycle time register of
 *		 1394 OHCI, including three elements; second, cycle, and offset.
 *
 * Get the value of cycle time in 1394 OHCI controller. The first element of array expresses the
 * value of sec field, up to 127. The second element of array expresses the value of cycle field,
 * up to 7999. The third element of array expresses the value of offset field, up to 3071.
 *
 * Since: 2.6.
 */
void hinawa_cycle_time_get_fields(HinawaCycleTime *self, guint16 cycle_time[3])
{
	cycle_time[0] = ieee1394_cycle_time_to_sec(self->cycle_timer);
	cycle_time[1] = ieee1394_cycle_time_to_cycle(self->cycle_timer);
	cycle_time[2] = ieee1394_cycle_time_to_offset(self->cycle_timer);
}

/**
 * hinawa_cycle_time_get_raw:
 * @self: A [struct@CycleTime].
 * @raw: (out caller-allocates): The raw value for CYCLE_TIME register.
 *
 * Get the value of cycle time in 1394 OHCI controller.
 *
 * Since: 2.6.
 */
void hinawa_cycle_time_get_raw(HinawaCycleTime *self, guint32 *raw)
{
	*raw = self->cycle_timer;
}
