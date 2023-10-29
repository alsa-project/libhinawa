// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

/**
 * HinawaFwReq:
 * A transaction executor to a node in IEEE 1394 bus.
 *
 * [class@FwReq] supports all types of transactions defiend in IEEE 1212.
 */

/**
 * hinawa_fw_req_error_quark:
 *
 * Return the [alias@GLib.Quark] for [struct@GLib.Error] with code of [enum@FwRcode].
 *
 * Since: 2.1
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinawa-fw-req-error-quark, hinawa_fw_req_error)

G_DEFINE_TYPE(HinawaFwReq, hinawa_fw_req, G_TYPE_OBJECT)

static const char *const err_labels[] = {
	[HINAWA_FW_REQ_ERROR_CONFLICT_ERROR]	= "conflict error",
	[HINAWA_FW_REQ_ERROR_DATA_ERROR]	= "data error",
	[HINAWA_FW_REQ_ERROR_TYPE_ERROR]	= "type error",
	[HINAWA_FW_REQ_ERROR_ADDRESS_ERROR]	= "address error",
	[HINAWA_FW_REQ_ERROR_SEND_ERROR]	= "send error",
	[HINAWA_FW_REQ_ERROR_CANCELLED]		= "timeout",
	[HINAWA_FW_REQ_ERROR_BUSY]		= "busy",
	[HINAWA_FW_REQ_ERROR_GENERATION]	= "bus reset",
	[HINAWA_FW_REQ_ERROR_NO_ACK]		= "no ack",
	[HINAWA_FW_REQ_ERROR_INVALID]		= "invalid",
};

static void generate_fw_req_error_with_errno(GError **error, HinawaFwReqError code, int err_no)
{
	g_set_error(error, HINAWA_FW_REQ_ERROR, code,
		    "%s %d (%s)", err_labels[code], err_no, strerror(err_no));
}

static void generate_fw_req_error_literal(GError **error, HinawaFwReqError code)
{
	g_set_error_literal(error, HINAWA_FW_REQ_ERROR, code, err_labels[code]);
}

enum fw_req_sig_type {
	FW_REQ_SIG_TYPE_RESPONDED = 0,
	FW_REQ_SIG_TYPE_COUNT,
};
static guint fw_req_sigs[FW_REQ_SIG_TYPE_COUNT] = { 0 };

static void hinawa_fw_req_class_init(HinawaFwReqClass *klass)
{
	/**
	 * HinawaFwReq::responded:
	 * @self: A [class@FwReq].
	 * @rcode: One of [enum@FwRcode].
	 * @request_tstamp: The isochronous cycle at which the request subaction was sent for the
	 *		    transaction.
	 * @response_tstamp: The isochronous cycle at which the response subaction arrived for the
	 *		     transaction.
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for the transaction.
	 * @frame_size: The number of elements of the array.
	 *
	 * Emitted when the node transfers asynchronous packet as response subaction for the
	 * transaction and the process successfully reads the content of packet from Linux firewire
	 * subsystem.
	 *
	 * The values of @request_tstamp and @response_tstamp are unsigned 16 bit integer including
	 * higher 3 bits for three low order bits of second field and the rest 13 bits for cycle
	 * field in the format of IEEE 1394 CYCLE_TIMER register.
	 *
	 * If the version of kernel ABI for Linux FireWire subsystem is less than 6, the
	 * @request_tstamp and @response_tstamp argument has invalid value (=G_MAXUINT).
	 *
	 * Since: 3.0
	 */
	fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED] =
		g_signal_new("responded",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwReqClass, responded),
			     NULL, NULL,
			     hinawa_sigs_marshal_VOID__ENUM_UINT_UINT_POINTER_UINT,
			     G_TYPE_NONE,
			     5, HINAWA_TYPE_FW_RCODE, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER,
			     G_TYPE_UINT);
}

static void hinawa_fw_req_init(HinawaFwReq *self)
{
	return;
}

/**
 * hinawa_fw_req_new:
 *
 * Instantiate [class@FwReq] object and return the instance.
 *
 * Returns: an instance of [class@FwReq].
 * Since: 1.3
 */
HinawaFwReq *hinawa_fw_req_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_REQ, NULL);
}

/**
 * hinawa_fw_req_request:
 * @self: A [class@FwReq].
 * @node: A [class@FwNode].
 * @tcode: A transaction code of [enum@FwTcode].
 * @addr: A destination address of target device
 * @length: The range of address in byte unit.
 * @frame: (array length=frame_size)(inout): An array with elements for byte data. Callers should
 *	   give it for buffer with enough space against the request since this library performs no
 *	   reallocation. Due to the reason, the value of this argument should point to the pointer
 *	   to the array and immutable. The content of array is mutable for read and lock
 *	   transaction.
 * @frame_size: The size of array in byte unit. The value of this argument should point to the
 *		numeric number and mutable for read and lock transaction.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; Hinawa.FwNodeError and
 *	   Hinawa.FwReqError.
 *
 * Execute request subaction of transaction to the given node according to given code. When the
 * response subaction arrives and running event dispatcher reads the contents,
 * [signal@FwReq::responded] signal handler is called.
 *
 * Since: 3.0
 */
