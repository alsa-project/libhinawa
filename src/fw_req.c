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
	gpointer *buf;
	guint len;

	GCond cond;
};
G_DEFINE_TYPE_WITH_PRIVATE (HinawaFwReq, hinawa_fw_req, G_TYPE_OBJECT)

static void hinawa_fw_req_dispose(GObject *gobject)
{
	G_OBJECT_CLASS (hinawa_fw_req_parent_class)->dispose(gobject);
}

static void hinawa_fw_req_finalize (GObject *gobject)
{
	HinawaFwReq *self = HINAWA_FW_REQ(gobject);

	G_OBJECT_CLASS(hinawa_fw_req_parent_class)->finalize(gobject);
}

static void hinawa_fw_req_class_init(HinawaFwReqClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = NULL;
	gobject_class->set_property = NULL;
	gobject_class->dispose = hinawa_fw_req_dispose;
	gobject_class->finalize = hinawa_fw_req_finalize;
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
			    enum fw_req_type type, gint64 addr, gpointer *buf,
			    guint len, gint *err)
{
	struct fw_cdev_send_request req = {0};
	unsigned int quads;
	int tcode;

	int fd;
	int generation;

	gint64 expiration;
	GMutex lock;

	/* Transaction frame should be aligned to 4 bytes. */
	quads = len / 4;
	if (quads == 0) {
		*err = EINVAL;
		return;
	}

	/* Setup a private structure. */
	if (type == FW_REQ_TYPE_READ) {
		self->priv->buf = buf;
		self->priv->len = len;
		req.data = (guint64)NULL;
		if (quads == 1)
			tcode = TCODE_READ_QUADLET_REQUEST;
		else
			tcode = TCODE_READ_BLOCK_REQUEST;
	} else if (type == FW_REQ_TYPE_WRITE) {
		self->priv->buf = NULL;
		self->priv->len = 0;
		req.data = (guint64)buf;
		if (quads == 1)
			tcode = TCODE_WRITE_QUADLET_REQUEST;
		else
			tcode = TCODE_WRITE_BLOCK_REQUEST;
	} else if ((type == FW_REQ_TYPE_COMPARE_SWAP) &&
		   ((quads == 2) || (quads == 4))) {
			self->priv->buf = NULL;
			self->priv->len = 0;
			req.data = (guint64)buf;
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
	req.length = len;
	req.offset = addr;
	req.closure = (guint64)self;
	req.generation = generation;

	/* NOTE: Timeout is 200 milli-seconds. */
	expiration = g_get_monotonic_time() + 2 * G_TIME_SPAN_MILLISECOND;
	g_cond_init(&self->priv->cond);
	g_mutex_init(&lock);

	/* Send this transaction. */
	if (ioctl(fd, FW_CDEV_IOC_SEND_REQUEST, &req) < 0) {
		*err = errno;
	/* Wait for a response with timeout, waken by a response handler. */
	} else {
		g_mutex_lock(&lock);
		if (!g_cond_wait_until(&self->priv->cond, &lock, expiration))
			*err = ETIMEDOUT;
		else
			*err = 0;
		g_mutex_unlock(&lock);
	}

	g_mutex_clear(&lock);
	g_cond_clear(&self->priv->cond);
}

void hinawa_fw_req_write(HinawaFwReq *req, HinawaFwUnit *unit, gint64 addr,
			 gpointer *buf, gint len, GError **exception)
{
	int err;

	g_object_ref(unit);

	fw_req_transact(req, unit, FW_REQ_TYPE_WRITE, addr, buf, len, &err);
	if (err != 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    err, "%s", strerror(err));

	g_object_unref(unit);
}

void hinawa_fw_req_read(HinawaFwReq *req, HinawaFwUnit *unit, gint64 addr,
			gpointer *buf, gint len, GError **exception)
{
	int err;

	g_object_ref(unit);

	fw_req_transact(req, unit, FW_REQ_TYPE_READ, addr, buf, len, &err);
	if (err != 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    err, "%s", strerror(err));

	g_object_unref(unit);
}

void hinawa_fw_req_compare_swap(HinawaFwReq *req, HinawaFwUnit *unit,
				gint64 addr, gpointer *buf, gint len,
				GError **exception)
{
	int err;

	g_object_ref(unit);

	fw_req_transact(req, unit, FW_REQ_TYPE_COMPARE_SWAP, addr, buf, len,
			&err);
	if (err != 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    err, "%s", strerror(err));

	g_object_unref(unit);
}

/* NOTE: For HinawaFwUnit, internal. */
void hinawa_fw_req_handle_response(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_response *event = &ev->response;
	HinawaFwReq *req = (HinawaFwReq *)event->closure;

	if (!req)
		return;

	/* Copy transaction frame. */
	if (req->priv->buf != NULL && req->priv->len >= event->length)
		memcpy(req->priv->buf, event->data, event->length);

	/* Waken a thread of an user application. */
	g_cond_signal(&req->priv->cond);

	return;
}
