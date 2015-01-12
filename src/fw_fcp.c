#include "fw_fcp.h"
#include "fw_resp.h"
#include "fw_req.h"

#define FCP_MAXIMUM_FRAME_BYTES	0x200U
#define FCP_REQUEST_ADDR	0xfffff0000b00
#define FCP_RESPOND_ADDR	0xfffff0000d00

/* For your information. */
enum avc_type {
	AVC_TYPE_CONTROL		= 0x00,
	AVC_TYPE_STATUS			= 0x01,
	AVC_TYPE_SPECIFIC_INQUIRY	= 0x02,
	AVC_TYPE_NOTIFY			= 0x03,
	AVC_TYPE_GENERAL_INQUIRY	= 0x04,
	/* 0x05-0x07 are reserved. */
};
/* continue */
enum avc_status {
	AVC_STATUS_NOT_IMPLEMENTED	= 0x08,
	AVC_STATUS_ACCEPTED		= 0x09,
	AVC_STATUS_REJECTED		= 0x0a,
	AVC_STATUS_IN_TRANSITION	= 0x0b,
	AVC_STATUS_IMPLEMENTED_STABLE	= 0x0c,
	AVC_STATUS_CHANGED		= 0x0d,
	/* reserved */
	AVC_STATUS_INTERIM		= 0x0f,
};

struct fcp_transaction {
	GArray *req_frame;	/* Request frame */
	GArray *resp_frame;	/* Response frame */
	GCond cond;
};

struct _HinawaFwFcpPrivate {
	HinawaFwUnit *unit;
	HinawaFwResp *resp;

	GList *transactions;
	GMutex lock;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwFcp, hinawa_fw_fcp, G_TYPE_OBJECT)
#define FW_FCP_GET_PRIVATE(obj)						\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), 				\
				     HINAWA_TYPE_FW_FCP, HinawaFwFcpPrivate))

static void hinawa_fw_fcp_dispose(GObject *obj)
{
	HinawaFwFcp *self = HINAWA_FW_FCP(obj);
	HinawaFwFcpPrivate *priv = FW_FCP_GET_PRIVATE(self);

	if (priv->resp != NULL)
		hinawa_fw_fcp_unlisten(self);

	G_OBJECT_CLASS(hinawa_fw_fcp_parent_class)->dispose(obj);
}

static void hinawa_fw_fcp_finalize (GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_fw_fcp_parent_class)->finalize(gobject);
}

static void hinawa_fw_fcp_class_init(HinawaFwFcpClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->dispose = hinawa_fw_fcp_dispose;
	gobject_class->finalize = hinawa_fw_fcp_finalize;
}

static void hinawa_fw_fcp_init(HinawaFwFcp *self)
{
	self->priv = hinawa_fw_fcp_get_instance_private(self);
}

HinawaFwFcp *hinawa_fw_fcp_new(GError **exception)
{
	return g_object_new(HINAWA_TYPE_FW_FCP, NULL);
}

/**
 * hinawa_fw_fcp_transact:
 * @self: A #HinawaFwFcp
 * @req_frame:  (element-type guint8) (array) (in): a byte frame for request
 * @resp_frame: (element-type guint8) (array) (out caller-allocates): a byte
 *		frame for response
 * @exception: A #GError
 */
void hinawa_fw_fcp_transact(HinawaFwFcp *self,
			    GArray *req_frame, GArray *resp_frame,
			    GError **exception)
{
	HinawaFwFcpPrivate *priv = FW_FCP_GET_PRIVATE(self);
	HinawaFwReq *req;
	struct fcp_transaction trans = {0};
	GMutex local_lock;
	gint64 expiration;
	guint quads, bytes;

	if (req_frame == NULL  || g_array_get_element_size(req_frame)  != 1 ||
	    resp_frame == NULL || g_array_get_element_size(resp_frame) != 1 ||
	    req_frame->len > FCP_MAXIMUM_FRAME_BYTES) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EINVAL, "%s", strerror(EINVAL));
		return;
	}

	req = hinawa_fw_req_new(exception);
	if (*exception != NULL)
		return;

	/* Copy guint8 array to guint32 array. */
	quads = (req_frame->len + sizeof(guint32) - 1) / sizeof(guint32);
	trans.req_frame = g_array_sized_new(FALSE, TRUE,
					    sizeof(guint32), quads);
	g_array_set_size(trans.req_frame, quads);
	memcpy(trans.req_frame->data, req_frame->data, req_frame->len);

	/* Prepare response buffer. */
	trans.resp_frame = g_array_sized_new(FALSE, TRUE,
					    sizeof(guint32), quads);

	/* Insert this entry. */
	g_mutex_lock(&priv->lock);
	priv->transactions = g_list_prepend(priv->transactions, &trans);
	g_mutex_unlock(&priv->lock);

	/* NOTE: Timeout is 200 milli-seconds. */
	expiration = g_get_monotonic_time() + 2000 * G_TIME_SPAN_MILLISECOND;
	g_cond_init(&trans.cond);
	g_mutex_init(&local_lock);

	/* Send this request frame. */
	hinawa_fw_req_write(req, priv->unit, FCP_REQUEST_ADDR, trans.req_frame,
			    exception);
	if (*exception != NULL)
		goto end;
