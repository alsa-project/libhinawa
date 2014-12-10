#include <sys/ioctl.h>
#include "fw_req.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

enum fw_req_type {
	FW_REQ_TYPE_WRITE = 0,
	FW_REQ_TYPE_READ,
	FW_REQ_TYPE_COMPARE_SWAP,
};

/* NOTE: This object has no properties and no signals. */
struct _HinawaFwReqPrivate {
	gint fd;
	GArray *frame;

	GCond cond;
};
G_DEFINE_TYPE_WITH_PRIVATE (HinawaFwReq, hinawa_fw_req, G_TYPE_OBJECT)
#define FW_REQ_GET_PRIVATE(obj)						\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), 				\
				     HINAWA_TYPE_FW_REQ, HinawaFwReqPrivate))

static void fw_req_dispose(GObject *gobject)
{
	G_OBJECT_CLASS (hinawa_fw_req_parent_class)->dispose(gobject);
}

static void fw_req_finalize (GObject *gobject)
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

HinawaFwReq *hinawa_fw_req_new(GError **exception)
{
	HinawaFwReq *self;

	self = g_object_new(HINAWA_TYPE_FW_REQ, NULL);
	if (self == NULL)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));

	return self;
}

static void fw_req_transact(HinawaFwReq *self, HinawaFwUnit *unit,
			    enum fw_req_type type, guint64 addr, GArray *frame,
			    gint *err)
{
	struct fw_cdev_send_request req = {0};
	HinawaFwReqPrivate *priv = FW_REQ_GET_PRIVATE(self);
	unsigned int quads;
	int tcode;

	int fd;
	guint64 generation;

	guint64 expiration;
	GMutex lock;

	/* Transaction frame should be aligned to 4 bytes. */
	quads = (frame->len + 3) / 4;
	if (quads * 4 != frame->len)
		frame = g_array_set_size(frame, quads * 4);

	/* Setup a private structure. */
	if (type == FW_REQ_TYPE_READ) {
		priv->frame = frame;
		req.data = (guint64)NULL;
		if (quads == 1)
			tcode = TCODE_READ_QUADLET_REQUEST;
		else
			tcode = TCODE_READ_BLOCK_REQUEST;
	} else if (type == FW_REQ_TYPE_WRITE) {
		priv->frame = NULL;
		req.data = (guint64)frame->data;
		if (quads == 1)
			tcode = TCODE_WRITE_QUADLET_REQUEST;
		else
			tcode = TCODE_WRITE_BLOCK_REQUEST;
	} else if ((type == FW_REQ_TYPE_COMPARE_SWAP) &&
		   ((quads == 2) || (quads == 4))) {
			priv->frame = NULL;
			req.data = (guint64)frame->data;
			tcode = TCODE_LOCK_COMPARE_SWAP;
	} else {
		*err = EINVAL;
		return;
	}

	/* Get unit properties. */
	g_object_get(G_OBJECT(unit), "fd", &fd, NULL);
	g_object_get(G_OBJECT(unit), "generation", &generation, NULL);

	/* Setup a transaction structure. */
	req.tcode = tcode;
	req.length = frame->len;
	req.offset = addr;
	req.closure = (guint64)self;
	req.generation = generation;

	/* NOTE: Timeout is 10 milli-seconds. */
	expiration = g_get_monotonic_time() + 10 * G_TIME_SPAN_MILLISECOND;
	g_cond_init(&priv->cond);
	g_mutex_init(&lock);

	/* Send this transaction. */
	if (ioctl(fd, FW_CDEV_IOC_SEND_REQUEST, &req) < 0) {
		*err = errno;
	/* Wait for a response with timeout, waken by a response handler. */
	} else {
		g_mutex_lock(&lock);
		if (!g_cond_wait_until(&priv->cond, &lock, expiration))
			*err = ETIMEDOUT;
		else
			*err = 0;
		g_mutex_unlock(&lock);
	}

	g_mutex_clear(&lock);
	g_cond_clear(&priv->cond);
}

/**
 * hinawa_fw_req_write:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint8) (array) (in): a byte frame
 * @exception: A #GError
 */
void hinawa_fw_req_write(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			 GArray *frame, GError **exception)
{
	int err;

	if (frame == NULL) {
		err = EINVAL;
	} else {
		g_object_ref(unit);
		fw_req_transact(self, unit,
				FW_REQ_TYPE_WRITE, addr, frame, &err);
		g_object_unref(unit);
	}

	if (err != 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    err, "%s", strerror(err));
}

/**
 * hinawa_fw_req_read:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint8) (array) (out caller-allocates): a byte frame
 * @len: the bytes to read
 * @exception: A #GError
 */
void hinawa_fw_req_read(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			GArray *frame, guint len, GError **exception)
{
	int err;

	if (frame == NULL) {
		err = EINVAL;
	} else {
		g_object_ref(unit);
		g_array_set_size(frame, len);
		fw_req_transact(self, unit,
				FW_REQ_TYPE_READ, addr, frame, &err);
		g_object_unref(unit);
	}

	if (err != 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    err, "%s", strerror(err));
}

/**
 * hinawa_fw_req_compare_swap:
 * @self: A #HinawaFwReq
 * @unit: A #HinawaFwUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint8) (array) (inout): a byte frame
 * @exception: A #GError
 */
void hinawa_fw_req_compare_swap(HinawaFwReq *self, HinawaFwUnit *unit,
				guint64 addr, GArray *frame,  GError **exception)
{
	int err;

	if (frame == NULL) {
		err = EINVAL;
	} else {
		g_object_ref(unit);
		fw_req_transact(self, unit,
				FW_REQ_TYPE_COMPARE_SWAP, addr, frame, &err);
		g_object_unref(unit);
	}

	if (err != 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    err, "%s", strerror(err));
}

/* NOTE: For HinawaFwUnit, internal. */
void hinawa_fw_req_handle_response(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_response *event = &ev->response;
	HinawaFwReq *req = (HinawaFwReq *)event->closure;
	HinawaFwReqPrivate *priv;

	if (!req)
		return;
	priv = FW_REQ_GET_PRIVATE(req);

	/* Copy transaction frame. */
	if (req->priv->frame != NULL)
		g_array_insert_vals(priv->frame, 0, event->data, event->length);

	/* Waken a thread of an user application. */
	g_cond_signal(&req->priv->cond);

	return;
}
