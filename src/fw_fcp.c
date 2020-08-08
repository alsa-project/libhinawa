/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "fw_fcp.h"

/**
 * SECTION:fw_fcp
 * @Title: HinawaFwFcp
 * @Short_description: A FCP transaction executor to a FireWire unit
 * @include: fw_fcp.h
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
	const guint8 *req_frame;
	gsize req_frame_size;
	guint8 *resp_frame;
	gsize resp_frame_size;
	GCond cond;
	GMutex mutex;
};

struct _HinawaFwFcpPrivate {
	HinawaFwNode *node;

	GList *transactions;
	GMutex transactions_mutex;
	guint timeout;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwFcp, hinawa_fw_fcp, HINAWA_TYPE_FW_RESP)

/* This object has one property. */
enum fw_fcp_prop_type {
	FW_FCP_PROP_TYPE_TIMEOUT = 1,
	FW_FCP_PROP_TYPE_IS_BOUND,
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
	case FW_FCP_PROP_TYPE_IS_BOUND:
		g_value_set_boolean(val, priv->node != NULL);
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

	hinawa_fw_fcp_unbind(self);

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
				  10, G_MAXUINT,
				  200,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	fw_fcp_props[FW_FCP_PROP_TYPE_IS_BOUND] =
		g_param_spec_boolean("is-bound", "is-bound",
				     "Whether this protocol is bound to any "
				     "instance of HinawaFwNode.",
				     FALSE,
				     G_PARAM_READABLE);

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

#define NOTIFY_ENOBUFS	1

/**
 * hinawa_fw_fcp_transaction:
 * @self: A #HinawaFwFcp.
 * @req_frame: (array length=req_frame_size)(in): An array with elements for
 *	       request byte data. The value of this argument should point to
 *	       the array and immutable.
 * @req_frame_size: The size of array for request in byte unit.
 * @resp_frame: (array length=resp_frame_size)(inout): An array with elements
 *		for response byte data. Callers should give it for buffer with
 *		enough space against the request since this library performs no
 *		reallocation. Due to the reason, the value of this argument
 *		should point to the pointer to the array and immutable. The
 *		content of array is mutable.
 * @resp_frame_size: The size of array for response in byte unit. The value of
 *		     this argument should point to the numerical number and
 *		     mutable.
 * @exception: A #GError.
 *
 * Execute FCP transaction.
 * Since: 1.4.
 */
void hinawa_fw_fcp_transaction(HinawaFwFcp *self,
			       const guint8 *req_frame, gsize req_frame_size,
			       guint8 *const *resp_frame, gsize *resp_frame_size,
			       GError **exception)
{
	HinawaFwFcpPrivate *priv;
	HinawaFwReq *req;
	struct fcp_transaction trans = {0};
	guint timeout_ms;
	gint64 expiration;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	priv = hinawa_fw_fcp_get_instance_private(self);

	if (req_frame == NULL || *resp_frame == NULL ||
	    req_frame_size == 0 || *resp_frame_size == 0 ||
	    req_frame_size > FCP_MAXIMUM_FRAME_BYTES) {
		raise(exception, EINVAL);
		return;
	}

	g_object_get(G_OBJECT(self), "timeout", &timeout_ms, NULL);

	// NOTE: This is too rough estimation that 66 % of the given timeout
	// can be consumed by write/response subaction for request transaction
	// of FCP command and the rest by write subaction for request
	// transaction of FCP response. This is just a heuristic because
	// actual transactions are internally handled by abstraction layer of
	// Linux FireWire subsystem.
	req = g_object_new(HINAWA_TYPE_FW_REQ, "timeout", timeout_ms * 2 / 3, NULL);

	// Prepare for an entry of FCP transaction.
	trans.req_frame = req_frame;
	trans.req_frame_size = req_frame_size;
	trans.resp_frame = *resp_frame;
	trans.resp_frame_size = *resp_frame_size;

	// This predicates against suprious wakeup.
	trans.resp_frame[0] = 0x00;
	g_cond_init(&trans.cond);
	g_mutex_init(&trans.mutex);
	g_mutex_lock(&trans.mutex);

	/* Insert this entry. */
	g_mutex_lock(&priv->transactions_mutex);
	priv->transactions = g_list_prepend(priv->transactions, &trans);
	g_mutex_unlock(&priv->transactions_mutex);

	// Send this request frame.
	hinawa_fw_req_transaction(req, priv->node,
			HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST,
			FCP_REQUEST_ADDR, trans.req_frame_size,
			(guint8 *const *)&trans.req_frame, &trans.req_frame_size,
			exception);
	if (*exception)
		goto end;
deferred:
	expiration = g_get_monotonic_time() +
		     timeout_ms * G_TIME_SPAN_MILLISECOND;

	while (trans.resp_frame[0] == 0x00) {
		// NOTE: Timeout at bus-reset, illegally.
		if (!g_cond_wait_until(&trans.cond, &trans.mutex, expiration))
			break;
	}
	if (trans.resp_frame[0] == 0x00) {
		raise(exception, ETIMEDOUT);
	} else if (trans.resp_frame[0] == AVC_STATUS_INTERIM) {
		// It's a deffered transaction, wait again.
		trans.resp_frame[0] = 0x00;
		// Although the timeout is infinite in 1394 TA specification,
		// use the finite value for safe.
		goto deferred;
	}

	if (trans.resp_frame_size > NOTIFY_ENOBUFS)
		*resp_frame_size = trans.resp_frame_size;
	else
		raise(exception, ENOBUFS);
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
	gsize length;
	GList *entry;

	g_mutex_lock(&priv->transactions_mutex);

	req_frame = NULL;
	length = 0;
	hinawa_fw_resp_get_req_frame(resp, &req_frame, &length);

	/* Seek corresponding request. */
	for (entry = priv->transactions; entry != NULL; entry = entry->next) {
		trans = (struct fcp_transaction *)entry->data;

		if (trans->req_frame[1] == req_frame[1] &&
		    trans->req_frame[2] == req_frame[2]) {
			g_mutex_lock(&trans->mutex);

			// To notify ENOBUFS.
			if (trans->resp_frame_size < length)
				length = NOTIFY_ENOBUFS;

			memcpy((void *)trans->resp_frame, req_frame, length);
			trans->resp_frame_size = length;
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
 * hinawa_fw_fcp_bind:
 * @self: A #HinawaFwFcp.
 * @node: A #HinawaFwNode.
 * @exception: A #GError.
 *
 * Start to listen to FCP responses.
 */
void hinawa_fw_fcp_bind(HinawaFwFcp *self, HinawaFwNode *node,
			GError **exception)
{
	HinawaFwFcpPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	priv = hinawa_fw_fcp_get_instance_private(self);

	if (priv->node == NULL) {
		hinawa_fw_resp_reserve(HINAWA_FW_RESP(self), node,
				FCP_RESPOND_ADDR, FCP_MAXIMUM_FRAME_BYTES,
				exception);
		if (*exception != NULL)
			return;
		priv->node = g_object_ref(node);

		g_mutex_init(&priv->transactions_mutex);
		priv->transactions = NULL;
	}
}

/**
 * hinawa_fw_fcp_unbind:
 * @self: A #HinawaFwFcp.
 *
 * Stop to listen to FCP responses.
 *
 * Since: 1.4.
 */
void hinawa_fw_fcp_unbind(HinawaFwFcp *self)
{
	HinawaFwFcpPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	priv = hinawa_fw_fcp_get_instance_private(self);

	if (priv->node != NULL) {
		hinawa_fw_resp_release(HINAWA_FW_RESP(self));

		g_object_unref(priv->node);
		priv->node = NULL;
	}
}
