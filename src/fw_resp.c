#include "fw_resp.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

struct _HinawaFwRespPrivate {
	gint fd;
	guint width;
	guint64 addr_handle;
};
G_DEFINE_TYPE_WITH_PRIVATE (HinawaFwResp, hinawa_fw_resp, G_TYPE_OBJECT)

/* This object has one signal. */
enum fw_resp_sig_type {
	FW_RESP_SIG_TYPE_REQ = 0,
	FW_RESP_SIG_TYPE_COUNT,
};
static guint fw_resp_sigs[FW_RESP_SIG_TYPE_COUNT] = { 0 };

static void hinawa_fw_resp_dispose(GObject *gobject)
{
	HinawaFwResp *self = HINAWA_FW_RESP(gobject);

	hinawa_fw_resp_unregister(self);
	G_OBJECT_CLASS (hinawa_fw_resp_parent_class)->dispose(gobject);
}

static void hinawa_fw_resp_finalize (GObject *gobject)
{
	HinawaFwResp *self = HINAWA_FW_RESP(gobject);

	G_OBJECT_CLASS(hinawa_fw_resp_parent_class)->finalize(gobject);
}

static void hinawa_fw_resp_class_init(HinawaFwRespClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = NULL;
	gobject_class->set_property = NULL;
	gobject_class->dispose = hinawa_fw_resp_dispose;
	gobject_class->finalize = hinawa_fw_resp_finalize;

	/* TODO: marshal. */
	fw_resp_sigs[FW_RESP_SIG_TYPE_REQ] =
			g_signal_new("request",
				     G_OBJECT_CLASS_TYPE(klass),
				     G_SIGNAL_RUN_LAST,
				     0,
				     NULL, NULL,
				     g_cclosure_marshal_VOID__VOID,
				     G_TYPE_NONE, 0, NULL);
}

static void hinawa_fw_resp_init(HinawaFwResp *self)
{
	self->priv = hinawa_fw_resp_get_instance_private(self);
}

HinawaFwResp *hinawa_fw_resp_new(HinawaFwUnit *unit, GError **exception)
{
	HinawaFwResp *self;
	gint fd;

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
			     GError **exception)
{
	struct fw_cdev_allocate allocate = {0};

	allocate.offset = addr;
	allocate.closure = (guint64)self;
	allocate.length = width;
	allocate.region_end = addr + width;

	if (ioctl(self->priv->fd, FW_CDEV_IOC_ALLOCATE, &allocate) < 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    errno, "%s", strerror(errno));
	else
		self->priv->width = allocate.length;
}

void hinawa_fw_resp_unregister(HinawaFwResp *self)
{
	struct fw_cdev_deallocate deallocate = {0};

	deallocate.handle = self->priv->addr_handle;
	ioctl(self->priv->fd, FW_CDEV_IOC_DEALLOCATE, &deallocate);
}

/* NOTE: For HinawaFwUnit, internal. */
void hinawa_fw_resp_handle_request(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_request2 *event = &ev->request2;
	HinawaFwResp *self = (HinawaFwResp *)event->closure;

	struct fw_cdev_send_response resp = {0};
	unsigned int length;
	guint32 rcode;
	int err;

	/* decide rcode */
	if (!self) {
		rcode = RCODE_ADDRESS_ERROR;
		goto respond;
	}

	if ((self->priv->width < event->length)) {
		rcode = RCODE_CONFLICT_ERROR;
		goto respond;
	}

	length = event->length;

	/* TODO: parameters. */
	g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0);
	if (err < 0)
		rcode = RCODE_DATA_ERROR;
	else
		rcode = RCODE_COMPLETE;
respond:
	resp.rcode = rcode;
	resp.length = length;
	resp.data = (guint64)event->data;
	resp.handle = event->handle;

	ioctl(self->priv->fd, FW_CDEV_IOC_SEND_RESPONSE, &resp);
}
