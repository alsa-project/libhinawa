#include <string.h>
#include <errno.h>

#include "fw_fcp.h"
#include "fw_req.h"
#include "fw_resp.h"

/**
 * SECTION:fw_fcp
 * @Title: HinawaFwFcp
 * @Short_description: A FCP transaction executor to a FireWire unit
 *
 * A HinawaFwFcp supports Function Control Protocol (FCP) in IEC 61883-1.
 * Some types of transaction in 'AV/C Digital Interface Command Set General
 * Specification Version 4.2' (Sep 1 2004, 1394TA) requires low layer support,
 * thus this class has a code for them.
 *
 * Any of transaction frames should be aligned to 8bit (byte).
 * This class is an application of #HinawaFwReq / #HinawaFwResp.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaFwFcp", hinawa_fw_fcp)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_fcp_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

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

static void hinawa_fw_fcp_finalize(GObject *obj)
{
	HinawaFwFcp *self = HINAWA_FW_FCP(obj);

	hinawa_fw_fcp_unlisten(self);

	G_OBJECT_CLASS(hinawa_fw_fcp_parent_class)->finalize(obj);
}

static void hinawa_fw_fcp_class_init(HinawaFwFcpClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = hinawa_fw_fcp_finalize;
}

static void hinawa_fw_fcp_init(HinawaFwFcp *self)
{
	return;
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
	HinawaFwFcpPrivate *priv;
	HinawaFwReq *req;
	struct fcp_transaction trans = {0};
	GMutex local_lock;
	gint64 expiration;
	guint32 *buf;
	guint i, quads, bytes;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	priv = hinawa_fw_fcp_get_instance_private(self);

	if (req_frame  == NULL || g_array_get_element_size(req_frame)  != 1 ||
	    resp_frame == NULL || g_array_get_element_size(resp_frame) != 1 ||
	    req_frame->len > FCP_MAXIMUM_FRAME_BYTES) {
		raise(exception, EINVAL);
		return;
	}

	req = g_object_new(HINAWA_TYPE_FW_REQ, NULL);

	/* Copy guint8 array to guint32 array. */
	quads = (req_frame->len + sizeof(guint32) - 1) / sizeof(guint32);
	trans.req_frame = g_array_sized_new(FALSE, TRUE,
					    sizeof(guint32), quads);
	g_array_set_size(trans.req_frame, quads);
	memcpy(trans.req_frame->data, req_frame->data, req_frame->len);
	buf = (guint32 *)trans.req_frame->data;
	for (i = 0; i < trans.req_frame->len; i++)
		buf[i] = GUINT32_TO_BE(buf[i]);

	/* Prepare response buffer. */
	trans.resp_frame = g_array_sized_new(FALSE, TRUE,
					     sizeof(guint32), quads);

	/* Insert this entry. */
	g_mutex_lock(&priv->lock);
	priv->transactions = g_list_prepend(priv->transactions, &trans);
	g_mutex_unlock(&priv->lock);

	/* NOTE: Timeout is 200 milli-seconds. */
	expiration = g_get_monotonic_time() + 200 * G_TIME_SPAN_MILLISECOND;
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
		raise(exception, ETIMEDOUT);
	g_mutex_unlock(&local_lock);

	/* Error happened. */
	if (*exception != NULL)
		goto end;

	/* It's a deffered transaction, wait 200 milli-seconds again. */
	if (trans.resp_frame->data[0] == AVC_STATUS_INTERIM) {
		expiration = g_get_monotonic_time() +
			     200 * G_TIME_SPAN_MILLISECOND;
		goto deferred;
	}

	/* Convert guint32 array to guint8 array. */
	buf = (guint32 *)trans.resp_frame->data;
	for (i = 0; i < trans.resp_frame->len; i++)
		buf[i] = GUINT32_TO_BE(buf[i]);
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

static GArray *handle_response(HinawaFwResp *self, gint tcode,
			       GArray *req_frame, gpointer user_data)
{
	HinawaFwFcp *fcp = (HinawaFwFcp *)user_data;
	HinawaFwFcpPrivate *priv = hinawa_fw_fcp_get_instance_private(fcp);
	struct fcp_transaction *trans;
	GList *entry;

	g_mutex_lock(&priv->lock);

	/* Seek corresponding request. */
	for (entry = priv->transactions; entry != NULL; entry = entry->next) {
		trans = (struct fcp_transaction *)entry->data;

		if ((trans->req_frame->data[1] == req_frame->data[1]) &&
		    (trans->req_frame->data[2] == req_frame->data[2]))
			break;
	}

	/* No requests corresponding to this response. */
	if (entry == NULL)
		goto end;

	g_array_insert_vals(trans->resp_frame, 0,
			    req_frame->data, req_frame->len);
	g_cond_signal(&trans->cond);
end:
	g_mutex_unlock(&priv->lock);

	/* Transfer no data in the response frame. */
	return NULL;
}

/**
 * hinawa_fw_fcp_listen:
 * @self: A #HinawaFwFcp
 * @unit: A #HinawaFwUnit
 * @exception: A #GError
 *
 * Start to listen to FCP responses.
 */
void hinawa_fw_fcp_listen(HinawaFwFcp *self, HinawaFwUnit *unit,
			  GError **exception)
{
	HinawaFwFcpPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	priv = hinawa_fw_fcp_get_instance_private(self);

	priv->resp = g_object_new(HINAWA_TYPE_FW_RESP, NULL);
	priv->unit = g_object_ref(unit);

	hinawa_fw_resp_register(priv->resp, priv->unit,
				FCP_RESPOND_ADDR, FCP_MAXIMUM_FRAME_BYTES,
				exception);
	if (*exception != NULL) {
		g_clear_object(&priv->resp);
		priv->resp = NULL;
		g_object_unref(priv->unit);
		priv->unit = NULL;
		return;
	}

	g_signal_connect(priv->resp, "requested",
			 G_CALLBACK(handle_response), self);

	g_mutex_init(&priv->lock);
	priv->transactions = NULL;
}

/**
 * hinawa_fw_fcp_unlisten:
 * @self: A #HinawaFwFcp
 *
 * Stop to listen to FCP responses.
 */
void hinawa_fw_fcp_unlisten(HinawaFwFcp *self)
{
	HinawaFwFcpPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	priv = hinawa_fw_fcp_get_instance_private(self);

	if (priv->resp == NULL)
		return;

	hinawa_fw_resp_unregister(priv->resp);
	priv->resp = NULL;
	g_object_unref(priv->unit);
	priv->unit = NULL;
}