gboolean hinawa_fw_req_request(HinawaFwReq *self, HinawaFwNode *node, HinawaFwTcode tcode,
			       guint64 addr, gsize length, guint8 **frame, gsize *frame_size,
			       GError **error)
{
	struct fw_cdev_send_request req = {0};
	guint generation;
	int err;

	g_return_val_if_fail(HINAWA_IS_FW_REQ(self), FALSE);
	g_return_val_if_fail(HINAWA_IS_FW_NODE(node), FALSE);
	g_return_val_if_fail(length > 0, FALSE);
	g_return_val_if_fail(frame != NULL, FALSE);
	g_return_val_if_fail(frame_size != NULL && *frame_size > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	// Should be aligned to quadlet.
	if (tcode == HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_READ_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_LOCK_MASK_SWAP ||
	    tcode == HINAWA_FW_TCODE_LOCK_COMPARE_SWAP ||
	    tcode == HINAWA_FW_TCODE_LOCK_FETCH_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_LITTLE_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_BOUNDED_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_WRAP_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT)
		g_return_val_if_fail(!(addr & 0x3) && !(length & 0x3), FALSE);

	// Should have enough space for read/written data.
	if (tcode == HINAWA_FW_TCODE_READ_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_READ_BLOCK_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST) {
		g_return_val_if_fail(*frame_size >= length, FALSE);
	} else if (tcode == HINAWA_FW_TCODE_LOCK_MASK_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_COMPARE_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_FETCH_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_LITTLE_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_BOUNDED_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_WRAP_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT) {
		g_return_val_if_fail(*frame_size >= length * 2, FALSE);
		length *= 2;
	} else {
		// Not supported due to no test.
		g_return_val_if_reached(FALSE);
	}

	// Get node property.
	g_object_get(G_OBJECT(node), "generation", &generation, NULL);

	// Setup a transaction structure.
	req.tcode = tcode;
	req.length = length;
	req.offset = addr;
	req.closure = (guint64)self;
	req.generation = generation;

	if (tcode != TCODE_READ_QUADLET_REQUEST && tcode != TCODE_READ_BLOCK_REQUEST)
		req.data = (guint64)(*frame);

	// Send this transaction.
	err = hinawa_fw_node_ioctl(node, FW_CDEV_IOC_SEND_REQUEST, &req, error);
	if (*error == NULL && err > 0)
		generate_fw_req_error_with_errno(error, HINAWA_FW_REQ_ERROR_SEND_ERROR, err);

	return err >= 0;
}

struct waiter {
	guint rcode;
	guint request_tstamp;
	guint response_tstamp;
	guint8 *frame;
	gsize length;
	GCond cond;
	GMutex mutex;
};

static void handle_responded_signal(HinawaFwReq *self, HinawaFwRcode rcode, guint request_tstamp,
				    guint response_tstamp, const guint8 *frame, guint frame_size,
				    gpointer user_data)
{
	struct waiter *w = (struct waiter *)user_data;

	g_mutex_lock(&w->mutex);

	w->rcode = rcode;

	w->request_tstamp = request_tstamp;
	w->response_tstamp = response_tstamp;

	w->length = MIN(w->length, frame_size);
	memcpy(w->frame, frame, w->length);

	// Waken a thread of an user application.
	g_cond_signal(&w->cond);

	g_mutex_unlock(&w->mutex);
}

/**
 * hinawa_fw_req_transaction_with_tstamp:
 * @self: A [class@FwReq].
 * @node: A [class@FwNode].
 * @tcode: A transaction code of [enum@FwTcode].
 * @addr: A destination address of target device
 * @length: The range of address in byte unit.
 * @frame: (array length=frame_size)(inout): An array with elements for byte data. Callers should
 *	   give it for buffer with enough space against the request since this library performs no
 *	   reallocation. Due to the reason, the value of this argument should point to the pointer
 *	   to the array and immutable. The content of array is mutable for read and lock
 *	   transaction.
 * @frame_size: The size of array in byte unit. The value of this argument should point to the
 *		numeric number and mutable for read and lock transaction.
 * @tstamp: (array fixed-size=2)(out caller-allocates): The array with two elements for time stamps.
 *	    The first element is for the isochronous cycle at which the request subaction was sent.
 *	    The second element is for the isochronous cycle at which the response subaction arrived.
 * @timeout_ms: The timeout to wait for the response subaction of transaction since the request
 *		subaction is initiated, in milliseconds.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; Hinawa.FwNodeError and
 *	   Hinawa.FwReqError.
 *
 * Execute request subaction of transaction to the given node according to given code, then wait
 * for response subaction within the given timeout.
 *
 * Each value of @tstamp is unsigned 16 bit integer including higher 3 bits for three low order bits
 * of second field and the rest 13 bits for cycle field in the format of IEEE 1394 CYCLE_TIMER register.
 *
 * If the version of kernel ABI for Linux FireWire subsystem is less than 6, each element of @tstamp
 * has invalid value (=G_MAXUINT).
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 * Since: 2.6
 */
gboolean hinawa_fw_req_transaction_with_tstamp(HinawaFwReq *self, HinawaFwNode *node,
					       HinawaFwTcode tcode, guint64 addr, gsize length,
					       guint8 **frame, gsize *frame_size, guint tstamp[2],
					       guint timeout_ms, GError **error)
{
	struct waiter w;
	gulong handler_id;
	guint64 expiration;

	g_return_val_if_fail(HINAWA_IS_FW_REQ(self), FALSE);
	g_return_val_if_fail(HINAWA_IS_FW_NODE(node), FALSE);
	g_return_val_if_fail(tstamp != NULL, FALSE);

	// This predicates against suprious wakeup.
	w.rcode = G_MAXUINT;
	w.request_tstamp = G_MAXUINT;
	w.response_tstamp = G_MAXUINT;
	w.frame = *frame;
	w.length = *frame_size;
	g_cond_init(&w.cond);
	g_mutex_init(&w.mutex);

        handler_id = g_signal_connect(self, "responded", G_CALLBACK(handle_responded_signal), &w);

	// Timeout is set in advance as a parameter of this object.
	expiration = g_get_monotonic_time() + timeout_ms * G_TIME_SPAN_MILLISECOND;

	g_object_ref(node);
	if (!hinawa_fw_req_request(self, node, tcode, addr, length, frame, frame_size, error)) {
		g_signal_handler_disconnect(self, handler_id);
		g_object_unref(node);
		return FALSE;
	}

	g_mutex_lock(&w.mutex);
	while (w.rcode == G_MAXUINT) {
		// Wait for a response with timeout, waken by the response handler.
		if (!g_cond_wait_until(&w.cond, &w.mutex, expiration))
			break;
	}
	g_signal_handler_disconnect(self, handler_id);
	g_cond_clear(&w.cond);
	g_mutex_unlock(&w.mutex);

	// Always for safe.
	hinawa_fw_node_invalidate_transaction(node, self);
	g_object_unref(node);

	if (w.rcode == G_MAXUINT) {
		generate_fw_req_error_literal(error, HINAWA_FW_REQ_ERROR_CANCELLED);
		return FALSE;
	}

	tstamp[0] = w.request_tstamp;
	tstamp[1] = w.response_tstamp;

	switch (w.rcode) {
	case RCODE_COMPLETE:
		*frame_size = w.length;
		return TRUE;
	case RCODE_CONFLICT_ERROR:
	case RCODE_DATA_ERROR:
	case RCODE_TYPE_ERROR:
	case RCODE_ADDRESS_ERROR:
	case RCODE_SEND_ERROR:
	case RCODE_CANCELLED:
	case RCODE_BUSY:
	case RCODE_GENERATION:
	case RCODE_NO_ACK:
		generate_fw_req_error_literal(error, (HinawaFwReqError)w.rcode);
		return FALSE;
	default:
		generate_fw_req_error_literal(error, HINAWA_FW_REQ_ERROR_INVALID);
		return FALSE;
	}
}

/**
 * hinawa_fw_req_transaction:
 * @self: A [class@FwReq].
 * @node: A [class@FwNode].
 * @tcode: A transaction code of [enum@FwTcode].
 * @addr: A destination address of target device
 * @length: The range of address in byte unit.
 * @frame: (array length=frame_size)(inout): An array with elements for byte data. Callers should
 *	   give it for buffer with enough space against the request since this library performs no
 *	   reallocation. Due to the reason, the value of this argument should point to the pointer
 *	   to the array and immutable. The content of array is mutable for read and lock
 *	   transaction.
 * @frame_size: The size of array in byte unit. The value of this argument should point to the
 *		numerical number and mutable for read and lock transaction.
 * @timeout_ms: The timeout to wait for response subaction of the transaction since request
 *		subaction is initiated, in milliseconds.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; Hinawa.FwNodeError and
 *	   Hinawa.FwReqError.
 *
 * Execute request subaction of transaction to the given node according to given code, then wait
 * for response subaction within the value of timeout argument. The function is a thin wrapper to
 * [method@FwReq.transaction_with_tstamp].
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 3.0
 */
gboolean hinawa_fw_req_transaction(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 **frame, gsize *frame_size, guint timeout_ms,
			       GError **error)
{
	guint tstamp[2];

	return hinawa_fw_req_transaction_with_tstamp(self, node, tcode, addr, length,
						     frame, frame_size, tstamp, timeout_ms, error);
}

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_req_handle_response(HinawaFwReq *self, const struct fw_cdev_event_response *event)
{
	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	g_signal_emit(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED], 0, event->rcode, G_MAXUINT,
		      G_MAXUINT, event->data, event->length);
}

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_req_handle_response2(HinawaFwReq *self, const struct fw_cdev_event_response2 *event)
{
	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	g_signal_emit(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED], 0, event->rcode,
		      event->request_tstamp, event->response_tstamp, event->data, event->length);
}
