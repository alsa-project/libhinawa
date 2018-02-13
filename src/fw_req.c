/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "internal.h"

/**
 * SECTION:fw_req
 * @Title: HinawaFwReq
 * @Short_description: A transaction executor to a FireWire unit
 *
 * A HinawaFwReq supports three types of transactions in IEEE 1212:
 *  - read
 *  - write
 *  - lock
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
	guint length;

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
				  10, UINT_MAX,
				  10,
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

static void fw_req_transact(HinawaFwReq *self, HinawaFwUnit *unit, int tcode,
			    guint64 addr, guint8 *frame, guint length,
			    GError **exception)
{
	struct fw_cdev_send_request req = {0};
	HinawaFwReqPrivate *priv = hinawa_fw_req_get_instance_private(self);
	guint64 generation;
	guint64 expiration;
	int err = 0;

	g_mutex_lock(&priv->mutex);

	/* Setup a private structure. */
	if (tcode == TCODE_READ_QUADLET_REQUEST ||
	    tcode == TCODE_READ_BLOCK_REQUEST) {
		priv->frame = frame;
		priv->length = length;
		req.data = (guint64)NULL;
	} else {
		priv->frame = NULL;
		req.data = (guint64)frame;
	}

	/* Get unit properties. */
	g_object_get(G_OBJECT(unit), "generation", &generation, NULL);

	/* Setup a transaction structure. */
	req.tcode = tcode;
	req.length = length;
	req.offset = addr;
	req.closure = (guint64)self;
	req.generation = generation;

	/* Timeout is set in advance as a parameter of this object. */
	expiration = g_get_monotonic_time() +
					priv->timeout * G_TIME_SPAN_MILLISECOND;

	/* Send this transaction. */
	hinawa_fw_unit_ioctl(unit, FW_CDEV_IOC_SEND_REQUEST, &req, &err);
	/* Wait for a response with timeout, waken by a response handler. */
	if (err == 0) {
		/* Initialize response code. This is a canary. */
		priv->rcode = G_MAXUINT;

		g_cond_init(&priv->cond);
		if (!g_cond_wait_until(&priv->cond, &priv->mutex, expiration))
			err = ETIMEDOUT;
		g_cond_clear(&priv->cond);
	}

	if (err != 0) {
		raise(exception, err);
		goto end;
	}

	if (priv->rcode != RCODE_COMPLETE) {
		/* The canary cries here. */
		if (priv->rcode > RCODE_NO_ACK) {
			raise(exception, EIO);
		} else {
			g_set_error(exception, hinawa_fw_req_quark(), EIO,
				"%d: %s", __LINE__, rcode_labels[priv->rcode]);
		}
		goto end;
	}
end:
	g_mutex_unlock(&priv->mutex);
}

/**
 * hinawa_fw_req_write:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint8) (array) (in): a 8bit array
 * @exception: A #GError
 *
 * Execute write transactions to the given unit.
 */
void hinawa_fw_req_write(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			 GByteArray *frame, GError **exception)
{
	int tcode;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	if (!frame) {
		raise(exception, EINVAL);
		return;
	}

	if (frame->len == 4 && !(addr & 0x03))
		tcode = TCODE_WRITE_QUADLET_REQUEST;
	else
		tcode = TCODE_WRITE_BLOCK_REQUEST;

	g_object_ref(unit);
	fw_req_transact(self, unit, tcode, addr, frame->data, frame->len,
			exception);
	g_object_unref(unit);
}

/**
 * hinawa_fw_req_read:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint8) (array) (out caller-allocates): a 8bit array
 * @length: the number of bytes to read
 * @exception: A #GError
 *
 * Execute read transaction to the given unit.
 */
void hinawa_fw_req_read(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			GByteArray *frame, guint length, GError **exception)
{
	int tcode;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	if (!frame) {
		raise(exception, EINVAL);
		return;
	}

	g_byte_array_set_size(frame, length);

	if (frame->len == 4 && !(addr & 0x03))
		tcode = TCODE_READ_QUADLET_REQUEST;
	else
		tcode = TCODE_READ_BLOCK_REQUEST;

	g_object_ref(unit);
	fw_req_transact(self, unit, tcode, addr, frame->data, frame->len,
			exception);
	g_object_unref(unit);
}

/**
 * hinawa_fw_req_lock:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint8) (array) (inout): a 8bit array
 * @lock_tcode: One of #HinawaFwTcode enumerator for lock operation.
 * @exception: A #GError
 *
 * Execute lock transaction to the given unit.
 */
void hinawa_fw_req_lock(HinawaFwReq *self, HinawaFwUnit *unit,
			guint64 addr, GByteArray **frame,
			HinawaFwTcode lock_tcode, GError **exception)
{
	static const HinawaFwTcode lock_tcodes[] = {
		HINAWA_FW_TCODE_LOCK_RESPONSE,
		HINAWA_FW_TCODE_LOCK_MASK_SWAP,
		HINAWA_FW_TCODE_LOCK_COMPARE_SWAP,
		HINAWA_FW_TCODE_LOCK_FETCH_ADD,
		HINAWA_FW_TCODE_LOCK_LITTLE_ADD,
		HINAWA_FW_TCODE_LOCK_BOUNDED_ADD,
		HINAWA_FW_TCODE_LOCK_WRAP_ADD,
		HINAWA_FW_TCODE_LOCK_VENDOR_DEPENDENT,
	};
	int i;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	if (!(*frame) || (*frame)->len == 0 || (*frame)->len % 8 > 0 ||
	    (addr & 0x03)) {
		raise(exception, EINVAL);
		return;
	}

	for (i = 0; i < sizeof(lock_tcodes) / sizeof(lock_tcodes[0]); ++i) {
		if (lock_tcodes[i] == lock_tcode)
			break;
	}
	if (i == sizeof(lock_tcodes) / sizeof(lock_tcodes[0])) {
		raise(exception, EINVAL);
		return;
	}

	g_object_ref(unit);
	fw_req_transact(self, unit, lock_tcode, addr, (*frame)->data,
			(*frame)->len, exception);
	g_object_unref(unit);
}

/* NOTE: For HinawaFwUnit, internal. */
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
		guint length = MIN(priv->length, event->length);
		memcpy(priv->frame, event->data, length);
	}

	g_mutex_unlock(&priv->mutex);

	/* Waken a thread of an user application. */
	g_cond_signal(&priv->cond);
}
