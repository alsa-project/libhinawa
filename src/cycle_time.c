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
void hinawa_cycle_time_get_system_time(const HinawaCycleTime *self, gint64 *tv_sec, gint32 *tv_nsec)
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
void hinawa_cycle_time_get_clock_id(const HinawaCycleTime *self, gint *clock_id)
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
 * @fields: (array fixed-size=3) (out caller-allocates): The value of cycle time register of 1394
 *	    OHCI, including three elements; second, cycle, and offset in its order.
 *
 * Get the value of cycle time in 1394 OHCI controller. The first element of array expresses the
 * value of sec field, up to 127. The second element of array expresses the value of cycle field,
 * up to 7999. The third element of array expresses the value of offset field, up to 3071.
 *
 * Since: 2.6.
 */
void hinawa_cycle_time_get_fields(const HinawaCycleTime *self, guint16 fields[3])
{
	fields[0] = ieee1394_cycle_time_to_sec(self->cycle_timer);
	fields[1] = ieee1394_cycle_time_to_cycle(self->cycle_timer);
	fields[2] = ieee1394_cycle_time_to_offset(self->cycle_timer);
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
void hinawa_cycle_time_get_raw(const HinawaCycleTime *self, guint32 *raw)
{
	*raw = self->cycle_timer;
}

#define OHCI1394_TSTAMP_SEC_MASK	0x0000e000
#define OHCI1394_TSTAMP_SEC_SHIFT	13
#define OHCI1394_TSTAMP_CYCLE_MASK	0x00001fff
#define OHCI1394_TSTAMP_CYCLE_SHIFT	0

#define IEEE1394_SEC_MAX	128
#define OHCI1394_SEC_MAX	8

/**
 * hinawa_cycle_time_compute_tstamp:
 * @self: A [struct@CycleTime].
 * @tstamp: The value of time stamp retrieved from each context of 1394 OHCI.
 * @isoc_cycle: (array fixed-size=2) (inout): The result to parse the time stamp. The first element
 *	    is for 7 bits of second field in the format of IEEE 1394 CYCLE_TIME register, up to
 *	    127. The second element is for 13 bits of cycle field in the format, up to 7,999.
 *
 * Compute second count and cycle count from unsigned 16 bit integer value retrieved by Asynchronous
 * Transmit (AT), Asynchronous Receive(AR), Isochronous Transmit (IT), and Isochronous Receive (IR)
 * contexts of 1394 OHCI. The second count is completed with the internal value read from the
 * CYCLE_TIME register. For the precise computation, the method should be called in the condition
 * that the timing between receipt of time stamp and access to CYCLE_TIME register is within 8
 * seconds.
 *
 * Since: 2.6
 */
void hinawa_cycle_time_compute_tstamp(const HinawaCycleTime *self, guint tstamp, guint **isoc_cycle)
{
	guint tstamp_sec_low = (tstamp & OHCI1394_TSTAMP_SEC_MASK) >> OHCI1394_TSTAMP_SEC_SHIFT;
	guint curr_sec_low = ieee1394_cycle_time_to_sec(self->cycle_timer) & 0x7;
	guint sec = ieee1394_cycle_time_to_sec(self->cycle_timer);

	// Round up.
	if (tstamp_sec_low < curr_sec_low)
		sec += OHCI1394_SEC_MAX;
	sec |= tstamp_sec_low;
	sec %= IEEE1394_SEC_MAX;

	(*isoc_cycle)[0] = sec;
	(*isoc_cycle)[1] = (tstamp & OHCI1394_TSTAMP_CYCLE_MASK) >> OHCI1394_TSTAMP_CYCLE_SHIFT;
}

/**
 * hinawa_cycle_time_parse_tstamp:
 * @tstamp: The value of time stamp retrieved from each context of 1394 OHCI.
 * @isoc_cycle: (array fixed-size=2) (inout): The result to parse the time stamp. The first element
 *	    is for three order bits of second field in the format of IEEE 1394 CYCLE_TIME register,
 *	    up to 7. The second element is for 13 bits of cycle field in the format, up to 7,999.
 *
 * Parse second count and cycle count from unsigned 16 bit integer value retrieved by Asynchronous
 * Transmit (AT), Asynchronous Receive(AR), Isochronous Transmit (IT), and Isochronous Receive (IR)
 * contexts of 1394 OHCI.
 *
 * Since: 2.6
 */
void hinawa_cycle_time_parse_tstamp(guint tstamp, guint **isoc_cycle)
{
	(*isoc_cycle)[0] = (tstamp & OHCI1394_TSTAMP_SEC_MASK) >> OHCI1394_TSTAMP_SEC_SHIFT;
	(*isoc_cycle)[1] = (tstamp & OHCI1394_TSTAMP_CYCLE_MASK) >> OHCI1394_TSTAMP_CYCLE_SHIFT;
}
