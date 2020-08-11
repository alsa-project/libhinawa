/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "internal.h"

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

/* For error handling. */
G_DEFINE_QUARK("HinawaFwReq", hinawa_fw_req)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_req_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

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
 * @exception: A #GError.
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
	priv = hinawa_fw_req_get_instance_private(self);

	if (length == 0 || *frame == NULL || *frame_size == 0) {
		raise(exception, EINVAL);
		return;
	}

	// Should be aligned to quadlet.
	if (tcode == HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_READ_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_LOCK_MASK_SWAP ||
	    tcode == HINAWA_FW_TCODE_LOCK_COMPARE_SWAP ||
	    tcode == HINAWA_FW_TCODE_LOCK_FETCH_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_LITTLE_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_BOUNDED_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_WRAP_ADD ||
	    tcode == HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT) {
		if ((addr & 0x3) || (length & 0x3)) {
			raise(exception, EINVAL);
			return;
		}
	}

	// Should have enough space for read/written data.
	if (tcode == HINAWA_FW_TCODE_READ_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_READ_BLOCK_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST ||
	    tcode == HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST) {
		if (*frame_size < length) {
			raise(exception, ENOBUFS);
			return;
		}
	} else if (tcode == HINAWA_FW_TCODE_LOCK_MASK_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_COMPARE_SWAP ||
		   tcode == HINAWA_FW_TCODE_LOCK_FETCH_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_LITTLE_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_BOUNDED_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_WRAP_ADD ||
		   tcode == HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT) {
		if (*frame_size < length * 2) {
			raise(exception, ENOBUFS);
			return;
		}
		length *= 2;
	} else {
		// Not supported due to no test.
		raise(exception, ENOTSUP);
		return;
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
		raise(exception, ETIMEDOUT);
		goto end;
	}

	if (priv->rcode != RCODE_COMPLETE) {
		if (priv->rcode > RCODE_NO_ACK) {
			raise(exception, EIO);
		} else {
			g_set_error(exception, hinawa_fw_req_quark(), EIO,
				"%d: %s", __LINE__, rcode_labels[priv->rcode]);
		}
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
