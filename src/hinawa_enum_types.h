/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_ENUM_TYPES_H__
#define __ALSA_HINAWA_ENUM_TYPES_H__

#include <glib-object.h>
#include <linux/firewire-constants.h>
#include <sound/firewire.h>

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
 * @HINAWA_FW_RCODE_INVALID:		For rcode out of specification.
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
	HINAWA_FW_RCODE_INVALID,
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

/**
 * HinawaFwNodeError:
 * @HINAWA_FW_NODE_ERROR_DISCONNECTED:	The node associated to the instance is disconnected.
 * @HINAWA_FW_NODE_ERROR_OPENED:	The instance is already associated to node by opening
 *					firewire character device.
 * @HINAWA_FW_NODE_ERROR_NOT_OPENED:	The instance is not associated to node by opening
 *					firewire character device.
 * @HINAWA_FW_NODE_ERROR_FAILED:	The system call fails.
 *
 * A set of error code for GError with domain which equals to #hinawa_fw_node_error_quark().
 */
typedef enum {
	HINAWA_FW_NODE_ERROR_DISCONNECTED,
	HINAWA_FW_NODE_ERROR_OPENED,
	HINAWA_FW_NODE_ERROR_NOT_OPENED,
	HINAWA_FW_NODE_ERROR_FAILED,
} HinawaFwNodeError;

/**
 * HinawaFwFcpError:
 * @HINAWA_FW_FCP_ERROR_TIMEOUT:	The transaction is canceled due to response timeout.
 * @HINAWA_FW_FCP_ERROR_LARGE_RESP:	The size of response is larger than expected.
 */
typedef enum {
	HINAWA_FW_FCP_ERROR_TIMEOUT,
	HINAWA_FW_FCP_ERROR_LARGE_RESP,
} HinawaFwFcpError;

/**
 * HinawaSndUnitError:
 * @HINAWA_SND_UNIT_ERROR_DISCONNECTED:	The hwdep device associated to the instance is disconnected.
 * @HINAWA_SND_UNIT_ERROR_USED:		The hedep device is already in use.
 * @HINAWA_SND_UNIT_ERROR_OPENED:	The instance is already associated to unit by opening hwdep
 *					character device.
 * @HINAWA_SND_UNIT_ERROR_NOT_OPENED:	The instance is not associated to unit yet by opening hwdep
 *					character device.
 * @HINAWA_SND_UNIT_ERROR_LOCKED:	The hwdep device is already locked for kernel packet streaming.
 * @HINAWA_SND_UNIT_ERROR_UNLOCKED:	The hwdep device is not locked for kernel packet streaming yet.
 * @HINAWA_SND_UNIT_ERROR_WRONG_CLASS:	The hwdep device is not for the unit expected by the class.
 * @HINAWA_SND_UNIT_ERROR_FAILED:	The system call fails.
 *
 * A set of error code for GError with domain of #HinawaSndUnitError.
 */
typedef enum {
	HINAWA_SND_UNIT_ERROR_DISCONNECTED,
	HINAWA_SND_UNIT_ERROR_USED,
	HINAWA_SND_UNIT_ERROR_OPENED,
	HINAWA_SND_UNIT_ERROR_NOT_OPENED,
	HINAWA_SND_UNIT_ERROR_LOCKED,
	HINAWA_SND_UNIT_ERROR_UNLOCKED,
	HINAWA_SND_UNIT_ERROR_WRONG_CLASS,
	HINAWA_SND_UNIT_ERROR_FAILED,
} HinawaSndUnitError;

/**
 * HinawaSndDiceError:
 * @HINAWA_SND_DICE_ERROR_TIMEOUT:	The transaction is canceled due to response timeout.
 *
 * A set of error code for GError with domain which equals to #hinawa_snd_dice_error_quark().
 */
typedef enum {
	HINAWA_SND_DICE_ERROR_TIMEOUT,
} HinawaSndDiceError;

/**
 * HinawaSndEfwError:
 * @HINAWA_SND_EFW_ERROR_BAD:			The request or response includes invalid header.
 * @HINAWA_SND_EFW_ERROR_BAD_COMMAND:		The request includes invalid category or command.
 * @HINAWA_SND_EFW_ERROR_COMM_ERR:		The transaction fails due to communication error.
 * @HINAWA_SND_EFW_ERROR_BAD_QUAD_COUNT:	The number of quadlets in transaction is invalid.
 * @HINAWA_SND_EFW_ERROR_UNSUPPORTED:		The request is not supported.
 * @HINAWA_SND_EFW_ERROR_TIMEOUT:		The transaction is canceled due to response timeout.
 * @HINAWA_SND_EFW_ERROR_DSP_TIMEOUT:		The operation for DSP did not finish within timeout.
 * @HINAWA_SND_EFW_ERROR_BAD_RATE:		The request includes invalid value for sampling frequency.
 * @HINAWA_SND_EFW_ERROR_BAD_CLOCK:		The request includes invalid value for source of clock.
 * @HINAWA_SND_EFW_ERROR_BAD_CHANNEL:		The request includes invalid value for the number of channel.
 * @HINAWA_SND_EFW_ERROR_BAD_PAN:		The request includes invalid value for panning.
 * @HINAWA_SND_EFW_ERROR_FLASH_BUSY:		The on-board flash is busy and not operable.
 * @HINAWA_SND_EFW_ERROR_BAD_MIRROR:		The request includes invalid value for mirroring channel.
 * @HINAWA_SND_EFW_ERROR_BAD_LED:		The request includes invalid value for LED.
 * @HINAWA_SND_EFW_ERROR_BAD_PARAMETER:		The request includes invalid value of parameter.
 * @HINAWA_SND_EFW_ERROR_LARGE_RESP:		The size of response is larger than expected.
 *
 * A set of error code for GError with domain which equals to #hinawa_snd_efw_error_quark();
 */
typedef enum {
	HINAWA_SND_EFW_ERROR_BAD		= 1,
	HINAWA_SND_EFW_ERROR_BAD_COMMAND	= 2,
	HINAWA_SND_EFW_ERROR_COMM_ERR		= 3,
	HINAWA_SND_EFW_ERROR_BAD_QUAD_COUNT	= 4,
	HINAWA_SND_EFW_ERROR_UNSUPPORTED	= 5,
	HINAWA_SND_EFW_ERROR_TIMEOUT		= 6,
	HINAWA_SND_EFW_ERROR_DSP_TIMEOUT	= 7,
	HINAWA_SND_EFW_ERROR_BAD_RATE		= 8,
	HINAWA_SND_EFW_ERROR_BAD_CLOCK		= 9,
	HINAWA_SND_EFW_ERROR_BAD_CHANNEL	= 10,
	HINAWA_SND_EFW_ERROR_BAD_PAN		= 11,
	HINAWA_SND_EFW_ERROR_FLASH_BUSY		= 12,
	HINAWA_SND_EFW_ERROR_BAD_MIRROR		= 13,
	HINAWA_SND_EFW_ERROR_BAD_LED		= 14,
	HINAWA_SND_EFW_ERROR_BAD_PARAMETER	= 15,
	HINAWA_SND_EFW_ERROR_LARGE_RESP,
} HinawaSndEfwError;

#endif
