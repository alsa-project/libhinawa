// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINAWA_ENUM_TYPES_H__
#define __ORG_KERNEL_HINAWA_ENUM_TYPES_H__

G_BEGIN_DECLS

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
 * Since: 1.0
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
 * Since: 1.0
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
 * HinawaFwReqError:
 * @HINAWA_FW_REQ_ERROR_CONFLICT_ERROR:	For error of conflicting.
 * @HINAWA_FW_REQ_ERROR_DATA_ERROR:	For error of data.
 * @HINAWA_FW_REQ_ERROR_TYPE_ERROR:	For error of type.
 * @HINAWA_FW_REQ_ERROR_ADDRESS_ERROR:	For error of address.
 * @HINAWA_FW_REQ_ERROR_SEND_ERROR:	For error of sending.
 * @HINAWA_FW_REQ_ERROR_CANCELLED:	For cancellation.
 * @HINAWA_FW_REQ_ERROR_BUSY:		For busyness.
 * @HINAWA_FW_REQ_ERROR_GENERATION:	For generation.
 * @HINAWA_FW_REQ_ERROR_NO_ACK:		For no acknowledge.
 * @HINAWA_FW_REQ_ERROR_INVALID:	For rcode out of specification.
 *
 * A set of error code for [struct@GLib.Error] for operations in [class@FwReq].
 * The actual value is equivalent to [enum@FwRcode].
 *
 * Since: 2.5.
 */
typedef enum {
	HINAWA_FW_REQ_ERROR_CONFLICT_ERROR	= HINAWA_FW_RCODE_CONFLICT_ERROR,
	HINAWA_FW_REQ_ERROR_DATA_ERROR		= HINAWA_FW_RCODE_DATA_ERROR,
	HINAWA_FW_REQ_ERROR_TYPE_ERROR		= HINAWA_FW_RCODE_TYPE_ERROR,
	HINAWA_FW_REQ_ERROR_ADDRESS_ERROR	= HINAWA_FW_RCODE_ADDRESS_ERROR,
	HINAWA_FW_REQ_ERROR_SEND_ERROR		= HINAWA_FW_RCODE_SEND_ERROR,
	HINAWA_FW_REQ_ERROR_CANCELLED		= HINAWA_FW_RCODE_CANCELLED,
	HINAWA_FW_REQ_ERROR_BUSY		= HINAWA_FW_RCODE_BUSY,
	HINAWA_FW_REQ_ERROR_GENERATION		= HINAWA_FW_RCODE_GENERATION,
	HINAWA_FW_REQ_ERROR_NO_ACK		= HINAWA_FW_RCODE_NO_ACK,
	HINAWA_FW_REQ_ERROR_INVALID		= HINAWA_FW_RCODE_INVALID,
} HinawaFwReqError;

/**
 * HinawaFwNodeError:
 * @HINAWA_FW_NODE_ERROR_DISCONNECTED:	The node associated to the instance is disconnected.
 * @HINAWA_FW_NODE_ERROR_OPENED:	The instance is already associated to node by opening
 *					firewire character device.
 * @HINAWA_FW_NODE_ERROR_NOT_OPENED:	The instance is not associated to node by opening
 *					firewire character device.
 * @HINAWA_FW_NODE_ERROR_FAILED:	The system call fails.
 *
 * A set of error code for [struct@GLib.Error] for operations in [class@FwNode].
 *
 * Since: 2.1
 */
typedef enum {
	HINAWA_FW_NODE_ERROR_DISCONNECTED,
	HINAWA_FW_NODE_ERROR_OPENED,
	HINAWA_FW_NODE_ERROR_NOT_OPENED,
	HINAWA_FW_NODE_ERROR_FAILED,
} HinawaFwNodeError;

/**
 * HinawaFwRespError:
 * @HINAWA_FW_RESP_ERROR_FAILED:		The generic system call error.
 * @HINAWA_FW_RESP_ERROR_RESERVED:		The instance is already associated to reserved address range.
 * @HINAWA_FW_RESP_ERROR_ADDR_SPACE_USED:	The address space is used exclusively.
 *
 * A set of error code for [struct@GLib.Error] for operations in [class@FwResp].
 *
 * Since: 2.2
 */
typedef enum {
	HINAWA_FW_RESP_ERROR_FAILED = 0,
	HINAWA_FW_RESP_ERROR_RESERVED,
	HINAWA_FW_RESP_ERROR_ADDR_SPACE_USED,
} HinawaFwRespError;

/**
 * HinawaFwFcpError:
 * @HINAWA_FW_FCP_ERROR_TIMEOUT:	The transaction is canceled due to response timeout.
 * @HINAWA_FW_FCP_ERROR_LARGE_RESP:	The size of response is larger than expected.
 * @HINAWA_FW_FCP_ERROR_ABORTED:	The transaction is aborted due to bus reset.
 *
 * A set of error code for [struct@GLib.Error] for operations in [class@FwFcp].
 *
 * Since: 2.1
 */
typedef enum {
	HINAWA_FW_FCP_ERROR_TIMEOUT,
	HINAWA_FW_FCP_ERROR_LARGE_RESP,
	HINAWA_FW_FCP_ERROR_ABORTED,
} HinawaFwFcpError;

G_END_DECLS

#endif
