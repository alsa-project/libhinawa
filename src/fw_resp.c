#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "internal.h"
#include "hinawa_sigs_marshal.h"

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

/* For error handling. */
G_DEFINE_QUARK("HinawaFwResp", hinawa_fw_resp)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_resp_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

struct _HinawaFwRespPrivate {
	HinawaFwUnit *unit;

	guchar *buf;
	guint width;
	guint64 addr_handle;

	GArray *req_frame;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwResp, hinawa_fw_resp, G_TYPE_OBJECT)

/* This object has one signal. */
enum fw_resp_sig_type {
	FW_RESP_SIG_TYPE_REQ = 0,
	FW_RESP_SIG_TYPE_COUNT,
};
static guint fw_resp_sigs[FW_RESP_SIG_TYPE_COUNT] = { 0 };

static void fw_resp_finalize(GObject *obj)
{
	HinawaFwResp *self = HINAWA_FW_RESP(obj);

	hinawa_fw_resp_unregister(self);

	G_OBJECT_CLASS(hinawa_fw_resp_parent_class)->finalize(obj);
}

static void hinawa_fw_resp_class_init(HinawaFwRespClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = NULL;
	gobject_class->set_property = NULL;
	gobject_class->finalize = fw_resp_finalize;

	/**
	 * HinawaFwResp::requested:
	 * @self: A #HinawaFwResp
	 * @tcode: Transaction code
	 * @req_frame: (element-type guint32) (array) (transfer none):
	 *		The frame in request
	 *
	 * When any units transfer requests to the range of address to which
	 * this object listening. The ::requested signal handler can set
	 * data frame to 'resp_frame' if needed.
	 *
	 * Returns: (element-type guint32) (array) (nullable) (transfer full):
	 *	A data frame for response.
	 */
	fw_resp_sigs[FW_RESP_SIG_TYPE_REQ] =
		g_signal_new("requested",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL, NULL,
			     hinawa_sigs_marshal_BOXED__INT_BOXED,
			     G_TYPE_ARRAY, 2, G_TYPE_INT, G_TYPE_ARRAY);
}

static void hinawa_fw_resp_init(HinawaFwResp *self)
{
	return;
}

/**
 * hinawa_fw_resp_register:
 * @self: A #HinawaFwResp
 * @unit: A #HinawaFwUnit
 * @addr: A start address to listen to in host controller
 * @width: The byte width of address to listen to host controller
 * @exception: A #GError
 *
 * Start to listen to a range of address in host controller
 */
void hinawa_fw_resp_register(HinawaFwResp *self, HinawaFwUnit *unit,
			     guint64 addr, guint width, GError **exception)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_allocate allocate = {0};
	int err;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);

	if (priv->unit != NULL) {
		raise(exception, EINVAL);
		return;
	}
	priv->unit = g_object_ref(unit);

	allocate.offset = addr;
	allocate.closure = (guint64)self;
	allocate.length = width;
	allocate.region_end = addr + width;

	hinawa_fw_unit_ioctl(priv->unit, FW_CDEV_IOC_ALLOCATE, &allocate, &err);
	if (err != 0) {
		raise(exception, err);
		g_object_unref(priv->unit);
		priv->unit = NULL;
		return;
	}

	priv->buf = g_malloc0(allocate.length);
	if (priv->buf == NULL) {
		raise(exception, ENOMEM);
		hinawa_fw_resp_unregister(self);
		return;
	}

	priv->req_frame  = g_array_new(FALSE, TRUE, sizeof(guint32));
	if (priv->req_frame == NULL) {
		raise(exception, ENOMEM);
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
	priv = hinawa_fw_resp_get_instance_private(self);

	if (priv->unit == NULL)
		return;

	deallocate.handle = priv->addr_handle;
	hinawa_fw_unit_ioctl(priv->unit, FW_CDEV_IOC_DEALLOCATE, &deallocate,
			     &err);
	g_object_unref(priv->unit);
	priv->unit = NULL;

	if (priv->req_frame != NULL)
		g_array_free(priv->req_frame, TRUE);
	priv->req_frame = NULL;
}

/* NOTE: For HinawaFwUnit, internal. */
void hinawa_fw_resp_handle_request(HinawaFwResp *self,
				   struct fw_cdev_event_request2 *event)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_send_response resp = {0};
	guint i, quads;
	guint32 *buf;
	GArray *resp_frame;
	int err;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);

	if (event->length > priv->width) {
		resp.rcode = RCODE_CONFLICT_ERROR;
		goto respond;
	}

	/* Store requested frame. */
	quads = (event->length + sizeof(guint32) - 1) / sizeof(guint32);
	g_array_set_size(priv->req_frame, quads);
	memcpy(priv->req_frame->data, event->data, event->length);

	/* For endiannness. */
	buf = (guint32 *)priv->req_frame->data;
	for (i = 0; i < quads; i++)
		buf[i] = GUINT32_FROM_BE(buf[i]);

	/* Emit signal to handlers. */
	g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0,
		      event->tcode, priv->req_frame, &resp_frame);

	resp.rcode = RCODE_COMPLETE;
	if (resp_frame == NULL || resp_frame->len == 0)
		goto respond;

	/* For endianness. */
	buf = (guint32 *)resp_frame->data;
	for (i = 0; i < resp_frame->len; i++)
		buf[i] = GUINT32_TO_BE(buf[i]);

	resp.length = resp_frame->len;
	resp.data = (guint64)resp_frame->data;
respond:
	resp.handle = event->handle;

	hinawa_fw_unit_ioctl(priv->unit, FW_CDEV_IOC_SEND_RESPONSE, &resp,
			     &err);
}
