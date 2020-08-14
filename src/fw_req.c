/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "internal.h"
#include "hinawa_sigs_marshal.h"

/**
 * SECTION:fw_req
 * @Title: HinawaFwReq
 * @Short_description: A transaction executor to a FireWire unit.
 * @include: fw_req.h
 *
 * A HinawaFwReq supports some types of transactions in IEEE 1212. Mainly for
 * read, write and lock operations.
 *
 * This class is an application of Linux FireWire subsystem. All of operations
 * utilize ioctl(2) with subsystem specific request commands.
 */

/**
 * hinawa_fw_req_error_quark:
 *
 * Return the GQuark for error domain of GError which has code in #HinawaFwRcode.
 *
 * Returns: A #GQuark.
 */
G_DEFINE_QUARK(hinawa-fw-req-error-quark, hinawa_fw_req_error)

/* This object has one property. */
enum fw_req_prop_type {
	FW_REQ_PROP_TYPE_TIMEOUT = 1,
	FW_REQ_PROP_TYPE_COUNT,
};
static GParamSpec *fw_req_props[FW_REQ_PROP_TYPE_COUNT] = { NULL, };

/* NOTE: This object has no properties and no signals. */
struct _HinawaFwReqPrivate {
	guint timeout;
	guint8 *frame;
	gsize length;

	guint rcode;
	GMutex mutex;
	GCond cond;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwReq, hinawa_fw_req, G_TYPE_OBJECT)

static const char *const rcode_labels[] = {
	[RCODE_COMPLETE]	= "no error",
	[RCODE_CONFLICT_ERROR]	= "conflict error",
	[RCODE_DATA_ERROR]	= "data error",
	[RCODE_TYPE_ERROR]	= "type error",
	[RCODE_ADDRESS_ERROR]	= "address error",
	[RCODE_SEND_ERROR]	= "send error",
	[RCODE_CANCELLED]	= "timeout",
	[RCODE_BUSY]		= "busy",
	[RCODE_GENERATION]	= "bus reset",
	[RCODE_NO_ACK]		= "no ack",
};

#define generate_local_error(exception, code)						\
	g_set_error_literal(exception, HINAWA_FW_REQ_ERROR, code, rcode_labels[code])

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

static void fw_req_finalize(GObject *obj)
{
	HinawaFwReq *self = HINAWA_FW_REQ(obj);
	HinawaFwReqPrivate *priv = hinawa_fw_req_get_instance_private(self);

	g_mutex_clear(&priv->mutex);

	G_OBJECT_CLASS(hinawa_fw_req_parent_class)->finalize(obj);
}

enum fw_req_sig_type {
	FW_REQ_SIG_TYPE_RESPONDED = 1,
	FW_REQ_SIG_TYPE_COUNT,
};
static guint fw_req_sigs[FW_REQ_SIG_TYPE_COUNT] = { 0 };

static void hinawa_fw_req_class_init(HinawaFwReqClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_req_get_property;
	gobject_class->set_property = fw_req_set_property;
	gobject_class->finalize = fw_req_finalize;

	fw_req_props[FW_REQ_PROP_TYPE_TIMEOUT] =
		g_param_spec_uint("timeout", "timeout",
				  "An elapse to expire waiting for response by ms unit.",
				  10, G_MAXUINT,
				  200,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties(gobject_class,
					  FW_REQ_PROP_TYPE_COUNT,
					  fw_req_props);

	/**
	 * HinawaFwReq::responded:
	 * @self: A #HinawaFwReq.
	 * @rcode: One of #HinawaFwRcode.
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for transaction.
	 * @frame_size: The number of elements of the array.
	 *
	 * When the unit transfers asynchronous packet as response subaction for the transaction,
	 * and the process successfully reads the content of packet from Linux firewire subsystem,
	 * the ::responded signal handler is called.
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
}

static void hinawa_fw_req_init(HinawaFwReq *self)
{
	HinawaFwReqPrivate *priv = hinawa_fw_req_get_instance_private(self);

	g_mutex_init(&priv->mutex);
}

/**
 * hinawa_fw_req_new:
 *
 * Instantiate #HinawaFwReq object and return the instance.
 *
 * Returns: an instance of #HinawaFwReq.
 * Since: 1.3.
 */
HinawaFwReq *hinawa_fw_req_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_REQ, NULL);
}

