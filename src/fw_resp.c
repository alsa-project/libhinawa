#include <sys/ioctl.h>
#include "fw_resp.h"
#include "internal.h"
#include "hinawa_sigs_marshal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/**
 * SECTION:fw_resp
 * @Title: HinawaFwResp
 * @Short_description: A transaction responder for a FireWire unit
 *
 * A HinawaFwResp responds requests from any units.
 *
 * Any of transaction frames should be aligned to 32bit (quadlet).
 * This class is an application of Linux FireWire subsystem. All of operations
 * utilize ioctl(2) with subsystem specific request commands.
 */
struct _HinawaFwRespPrivate {
	HinawaFwUnit *unit;

	guchar *buf;
	guint width;
	guint64 addr_handle;

	unsigned int len;
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
	HinawaFwRespPrivate *priv = FW_RESP_GET_PRIVATE(self);

	hinawa_fw_resp_unregister(self);
	g_object_unref(priv->unit);

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
	 * @frame: (element-type guint32) (array): The requested frame content
	 *
	 * When any units transfer requests to the range of address to which
	 * this object listening. The ::requested signal handler should set
	 * a response frame by hinawa_fw_resp_set_frame if needed.
	 *
	 * Return value: %TRUE if a data in requested frame is valid, or %FALSE.
	 */
	fw_resp_sigs[FW_RESP_SIG_TYPE_REQ] =
		g_signal_new("requested",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL, NULL,
			     hinawa_sigs_marshal_BOOL__INT_BOXED,
			     G_TYPE_BOOLEAN, 2, G_TYPE_INT, G_TYPE_ARRAY);
}

static void hinawa_fw_resp_init(HinawaFwResp *self)
{
	self->priv = hinawa_fw_resp_get_instance_private(self);
}

/**
 * hinawa_fw_resp_new:
 * @unit: A #HinawaFwUnit
 * @exception: A #GError
 *
 * Returns: An instance of #HinawaFwResp
 */
HinawaFwResp *hinawa_fw_resp_new(HinawaFwUnit *unit, GError **exception)
{
	HinawaFwResp *self;
	HinawaFwRespPrivate *priv;

	self = g_object_new(HINAWA_TYPE_FW_RESP, NULL);
	if (self == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}
	priv = FW_RESP_GET_PRIVATE(self);

	g_object_ref(unit);
	priv->unit = unit;

	return self;
}

/**
 * hinawa_fw_resp_register:
 * @self: A #HinawaFwResp
 * @addr: A start address to listen to in host controller
 * @width: The byte width of address to listen to host controller
 * @exception: A #GError
 *
 * Start to listen to a range of address in host controller
 */
void hinawa_fw_resp_register(HinawaFwResp *self, guint64 addr, guint width,
			     GError **exception)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_allocate allocate = {0};
	int err;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = FW_RESP_GET_PRIVATE(self);

	allocate.offset = addr;
	allocate.closure = (guint64)self;
	allocate.length = width;
	allocate.region_end = addr + width;

	hinawa_fw_unit_ioctl(priv->unit, FW_CDEV_IOC_ALLOCATE, &allocate, &err);
	if (err != 0) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    err, "%s", strerror(err));
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
}

/**
 * hinawa_fw_resp_unregister:
 * @self: A HinawaFwResp
 *
 * stop to listen to a range of address in host controller
 */
void hinawa_fw_resp_unregister(HinawaFwResp *self)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_deallocate deallocate = {0};
	int err;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = FW_RESP_GET_PRIVATE(self);

	deallocate.handle = priv->addr_handle;
	hinawa_fw_unit_ioctl(priv->unit, FW_CDEV_IOC_DEALLOCATE, &deallocate,
			     &err);
}

/**
 * hinawa_fw_resp_set_frame:
 * @self: A #HinawaFwResp
 * @frame: (element-type guint8) (array) (out caller-allocates): a byte frame
 * @len: the bytes to read
 * @exception: A #GError
 *
 * The caller sets transaction frame as a response against request from a unit.
 * This function is expected to be called in 'requested' signal handler.
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
void hinawa_fw_resp_handle_request(HinawaFwResp *self,
				   struct fw_cdev_event_request2 *event)
{
	HinawaFwRespPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = FW_RESP_GET_PRIVATE(self);

	GArray *frame = NULL;
	guint i, quads;
	guint32 *buf;

	struct fw_cdev_send_response resp = {0};
	gboolean error;
	int err;

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
		      event->tcode, frame, &error);
	if (!error) {
		resp.rcode = RCODE_DATA_ERROR;
		goto respond;
	}

	resp.rcode = RCODE_COMPLETE;
	resp.length = priv->len;
	resp.data = (guint64)event->data;
respond:
	resp.handle = event->handle;

	hinawa_fw_unit_ioctl(priv->unit, FW_CDEV_IOC_SEND_RESPONSE, &resp, &err);

	priv->len = 0;
	if (frame == NULL)
		g_array_free(frame, TRUE);
}
