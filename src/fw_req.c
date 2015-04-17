#include <sys/ioctl.h>
#include "fw_req.h"
#include "internal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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
 * Any of transaction frames should be aligned to 32bit (quadlet).
 * This class is an application of Linux FireWire subsystem. All of operations
 * utilize ioctl(2) with subsystem specific request commands.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaFwReq", hinawa_fw_req)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_req_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

enum fw_req_type {
	FW_REQ_TYPE_WRITE = 0,
	FW_REQ_TYPE_READ,
	FW_REQ_TYPE_COMPARE_SWAP,
};

/* NOTE: This object has no properties and no signals. */
struct _HinawaFwReqPrivate {
	GArray *frame;

	GCond cond;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwReq, hinawa_fw_req, G_TYPE_OBJECT)
#define FW_REQ_GET_PRIVATE(obj)						\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj),				\
				     HINAWA_TYPE_FW_REQ, HinawaFwReqPrivate))

static void fw_req_dispose(GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_fw_req_parent_class)->dispose(gobject);
}

static void fw_req_finalize(GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_fw_req_parent_class)->finalize(gobject);
}

static void hinawa_fw_req_class_init(HinawaFwReqClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = NULL;
	gobject_class->set_property = NULL;
	gobject_class->dispose = fw_req_dispose;
	gobject_class->finalize = fw_req_finalize;
}

static void hinawa_fw_req_init(HinawaFwReq *self)
{
	self->priv = hinawa_fw_req_get_instance_private(self);
}

static void fw_req_transact(HinawaFwReq *self, HinawaFwUnit *unit,
			    enum fw_req_type type, guint64 addr, GArray *frame,
			    gint *err)
{
	struct fw_cdev_send_request req = {0};
	HinawaFwReqPrivate *priv = FW_REQ_GET_PRIVATE(self);
	int tcode;

	guint64 generation;

	guint64 expiration;
	GMutex lock;

	guint32 *buf;
	int i;

	/* From host order to be32. */
	if (frame != NULL) {
		buf = (guint32 *)frame->data;
		for (i = 0; i < frame->len; i++)
			buf[i] = htobe32(buf[i]);
	}

	/* Setup a private structure. */
	if (type == FW_REQ_TYPE_READ) {
		priv->frame = frame;
		req.data = (guint64)NULL;
		if (frame->len == 1)
			tcode = TCODE_READ_QUADLET_REQUEST;
		else
			tcode = TCODE_READ_BLOCK_REQUEST;
	} else if (type == FW_REQ_TYPE_WRITE) {
		priv->frame = NULL;
		req.data = (guint64)frame->data;
		if (frame->len == 1)
			tcode = TCODE_WRITE_QUADLET_REQUEST;
		else
			tcode = TCODE_WRITE_BLOCK_REQUEST;
	} else if ((type == FW_REQ_TYPE_COMPARE_SWAP) &&
		   ((frame->len == 2) || (frame->len == 4))) {
			priv->frame = NULL;
			req.data = (guint64)frame->data;
			tcode = TCODE_LOCK_COMPARE_SWAP;
	} else {
		*err = EINVAL;
		return;
	}

	/* Get unit properties. */
	g_object_get(G_OBJECT(unit), "generation", &generation, NULL);

	/* Setup a transaction structure. */
	req.tcode = tcode;
	req.length = frame->len * sizeof(guint32);
	req.offset = addr;
	req.closure = (guint64)self;
	req.generation = generation;

	/* NOTE: Timeout is 10 milli-seconds. */
	expiration = g_get_monotonic_time() + 10 * G_TIME_SPAN_MILLISECOND;
	g_cond_init(&priv->cond);
	g_mutex_init(&lock);

	/* Send this transaction. */
	hinawa_fw_unit_ioctl(unit, FW_CDEV_IOC_SEND_REQUEST, &req, err);
	/* Wait for a response with timeout, waken by a response handler. */
	if (*err == 0) {
		g_mutex_lock(&lock);
		if (!g_cond_wait_until(&priv->cond, &lock, expiration))
			*err = ETIMEDOUT;
		else
			*err = 0;
		g_mutex_unlock(&lock);
	}

	/* From be32 to host order. */
	if (frame != NULL) {
		buf = (guint32 *)frame->data;
		for (i = 0; i < frame->len; i++)
			buf[i] = be32toh(buf[i]);
	}

	g_mutex_clear(&lock);
	g_cond_clear(&priv->cond);
}

/**
 * hinawa_fw_req_write:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint32) (array) (in): a 32bit array
 * @exception: A #GError
 *
 * Execute write transactions to the given unit.
 */
void hinawa_fw_req_write(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			 GArray *frame, GError **exception)
{
	int err;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	if (frame == NULL || g_array_get_element_size(frame) != 4) {
		err = EINVAL;
	} else {
		g_object_ref(unit);
		fw_req_transact(self, unit,
				FW_REQ_TYPE_WRITE, addr, frame, &err);
		g_object_unref(unit);
	}

	if (err != 0)
		raise(exception, err);
}

/**
 * hinawa_fw_req_read:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint32) (array) (out caller-allocates): a 32bit array
 * @len: the bytes to read
 * @exception: A #GError
 *
 * Execute read transaction to the given unit.
 */
void hinawa_fw_req_read(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			GArray *frame, guint len, GError **exception)
{
	int err;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	if (frame == NULL || g_array_get_element_size(frame) != 4) {
		err = EINVAL;
	} else {
		g_object_ref(unit);
		g_array_set_size(frame, len);
		fw_req_transact(self, unit,
				FW_REQ_TYPE_READ, addr, frame, &err);
		g_object_unref(unit);
	}

	if (err != 0)
		raise(exception, err);
}

/**
 * hinawa_fw_req_lock:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint32) (array) (inout): a 32bit array
 * @exception: A #GError
 *
 * Execute lock transaction to the given unit.
 */
void hinawa_fw_req_lock(HinawaFwReq *self, HinawaFwUnit *unit,
			guint64 addr, GArray *frame,  GError **exception)
{
	int err;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));

	if (frame == NULL || g_array_get_element_size(frame) != 4) {
		err = EINVAL;
	} else {
		g_object_ref(unit);
		fw_req_transact(self, unit,
				FW_REQ_TYPE_COMPARE_SWAP, addr, frame, &err);
		g_object_unref(unit);
	}

	if (err != 0)
		raise(exception, err);
}

/* NOTE: For HinawaFwUnit, internal. */
void hinawa_fw_req_handle_response(HinawaFwReq *self,
				   struct fw_cdev_event_response *event)
{
	HinawaFwReqPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_REQ(self));
	priv = FW_REQ_GET_PRIVATE(self);

	/* Copy transaction frame. */
	if (priv->frame != NULL) {
		priv->frame->len = 0;
		g_array_append_vals(priv->frame, event->data,
			event->length / g_array_get_element_size(priv->frame));
	}

	/* Waken a thread of an user application. */
	g_cond_signal(&priv->cond);
}
