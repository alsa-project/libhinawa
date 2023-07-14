// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

/**
 * HinawaFwReq:
 * A transaction executor to a FireWire unit.
 *
 * A HinawaFwReq supports some types of transactions in IEEE 1212. Mainly for read, write and lock
 * operations.
 *
 * This class is an application of Linux FireWire subsystem. All of operations utilize ioctl(2)
 * with subsystem specific request commands.
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

/* This object has one property. */
enum fw_req_prop_type {
	FW_REQ_PROP_TYPE_TIMEOUT = 1,
	FW_REQ_PROP_TYPE_COUNT,
};
static GParamSpec *fw_req_props[FW_REQ_PROP_TYPE_COUNT] = { NULL, };

typedef struct {
	guint timeout;
} HinawaFwReqPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwReq, hinawa_fw_req, G_TYPE_OBJECT)

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

static void fw_req_get_property(GObject *obj, guint id, GValue *val,
				GParamSpec *spec)
{
	HinawaFwReq *self = HINAWA_FW_REQ(obj);
	HinawaFwReqPrivate *priv = hinawa_fw_req_get_instance_private(self);

	switch (id) {
	case FW_REQ_PROP_TYPE_TIMEOUT:
		g_value_set_uint(val, priv->timeout);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_req_set_property(GObject *obj, guint id, const GValue *val,
				GParamSpec *spec)
{
	HinawaFwReq *self = HINAWA_FW_REQ(obj);
	HinawaFwReqPrivate *priv = hinawa_fw_req_get_instance_private(self);

	switch (id) {
	case FW_REQ_PROP_TYPE_TIMEOUT:
		priv->timeout = g_value_get_uint(val);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

enum fw_req_sig_type {
	FW_REQ_SIG_TYPE_RESPONDED = 1,
	FW_REQ_SIG_TYPE_RESPONDED2,
	FW_REQ_SIG_TYPE_COUNT,
};
static guint fw_req_sigs[FW_REQ_SIG_TYPE_COUNT] = { 0 };

static void hinawa_fw_req_class_init(HinawaFwReqClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_req_get_property;
	gobject_class->set_property = fw_req_set_property;

	/**
	 * HinawaFwReq:timeout:
	 *
	 * Since: 1.4
	 * Deprecated: 2.1: Use timeout_ms parameter of [method@FwReq.transaction_with_tstamp].
	 */
	fw_req_props[FW_REQ_PROP_TYPE_TIMEOUT] =
		g_param_spec_uint("timeout", "timeout",
				  "An elapse to expire waiting for response by ms unit.",
				  10, G_MAXUINT,
				  200,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED);

	g_object_class_install_properties(gobject_class,
					  FW_REQ_PROP_TYPE_COUNT,
					  fw_req_props);

	/**
	 * HinawaFwReq::responded:
	 * @self: A [class@FwReq].
	 * @rcode: One of [enum@FwRcode].
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for transaction.
	 * @frame_size: The number of elements of the array.
	 *
	 * Emitted when the unit transfers asynchronous packet as response subaction for the
	 * transaction and the process successfully reads the content of packet from Linux firewire
	 * subsystem, except for the case that [signal@FwReq::responded2] signal handler is already
	 * assigned.
	 *
	 * Since: 2.1
	 * Deprecated: 2.6: Use [signal@FwReq::responded2], instead.
	 */
	fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED] =
		g_signal_new("responded",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwReqClass, responded),
			     NULL, NULL,
			     hinawa_sigs_marshal_VOID__ENUM_POINTER_UINT,
			     G_TYPE_NONE,
			     3, HINAWA_TYPE_FW_RCODE, G_TYPE_POINTER, G_TYPE_UINT);

	/**
	 * HinawaFwReq::responded2:
	 * @self: A [class@FwReq].
	 * @rcode: One of [enum@FwRcode].
	 * @request_tstamp: The isochronous cycle at which the request was sent.
	 * @response_tstamp: The isochronous cycle at which the response arrived.
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for transaction.
	 * @frame_size: The number of elements of the array.
	 *
	 * Emitted when the unit transfers asynchronous packet as response subaction for the
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
	 * Since: 2.6
	 */
	fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED2] =
		g_signal_new("responded2",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwReqClass, responded2),
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
 * Since: 1.3.
 */
HinawaFwReq *hinawa_fw_req_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_REQ, NULL);
}

