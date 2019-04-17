/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "fw_fcp.h"
#include "hinawa_enum_types.h"

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
	GByteArray *req_frame;	/* Request frame */
	GByteArray *resp_frame;	/* Response frame */
	GCond cond;
	GMutex mutex;
};

struct _HinawaFwFcpPrivate {
	HinawaFwUnit *unit;

	GList *transactions;
	GMutex transactions_mutex;
	guint timeout;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwFcp, hinawa_fw_fcp, HINAWA_TYPE_FW_RESP)

/* This object has one property. */
enum fw_fcp_prop_type {
	FW_FCP_PROP_TYPE_TIMEOUT = 1,
	FW_FCP_PROP_TYPE_COUNT,
};
static GParamSpec *fw_fcp_props[FW_FCP_PROP_TYPE_COUNT] = { NULL, };

static void fw_fcp_get_property(GObject *obj, guint id, GValue *val,
				GParamSpec *spec)
{
	HinawaFwFcp *self = HINAWA_FW_FCP(obj);
	HinawaFwFcpPrivate *priv = hinawa_fw_fcp_get_instance_private(self);

	switch (id) {
	case FW_FCP_PROP_TYPE_TIMEOUT:
		g_value_set_uint(val, priv->timeout);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_fcp_set_property(GObject *obj, guint id, const GValue *val,
				GParamSpec *spec)
{
	HinawaFwFcp *self = HINAWA_FW_FCP(obj);
	HinawaFwFcpPrivate *priv = hinawa_fw_fcp_get_instance_private(self);

	switch (id) {
	case FW_FCP_PROP_TYPE_TIMEOUT:
		priv->timeout = g_value_get_uint(val);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_fcp_finalize(GObject *obj)
{
	HinawaFwFcp *self = HINAWA_FW_FCP(obj);

	hinawa_fw_fcp_unlisten(self);

	G_OBJECT_CLASS(hinawa_fw_fcp_parent_class)->finalize(obj);
}

// Define later.
static HinawaFwRcode handle_response(HinawaFwResp *resp, HinawaFwTcode tcode);

static void hinawa_fw_fcp_class_init(HinawaFwFcpClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	HINAWA_FW_RESP_CLASS(klass)->requested = handle_response;

	gobject_class->get_property = fw_fcp_get_property;
	gobject_class->set_property = fw_fcp_set_property;
	gobject_class->finalize = fw_fcp_finalize;

	fw_fcp_props[FW_FCP_PROP_TYPE_TIMEOUT] =
		g_param_spec_uint("timeout", "timeout",
				  "An elapse to expire waiting for response "
				  "by msec unit.",
				  10, UINT_MAX,
				  200,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties(gobject_class,
					  FW_FCP_PROP_TYPE_COUNT,
					  fw_fcp_props);
}

static void hinawa_fw_fcp_init(HinawaFwFcp *self)
{
	return;
}

/**
 * hinawa_fw_fcp_new:
 *
 * Instantiate #HinawaFwFcp object and return the instance.
 *
 * Returns: an instance of #HinawaFwFcp.
 * Since: 1.3.
 */
HinawaFwFcp *hinawa_fw_fcp_new(void)
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
 *
 * Execute fCP transaction.
 */
void hinawa_fw_fcp_transact(HinawaFwFcp *self,
			    GByteArray *req_frame, GByteArray *resp_frame,
			    GError **exception)
{
	HinawaFwFcpPrivate *priv;
	HinawaFwReq *req;
	struct fcp_transaction trans = {0};
	GValue timeout_ms = G_VALUE_INIT;
	gint64 expiration;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	priv = hinawa_fw_fcp_get_instance_private(self);

	if (!req_frame || !resp_frame ||
	    req_frame->len > FCP_MAXIMUM_FRAME_BYTES) {
		raise(exception, EINVAL);
		return;
	}

	req = g_object_new(HINAWA_TYPE_FW_REQ, NULL);

	/* Prepare for an entry of FCP transaction. */
	trans.req_frame = req_frame;
	trans.resp_frame = resp_frame;

	g_value_init(&timeout_ms, G_TYPE_UINT);
	g_object_get_property(G_OBJECT(self), "timeout", &timeout_ms);

	// This predicates against suprious wakeup.
	g_byte_array_remove_range(trans.resp_frame, 0, trans.resp_frame->len);
	g_cond_init(&trans.cond);
	g_mutex_init(&trans.mutex);
	g_mutex_lock(&trans.mutex);

	/* Insert this entry. */
	g_mutex_lock(&priv->transactions_mutex);
	priv->transactions = g_list_prepend(priv->transactions, &trans);
	g_mutex_unlock(&priv->transactions_mutex);

	/* Send this request frame. */
	hinawa_fw_req_write(req, priv->unit, FCP_REQUEST_ADDR, trans.req_frame,
			    exception);
	if (*exception)
		goto end;
deferred:
	expiration = g_get_monotonic_time() +
		     g_value_get_uint(&timeout_ms) * G_TIME_SPAN_MILLISECOND;

	while (trans.resp_frame->len == 0) {
		// NOTE: Timeout at bus-reset, illegally.
		if (!g_cond_wait_until(&trans.cond, &trans.mutex, expiration))
			break;
	}
	if (trans.resp_frame->len == 0) {
		raise(exception, ETIMEDOUT);
	} else if (trans.resp_frame->data[0] == AVC_STATUS_INTERIM) {
		/* It's a deffered transaction, wait again. */
		g_byte_array_remove_range(trans.resp_frame, 0,
					  trans.resp_frame->len);
		goto deferred;
	}
end:
	g_mutex_unlock(&trans.mutex);
	g_cond_clear(&trans.cond);

	/* Remove this entry. */
	g_mutex_lock(&priv->transactions_mutex);
	priv->transactions =
			g_list_remove(priv->transactions, (gpointer *)&trans);
	g_mutex_unlock(&priv->transactions_mutex);

	g_mutex_clear(&trans.mutex);
	g_clear_object(&req);
}

static HinawaFwRcode handle_response(HinawaFwResp *resp, HinawaFwTcode tcode)
{
	HinawaFwFcp *self = HINAWA_FW_FCP(resp);
	HinawaFwFcpPrivate *priv = hinawa_fw_fcp_get_instance_private(self);
	struct fcp_transaction *trans;
	const guint8 *req_frame;
	guint length;
	GList *entry;

	g_mutex_lock(&priv->transactions_mutex);

	req_frame = NULL;
	length = 0;
	hinawa_fw_resp_get_req_frame(resp, &req_frame, &length);

	/* Seek corresponding request. */
	for (entry = priv->transactions; entry != NULL; entry = entry->next) {
		trans = (struct fcp_transaction *)entry->data;

		if (g_array_index(trans->req_frame, guint8, 1) == req_frame[1] &&
		    g_array_index(trans->req_frame, guint8, 2) == req_frame[2]) {
			g_mutex_lock(&trans->mutex);
			g_byte_array_append(trans->resp_frame,
					    (guint8 *)req_frame, length);
			g_cond_signal(&trans->cond);
			g_mutex_unlock(&trans->mutex);
			break;
		}
	}

	g_mutex_unlock(&priv->transactions_mutex);

	/* MEMO: no need to send any data on response frame. */

	return HINAWA_FW_RCODE_COMPLETE;
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

	priv->unit = g_object_ref(unit);

	hinawa_fw_resp_register(HINAWA_FW_RESP(self), priv->unit,
				FCP_RESPOND_ADDR, FCP_MAXIMUM_FRAME_BYTES,
				exception);
	if (*exception != NULL) {
		g_object_unref(priv->unit);
		priv->unit = NULL;
		return;
	}

	g_mutex_init(&priv->transactions_mutex);
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

	hinawa_fw_resp_unregister(HINAWA_FW_RESP(self));

	if (priv->unit != NULL) {
		g_object_unref(priv->unit);
		priv->unit = NULL;
	}
}
