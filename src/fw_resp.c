#include <sys/ioctl.h>
#include "fw_resp.h"
#include "hinawa_sigs_marshal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

struct _HinawaFwRespPrivate {
	gint fd;
	guchar *buf;
	guint width;
	guint64 addr_handle;

	unsigned int len;
	gpointer private_data;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwResp, hinawa_fw_resp, G_TYPE_OBJECT)
#define FW_RESP_GET_PRIVATE(obj)					\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), 				\
				     HINAWA_TYPE_FW_RESP, HinawaFwRespPrivate))

/* This object has one signal. */
enum fw_resp_sig_type {
	FW_RESP_SIG_TYPE_REQ = 0,
	FW_RESP_SIG_TYPE_COUNT,
};
static guint fw_resp_sigs[FW_RESP_SIG_TYPE_COUNT] = { 0 };

static void fw_resp_dispose(GObject *gobject)
{
	HinawaFwResp *self = HINAWA_FW_RESP(gobject);

	hinawa_fw_resp_unregister(self);
	G_OBJECT_CLASS(hinawa_fw_resp_parent_class)->dispose(gobject);
}

static void fw_resp_finalize(GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_fw_resp_parent_class)->finalize(gobject);
}

static void hinawa_fw_resp_class_init(HinawaFwRespClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = NULL;
	gobject_class->set_property = NULL;
	gobject_class->dispose = fw_resp_dispose;
	gobject_class->finalize = fw_resp_finalize;

	/**
	 * HinawaFwResp::requested:
	 * @self: A #HinawaFwResp
	 * @tcode: Transaction code
	 * @frame: (element-type guint32) (array): The frame content
	 * @private_data: A user data
	 *
	 * Returns: data error
	 */
	fw_resp_sigs[FW_RESP_SIG_TYPE_REQ] =
		g_signal_new("requested",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL, NULL,
			     hinawa_sigs_marshal_BOOL__INT_BOXED_POINTER,
			     G_TYPE_BOOLEAN, 3, G_TYPE_INT, G_TYPE_ARRAY,
			     G_TYPE_POINTER);
}

static void hinawa_fw_resp_init(HinawaFwResp *self)
{
	self->priv = hinawa_fw_resp_get_instance_private(self);
}

HinawaFwResp *hinawa_fw_resp_new(HinawaFwUnit *unit, GError **exception)
{
	HinawaFwResp *self;

	self = g_object_new(HINAWA_TYPE_FW_RESP, NULL);
	if (self == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}

	g_object_get(G_OBJECT(unit), "fd", &self->priv->fd, NULL);

	return self;
}

void hinawa_fw_resp_register(HinawaFwResp *self, guint64 addr, guint width,
			     gpointer private_data, GError **exception)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_allocate allocate = {0};

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = FW_RESP_GET_PRIVATE(self);

	allocate.offset = addr;
	allocate.closure = (guint64)self;
	allocate.length = width;
	allocate.region_end = addr + width;

	if (ioctl(self->priv->fd, FW_CDEV_IOC_ALLOCATE, &allocate) < 0) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    errno, "%s", strerror(errno));
		return;
	}

	priv->buf = g_malloc0(allocate.length);
	if (priv->buf == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		hinawa_fw_resp_unregister(self);
		return;
	}

	priv->width = allocate.length;
	priv->private_data = private_data;
}

void hinawa_fw_resp_unregister(HinawaFwResp *self)
{
	struct fw_cdev_deallocate deallocate = {0};

	g_return_if_fail(HINAWA_IS_FW_RESP(self));

	deallocate.handle = self->priv->addr_handle;
	ioctl(self->priv->fd, FW_CDEV_IOC_DEALLOCATE, &deallocate);
}

/**
 * hinawa_fw_resp_set_frame:
 * @self: A #HinawaFwResp
 * @frame: (element-type guint8) (array) (out caller-allocates): a byte frame
 * @len: the bytes to read
 * @exception: A #GError
 */
void hinawa_fw_resp_set_frame(HinawaFwResp *self, GArray *frame,
			      guint len, GError **exception)
{
	HinawaFwRespPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));

	if (self == NULL || frame == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EINVAL, "%s", strerror(EINVAL));
		return;
	}
	priv = FW_RESP_GET_PRIVATE(self);

	if (len == 0 || len > priv->width) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EINVAL, "%s", strerror(EINVAL));
		return;
	}

	memcpy(priv->buf, frame->data, len);
	priv->len = len;
}

/* NOTE: For HinawaFwUnit, internal. */
void hinawa_fw_resp_handle_request(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_request2 *event = &ev->request2;
	HinawaFwResp *self = (HinawaFwResp *)event->closure;
	HinawaFwRespPrivate *priv = FW_RESP_GET_PRIVATE(self);

	GArray *frame = NULL;
	guint i, quads;
	guint32 *buf;

	struct fw_cdev_send_response resp = {0};
	gboolean error;

	if (event->length > priv->width) {
		resp.rcode = RCODE_CONFLICT_ERROR;
		goto respond;
	}

	/* Allocate 32bit array. */
	quads = event->length / 4;
	frame = g_array_sized_new(FALSE, TRUE, sizeof(guint32), quads);
	if (frame == NULL) {
		resp.rcode = RCODE_TYPE_ERROR;
		goto respond;
	}
	g_array_set_size(frame, quads);
	memcpy(frame->data, event->data, event->length);

	buf = (guint32 *)frame->data;
	for (i = 0; i < quads; i++)
		buf[i] = be32toh(buf[i]);

	g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0,
		      event->tcode, frame, priv->private_data, &error);
	if (!error) {
		resp.rcode = RCODE_DATA_ERROR;
		goto respond;
	}

	resp.rcode = RCODE_COMPLETE;
	resp.length = priv->len;
	resp.data = (guint64)event->data;
respond:
	resp.handle = event->handle;

	ioctl(priv->fd, FW_CDEV_IOC_SEND_RESPONSE, &resp);

	priv->len = 0;
	if (frame == NULL)
		g_array_free(frame, TRUE);
}