static gboolean initiate_transaction(HinawaFwReq *self, HinawaFwNode *node, HinawaFwTcode tcode,
				     guint64 addr, gsize length, guint8 *const *frame,
				     gsize *frame_size, GError **error)
{
	struct fw_cdev_send_request req = {0};
	guint64 generation;
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

	// Get unit properties.
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


/**
 * hinawa_fw_req_transaction_async:
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
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; Hinawa.FwNodeError and
 *	   Hinawa.FwReqError.
 *
 * Execute request subaction of transactions to the given node according to given code. When the
 * response subaction arrives and read the contents, [signal@FwReq::responded2] signal handler is called
 * as long as event dispatcher runs.
 *
 * Since: 2.1.
 */
void hinawa_fw_req_transaction_async(HinawaFwReq *self, HinawaFwNode *node,
				     HinawaFwTcode tcode, guint64 addr, gsize length,
				     guint8 *const *frame, gsize *frame_size,
				     GError **error)
{
	(void)initiate_transaction(self, node, tcode, addr, length, frame, frame_size, error);
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

static void handle_responded2_signal(HinawaFwReq *self, HinawaFwRcode rcode, guint request_tstamp,
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

static gboolean complete_transaction(HinawaFwReq *self, HinawaFwNode *node, HinawaFwTcode tcode,
				     guint64 addr, gsize length, guint8 *const *frame,
				     gsize *frame_size, guint timeout_ms, struct waiter *w,
				     GError **error)
{
	gulong handler_id;
	guint64 expiration;

	g_return_val_if_fail(HINAWA_IS_FW_REQ(self), FALSE);
	g_return_val_if_fail(HINAWA_IS_FW_NODE(node), FALSE);
	g_return_val_if_fail(w != NULL, FALSE);

	// This predicates against suprious wakeup.
	w->rcode = G_MAXUINT;
	w->request_tstamp = G_MAXUINT;
	w->response_tstamp = G_MAXUINT;
	w->frame = *frame;
	w->length = *frame_size;
	g_cond_init(&w->cond);
	g_mutex_init(&w->mutex);

        handler_id = g_signal_connect(self, "responded2", G_CALLBACK(handle_responded2_signal), w);

	// Timeout is set in advance as a parameter of this object.
	expiration = g_get_monotonic_time() + timeout_ms * G_TIME_SPAN_MILLISECOND;

	g_object_ref(node);
	if (!initiate_transaction(self, node, tcode, addr, length, frame, frame_size, error)) {
		g_signal_handler_disconnect(self, handler_id);
		g_object_unref(node);
		return FALSE;
	}

	g_mutex_lock(&w->mutex);
	while (w->rcode == G_MAXUINT) {
		// Wait for a response with timeout, waken by the response handler.
		if (!g_cond_wait_until(&w->cond, &w->mutex, expiration))
			break;
	}
	g_signal_handler_disconnect(self, handler_id);
	g_cond_clear(&w->cond);
	g_mutex_unlock(&w->mutex);

	// Always for safe.
	hinawa_fw_node_invalidate_transaction(node, self);
	g_object_unref(node);

	if (w->rcode == G_MAXUINT) {
		generate_fw_req_error_literal(error, HINAWA_FW_REQ_ERROR_CANCELLED);
		return FALSE;
	}

	switch (w->rcode) {
	case RCODE_COMPLETE:
		*frame_size = w->length;
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
		generate_fw_req_error_literal(error, (HinawaFwReqError)w->rcode);
		return FALSE;
	default:
		generate_fw_req_error_literal(error, HINAWA_FW_REQ_ERROR_INVALID);
		return FALSE;
	}
}

/**
 * hinawa_fw_req_transaction_sync:
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
 * @timeout_ms: The timeout to wait for response subaction of the transaction since request
 *		subaction is initiated, in milliseconds.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; Hinawa.FwNodeError and
 *	   Hinawa.FwReqError.
 *
 * Execute request subaction of transaction to the given node according to given code, then wait
 * for response subaction within the given timeout. The [property@FwReq:timeout] property of
 * instance is ignored.
 *
 * Since: 2.1.
 * Deprecated: 2.6. Use [method@FwReq.transaction_with_tstamp] instead.
 */
void hinawa_fw_req_transaction_sync(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 *const *frame, gsize *frame_size, guint timeout_ms,
			       GError **error)
{
	struct waiter w;

	(void)complete_transaction(self, node, tcode, addr, length, frame, frame_size, timeout_ms,
				   &w, error);
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
 *	    The first element is for the isochronous cycle at which the request was sent. The second
 *	    element is for the isochronous cycle at which the response arrived.
 * @timeout_ms: The timeout to wait for response subaction of the transaction since request
 *		subaction is initiated, in milliseconds.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; Hinawa.FwNodeError and
 *	   Hinawa.FwReqError.
 *
 * Execute request subaction of transaction to the given node according to given code, then wait
 * for response subaction within the given timeout. The [property@FwReq:timeout] property of
 * instance is ignored.
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
	gboolean result;

	g_return_val_if_fail(tstamp != NULL, FALSE);

	result = complete_transaction(self, node, tcode, addr, length, frame, frame_size,
				      timeout_ms, &w, error);
	if (*error == NULL) {
		tstamp[0] = w.request_tstamp;
		tstamp[1] = w.response_tstamp;
	}

	return result;
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
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; Hinawa.FwNodeError and
 *	   Hinawa.FwReqError.
 *
 * Execute request subaction of transaction to the given node according to given code, then wait
 * for response subaction within the value of timeout argument.
 *
 * Since: 1.4
 * Deprecated: 2.1: Use [method@FwReq.transaction_with_tstamp()] instead.
 */
void hinawa_fw_req_transaction(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 *const *frame, gsize *frame_size,
			       GError **error)
{
	HinawaFwReqPrivate *priv;
	struct waiter w;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));
	priv = hinawa_fw_req_get_instance_private(self);

	(void)complete_transaction(self, node, tcode, addr, length, frame, frame_size,
				   priv->timeout, &w, error);
}

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_req_handle_response(HinawaFwReq *self, const struct fw_cdev_event_response *event)
{
	HinawaFwReqClass *klass;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	klass = HINAWA_FW_REQ_GET_CLASS(self);

	if (klass->responded2 != NULL ||
	    g_signal_has_handler_pending(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED2], 0, TRUE)) {
		g_signal_emit(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED2], 0, event->rcode,
			      G_MAXUINT, G_MAXUINT, event->data, event->length);
	} else if (klass->responded != NULL ||
		   g_signal_has_handler_pending(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED], 0, TRUE)) {
		g_signal_emit(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED], 0, event->rcode,
			      event->data, event->length);
	}
}

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_req_handle_response2(HinawaFwReq *self, const struct fw_cdev_event_response2 *event)
{
	HinawaFwReqClass *klass;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	klass = HINAWA_FW_REQ_GET_CLASS(self);

	if (klass->responded2 != NULL ||
	    g_signal_has_handler_pending(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED2], 0, TRUE)) {
		g_signal_emit(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED2], 0, event->rcode,
			      event->request_tstamp, event->response_tstamp, event->data,
			      event->length);
	} else if (klass->responded != NULL ||
		   g_signal_has_handler_pending(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED], 0, TRUE)) {
		g_signal_emit(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED], 0, event->rcode,
			      event->data, event->length);
	}
}