/**
 * hinawa_fw_req_transaction_async:
 * @self: A #HinawaFwReq.
 * @node: A #HinawaFwNode.
 * @tcode: A transaction code of HinawaFwTcode.
 * @addr: A destination address of target device
 * @length: The range of address in byte unit.
 * @frame: (array length=frame_size)(inout): An array with elements for byte
 *	   data. Callers should give it for buffer with enough space against the
 *	   request since this library performs no reallocation. Due to the
 *	   reason, the value of this argument should point to the pointer to the
 *	   array and immutable. The content of array is mutable for read and
 *	   lock transaction.
 * @frame_size: The size of array in byte unit. The value of this argument
 *		should point to the numerical number and mutable for read and
 *		lock transaction.
 * @exception: A #GError. Error can be generated with two domains; #hinawa_fw_node_error_quark(),
 *	       and #hinawa_fw_req_error_quark().
 *
 * Execute request subaction of transactions to the given node according to given code. When the
 * response subaction arrives and read the contents, ::responded signal handler is called as long
 * as event dispatcher runs.
 *
 * Since: 2.1.
 */
void hinawa_fw_req_transaction_async(HinawaFwReq *self, HinawaFwNode *node,
				     HinawaFwTcode tcode, guint64 addr, gsize length,
				     guint8 *const *frame, gsize *frame_size,
				     GError **exception)
{
	struct fw_cdev_send_request req = {0};
	guint64 generation;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));
	g_return_if_fail(length > 0);
	g_return_if_fail(frame != NULL);
	g_return_if_fail(frame_size != NULL && *frame_size > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

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
		g_return_if_fail(!(addr & 0x3) && !(length & 0x3));

	// Should have enough space for read/written data.
	if (tcode == HINAWA_FW_TCODE_READ_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_READ_BLOCK_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST) {
		g_return_if_fail(*frame_size >= length);
	} else if (tcode == HINAWA_FW_TCODE_LOCK_MASK_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_COMPARE_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_FETCH_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_LITTLE_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_BOUNDED_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_WRAP_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT) {
		g_return_if_fail(*frame_size >= length * 2);
		length *= 2;
	} else {
		// Not supported due to no test.
		g_return_if_reached();
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
	hinawa_fw_node_ioctl(node, FW_CDEV_IOC_SEND_REQUEST, &req, exception);
}

struct waiter {
	guint rcode;
	guint8 *frame;
	gsize length;
	GCond cond;
	GMutex mutex;
};

static void handle_responded_signal(HinawaFwReq *self, HinawaFwRcode rcode, const guint8 *frame,
				    guint frame_size, gpointer user_data)
{
	struct waiter *w = (struct waiter *)user_data;

	g_mutex_lock(&w->mutex);

	w->rcode = rcode;

	w->length = MIN(w->length, frame_size);
	memcpy(w->frame, frame, w->length);

	// Waken a thread of an user application.
	g_cond_signal(&w->cond);

	g_mutex_unlock(&w->mutex);
}

/**
 * hinawa_fw_req_transaction_sync:
 * @self: A #HinawaFwReq.
 * @node: A #HinawaFwNode.
 * @tcode: A transaction code of HinawaFwTcode.
 * @addr: A destination address of target device
 * @length: The range of address in byte unit.
 * @frame: (array length=frame_size)(inout): An array with elements for byte
 *	   data. Callers should give it for buffer with enough space against the
 *	   request since this library performs no reallocation. Due to the
 *	   reason, the value of this argument should point to the pointer to the
 *	   array and immutable. The content of array is mutable for read and
 *	   lock transaction.
 * @frame_size: The size of array in byte unit. The value of this argument
 *		should point to the numerical number and mutable for read and
 *		lock transaction.
 * @timeout_ms: The timeout to wait for response subaction of the transaction since request
 *		subaction is initiated, in milliseconds.
 * @exception: A #GError. Error can be generated with two domains; #hinawa_fw_node_error_quark(),
 *	       and #hinawa_fw_req_error_quark().
 *
 * Execute request subaction of transaction to the given node according to given code, then wait
 * for response subaction within the given timeout. The ::timeout property of instance is ignored.
 *
 * Since: 2.1.
 */
void hinawa_fw_req_transaction_sync(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 *const *frame, gsize *frame_size, guint timeout_ms,
			       GError **exception)
{
	gulong handler_id;
	struct waiter w;
	guint64 expiration;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	// This predicates against suprious wakeup.
	w.rcode = G_MAXUINT;
	w.frame = *frame;
	w.length = *frame_size;
	g_cond_init(&w.cond);
	g_mutex_init(&w.mutex);

        handler_id = g_signal_connect(self, "responded", (GCallback)handle_responded_signal, &w);

	// Timeout is set in advance as a parameter of this object.
	expiration = g_get_monotonic_time() + timeout_ms * G_TIME_SPAN_MILLISECOND;

	g_object_ref(node);
	hinawa_fw_req_transaction_async(self, node, tcode, addr, length, frame, frame_size,
					exception);
	if (*exception != NULL) {
		g_signal_handler_disconnect(self, handler_id);
		g_object_unref(node);
		return;
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
		generate_local_error(exception, HINAWA_FW_RCODE_CANCELLED);
		return;
	}

	if (w.rcode != RCODE_COMPLETE) {
		switch (w.rcode) {
		case RCODE_COMPLETE:
		case RCODE_CONFLICT_ERROR:
		case RCODE_DATA_ERROR:
		case RCODE_TYPE_ERROR:
		case RCODE_ADDRESS_ERROR:
		case RCODE_SEND_ERROR:
		case RCODE_CANCELLED:
		case RCODE_BUSY:
		case RCODE_GENERATION:
		case RCODE_NO_ACK:
			break;
		default:
			w.rcode = HINAWA_FW_RCODE_INVALID;
			break;
		}

		generate_local_error(exception, w.rcode);
		return;
	}

	*frame_size = w.length;
}

/**
 * hinawa_fw_req_transaction:
 * @self: A #HinawaFwReq.
 * @node: A #HinawaFwNode.
 * @tcode: A transaction code of HinawaFwTcode.
 * @addr: A destination address of target device
 * @length: The range of address in byte unit.
 * @frame: (array length=frame_size)(inout): An array with elements for byte
 *	   data. Callers should give it for buffer with enough space against the
 *	   request since this library performs no reallocation. Due to the
 *	   reason, the value of this argument should point to the pointer to the
 *	   array and immutable. The content of array is mutable for read and
 *	   lock transaction.
 * @frame_size: The size of array in byte unit. The value of this argument
 *		should point to the numerical number and mutable for read and
 *		lock transaction.
 * @exception: A #GError. Error can be generated with two domains; #hinawa_fw_node_error_quark(),
 *	       and #hinawa_fw_req_error_quark().
 *
 * Execute transactions to the given node according to given code.
 * Since: 1.4.
 */
void hinawa_fw_req_transaction(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 *const *frame, gsize *frame_size,
			       GError **exception)
{
	struct fw_cdev_send_request req = {0};
	HinawaFwReqPrivate *priv;
	guint64 generation;
	guint64 expiration;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));
	g_return_if_fail(length > 0);
	g_return_if_fail(frame != NULL);
	g_return_if_fail(frame_size != NULL && *frame_size > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_fw_req_get_instance_private(self);

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
		g_return_if_fail(!(addr & 0x3) && !(length & 0x3));

	// Should have enough space for read/written data.
	if (tcode == HINAWA_FW_TCODE_READ_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_READ_BLOCK_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST) {
		g_return_if_fail(*frame_size >= length);
	} else if (tcode == HINAWA_FW_TCODE_LOCK_MASK_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_COMPARE_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_FETCH_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_LITTLE_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_BOUNDED_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_WRAP_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT) {
		g_return_if_fail(*frame_size >= length * 2);
		length *= 2;
	} else {
		// Not supported due to no test.
		g_return_if_reached();
	}

	// Setup a private structure.
	if (tcode == TCODE_READ_QUADLET_REQUEST ||
	    tcode == TCODE_READ_BLOCK_REQUEST) {
		priv->frame = *frame;
		priv->length = length;
		req.data = (guint64)NULL;
	} else {
		priv->frame = NULL;
		req.data = (guint64)(*frame);
	}

	// Get unit properties.
	g_object_ref(node);
	g_object_get(G_OBJECT(node), "generation", &generation, NULL);

	// Setup a transaction structure.
	req.tcode = tcode;
	req.length = length;
	req.offset = addr;
	req.closure = (guint64)self;
	req.generation = generation;

	// Timeout is set in advance as a parameter of this object.
	expiration = g_get_monotonic_time() +
					priv->timeout * G_TIME_SPAN_MILLISECOND;

	// This predicates against suprious wakeup.
	priv->rcode = G_MAXUINT;
	g_cond_init(&priv->cond);
	g_mutex_lock(&priv->mutex);

	// Send this transaction.
	hinawa_fw_node_ioctl(node, FW_CDEV_IOC_SEND_REQUEST, &req, exception);
	if (*exception != NULL) {
		g_mutex_unlock(&priv->mutex);
		g_cond_clear(&priv->cond);
		goto end;
	}

	while (priv->rcode == G_MAXUINT) {
		// Wait for a response with timeout, waken by a
		// response handler.
		if (!g_cond_wait_until(&priv->cond, &priv->mutex, expiration))
			break;
	}

	g_cond_clear(&priv->cond);
	g_mutex_unlock(&priv->mutex);

	if (priv->rcode == G_MAXUINT) {
		hinawa_fw_node_invalidate_transaction(node, self);
		generate_local_error(exception, HINAWA_FW_RCODE_CANCELLED);
		goto end;
	}

	if (priv->rcode != RCODE_COMPLETE) {
		switch (priv->rcode) {
		case RCODE_COMPLETE:
		case RCODE_CONFLICT_ERROR:
		case RCODE_DATA_ERROR:
		case RCODE_TYPE_ERROR:
		case RCODE_ADDRESS_ERROR:
		case RCODE_SEND_ERROR:
		case RCODE_CANCELLED:
		case RCODE_BUSY:
		case RCODE_GENERATION:
		case RCODE_NO_ACK:
			break;
		default:
			priv->rcode = HINAWA_FW_RCODE_INVALID;
			break;
		}

		generate_local_error(exception, priv->rcode);
		goto end;
	}

	if (tcode == HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST &&
	    tcode == HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST)
		*frame_size = length;
end:
	g_object_unref(node);
}

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_req_handle_response(HinawaFwReq *self,
				   struct fw_cdev_event_response *event)
{
	HinawaFwReqPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));
	priv = hinawa_fw_req_get_instance_private(self);

	g_signal_emit(self, fw_req_sigs[FW_REQ_SIG_TYPE_RESPONDED], 0,
		      event->rcode, event->data, event->length);

	g_mutex_lock(&priv->mutex);

	priv->rcode = event->rcode;

	/* Copy transaction frame if needed. */
	if (priv->frame && priv->length > 0) {
		gsize length = MIN(priv->length, event->length);
		memcpy(priv->frame, event->data, length);
	}

	/* Waken a thread of an user application. */
	g_cond_signal(&priv->cond);

	g_mutex_unlock(&priv->mutex);
}