deferred:
	/*
	 * Wait corresponding response till timeout.
	 * NOTE: Timeout at bus-reset, illegally.
	 */
	g_mutex_lock(&local_lock);
	if (!g_cond_wait_until(&trans.cond, &local_lock, expiration))
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ETIMEDOUT, "%s", strerror(ETIMEDOUT));
	g_mutex_unlock(&local_lock);

	/* Error happened. */
	if (*exception != NULL)
		goto end;

	/* It's a deffered transaction, wait 200 milli-seconds again. */
	if (trans.resp_frame->data[0] >> 24 == AVC_STATUS_INTERIM) {
		expiration = g_get_monotonic_time() +
			     2 * G_TIME_SPAN_MILLISECOND;
		goto deferred;
	}

	/* Convert guint32 array to guint8 array. */
	bytes = trans.resp_frame->len * sizeof(guint32);
	g_array_set_size(resp_frame, bytes);
	memcpy(resp_frame->data, trans.resp_frame->data, bytes);
end:
	/* Remove this entry. */
	g_mutex_lock(&priv->lock);
	priv->transactions =
			g_list_remove(priv->transactions, (gpointer *)&trans);
	g_mutex_unlock(&priv->lock);

	g_array_free(trans.req_frame, TRUE);
	g_mutex_clear(&local_lock);
	g_clear_object(&req);
}

static gboolean handle_response(HinawaFwResp *self, gint tcode,
				GArray *resp_frame, gpointer private_data,
				gpointer something)
{
	HinawaFwFcp *fcp = (HinawaFwFcp *)private_data;
	HinawaFwFcpPrivate *priv = FW_FCP_GET_PRIVATE(fcp);
	struct fcp_transaction *trans;
	gboolean error;
	GList *entry;

	g_mutex_lock(&priv->lock);

	/* Seek correcponding request. */
	for (entry = priv->transactions; entry != NULL; entry = entry->next) {
		trans = (struct fcp_transaction *)entry->data;

		if ((trans->req_frame->data[1] == resp_frame->data[1]) &&
		    (trans->req_frame->data[2] == resp_frame->data[2]))
			break;

	}

	/* No requests corresponding to this response. */
	if (entry == NULL) {
		error = TRUE;
	} else {
		g_array_insert_vals(trans->resp_frame, 0,
				    resp_frame->data, resp_frame->len);
		g_cond_signal(&trans->cond);
		error = FALSE;
	}

	g_mutex_unlock(&priv->lock);
	return error;
}

void hinawa_fw_fcp_listen(HinawaFwFcp *self, HinawaFwUnit *unit,
			  GError **exception)
{
	HinawaFwResp *resp;
	HinawaFwFcpPrivate *priv = FW_FCP_GET_PRIVATE(self);

	resp = hinawa_fw_resp_new(unit, exception);
	if (*exception != NULL)
		return;

	hinawa_fw_resp_register(resp, FCP_RESPOND_ADDR, FCP_MAXIMUM_FRAME_BYTES,
				(void *)self, exception);
	if (*exception != NULL)
		return;

	g_signal_connect(resp, "requested", G_CALLBACK(handle_response), resp);

	g_mutex_init(&priv->lock);
	priv->transactions = NULL;

	priv->resp = resp;
	priv->unit = unit;
}

void hinawa_fw_fcp_unlisten(HinawaFwFcp *self)
{
	HinawaFwFcpPrivate *priv = FW_FCP_GET_PRIVATE(self);

	hinawa_fw_resp_unregister(priv->resp);
	priv->resp = NULL;
}
