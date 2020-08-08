/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_ENUM_TYPES_H__
#define __ALSA_HINAWA_ENUM_TYPES_H__

#include <glib-object.h>
#include <linux/firewire-constants.h>

/**
 * HinawaFwTcode:
 * @HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST:	For request to write quadlet.
 * @HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST:	For request to write block.
 * @HINAWA_FW_TCODE_WRITE_RESPONSE:		For response to write.
 * @HINAWA_FW_TCODE_READ_QUADLET_REQUEST:	For response to read quadlet.
 * @HINAWA_FW_TCODE_READ_BLOCK_REQUEST:		For request to read block.
 * @HINAWA_FW_TCODE_READ_QUADLET_RESPONSE:	For response to quadlet read.
 * @HINAWA_FW_TCODE_READ_BLOCK_RESPONSE:	For response to block read.
 * @HINAWA_FW_TCODE_CYCLE_START:		For cycle start.
 * @HINAWA_FW_TCODE_LOCK_REQUEST:		For request to lock.
 * @HINAWA_FW_TCODE_STREAM_DATA:		For stream data.
 * @HINAWA_FW_TCODE_LOCK_RESPONSE:		For response to lock.
 * @HINAWA_FW_TCODE_LOCK_MASK_SWAP:		For lock request for mask-swap.
 * @HINAWA_FW_TCODE_LOCK_COMPARE_SWAP:		For lock request for compare-swap.
 * @HINAWA_FW_TCODE_LOCK_FETCH_ADD:		For lock request for fetch-add.
 * @HINAWA_FW_TCODE_LOCK_LITTLE_ADD:		For lock request for little-add.
 * @HINAWA_FW_TCODE_LOCK_BOUNDED_ADD:		For lock request for bounded-add.
 * @HINAWA_FW_TCODE_LOCK_WRAP_ADD:		For lock request for wrap-add.
 * @HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT:	For lock request for vendor-dependent.
 *
 * A representation for tcode of asynchronous transaction on IEEE 1394 bus.
 *
 */
typedef enum {
	HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST	= TCODE_WRITE_QUADLET_REQUEST,
	HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST	= TCODE_WRITE_BLOCK_REQUEST,
	HINAWA_FW_TCODE_WRITE_RESPONSE		= TCODE_WRITE_RESPONSE,
	HINAWA_FW_TCODE_READ_QUADLET_REQUEST	= TCODE_READ_QUADLET_REQUEST,
	HINAWA_FW_TCODE_READ_BLOCK_REQUEST	= TCODE_READ_BLOCK_REQUEST,
	HINAWA_FW_TCODE_READ_QUADLET_RESPONSE	= TCODE_READ_QUADLET_RESPONSE,
	HINAWA_FW_TCODE_READ_BLOCK_RESPONSE	= TCODE_READ_BLOCK_RESPONSE,
	HINAWA_FW_TCODE_CYCLE_START		= TCODE_CYCLE_START,
	HINAWA_FW_TCODE_LOCK_REQUEST		= TCODE_LOCK_REQUEST,
	HINAWA_FW_TCODE_STREAM_DATA		= TCODE_STREAM_DATA,
	HINAWA_FW_TCODE_LOCK_RESPONSE		= TCODE_LOCK_RESPONSE,
	HINAWA_FW_TCODE_LOCK_MASK_SWAP		= TCODE_LOCK_MASK_SWAP,
	HINAWA_FW_TCODE_LOCK_COMPARE_SWAP	= TCODE_LOCK_COMPARE_SWAP,
	HINAWA_FW_TCODE_LOCK_FETCH_ADD		= TCODE_LOCK_FETCH_ADD,
	HINAWA_FW_TCODE_LOCK_LITTLE_ADD		= TCODE_LOCK_LITTLE_ADD,
	HINAWA_FW_TCODE_LOCK_BOUNDED_ADD	= TCODE_LOCK_BOUNDED_ADD,
	HINAWA_FW_TCODE_LOCK_WRAP_ADD		= TCODE_LOCK_WRAP_ADD,
	HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT	= TCODE_LOCK_VENDOR_DEPENDENT,
} HinawaFwTcode;

/**
 * HinawaFwRcode:
 * @HINAWA_FW_RCODE_COMPLETE:		For completion.
 * @HINAWA_FW_RCODE_CONFLICT_ERROR:	For error of conflicting.
 * @HINAWA_FW_RCODE_DATA_ERROR:		For error of data.
 * @HINAWA_FW_RCODE_TYPE_ERROR:		For error of type.
 * @HINAWA_FW_RCODE_ADDRESS_ERROR:	For error of address.
 * @HINAWA_FW_RCODE_SEND_ERROR:		For error of sending.
 * @HINAWA_FW_RCODE_CANCELLED:		For cancellation.
 * @HINAWA_FW_RCODE_BUSY:		For busyness.
 * @HINAWA_FW_RCODE_GENERATION:		For generation.
 * @HINAWA_FW_RCODE_NO_ACK:		For no acknowledge.
 *
 * A representation for rcode of asynchronous transaction on IEEE 1394 bus.
 *
 */
typedef enum {
	HINAWA_FW_RCODE_COMPLETE	= RCODE_COMPLETE,
	HINAWA_FW_RCODE_CONFLICT_ERROR	= RCODE_CONFLICT_ERROR,
	HINAWA_FW_RCODE_DATA_ERROR	= RCODE_DATA_ERROR,
	HINAWA_FW_RCODE_TYPE_ERROR	= RCODE_TYPE_ERROR,
	HINAWA_FW_RCODE_ADDRESS_ERROR	= RCODE_ADDRESS_ERROR,
	HINAWA_FW_RCODE_SEND_ERROR	= RCODE_SEND_ERROR,
	HINAWA_FW_RCODE_CANCELLED	= RCODE_CANCELLED,
	HINAWA_FW_RCODE_BUSY		= RCODE_BUSY,
	HINAWA_FW_RCODE_GENERATION	= RCODE_GENERATION,
	HINAWA_FW_RCODE_NO_ACK		= RCODE_NO_ACK,
} HinawaFwRcode;

/**
 * HinawaSndUnitType:
 * @HINAWA_SND_UNIT_TYPE_DICE:		The type of DICE ASIC.
 * @HINAWA_SND_UNIT_TYPE_FIREWORKS:	The type of Fireworks board module.
 * @HINAWA_SND_UNIT_TYPE_BEBOB:		The type of BeBoB ASIC.
 * @HINAWA_SND_UNIT_TYPE_OXFW:		The type of OXFW ASIC
 * @HINAWA_SND_UNIT_TYPE_DIGI00X:	The type of Digi00x series.
 * @HINAWA_SND_UNIT_TYPE_TASCAM:	The type of Tascam FireWire series.
 * @HINAWA_SND_UNIT_TYPE_MOTU:		The type of MOTU FireWire series.
 * @HINAWA_SND_UNIT_TYPE_FIREFACE:	The type of RME Fireface series.
 *
 * A representation of type for sound unit defined by Linux sound subsystem.
 */
typedef enum {
	HINAWA_SND_UNIT_TYPE_DICE = 1,
	HINAWA_SND_UNIT_TYPE_FIREWORKS,
	HINAWA_SND_UNIT_TYPE_BEBOB,
	HINAWA_SND_UNIT_TYPE_OXFW,
	HINAWA_SND_UNIT_TYPE_DIGI00X,
	HINAWA_SND_UNIT_TYPE_TASCAM,
	HINAWA_SND_UNIT_TYPE_MOTU,
	HINAWA_SND_UNIT_TYPE_FIREFACE,
} HinawaSndUnitType;

#endif
