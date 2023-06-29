// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>

/**
 * HinawaFwFcp:
 * A FCP transaction executor to node in IEEE 1394 bus.
 *
 * A HinawaFwFcp supports Function Control Protocol (FCP) in IEC 61883-1, in which no way is defined
 * to match response against command by the contents of frames. In 'AV/C Digital Interface Command
 * Set General Specification Version 4.2' (Sep 1 2004, 1394TA), a pair of command and response is
 * loosely matched by the contents of frames.
 *
 * Any of transaction frames should be aligned to 8bit (byte). This class is an application of
 * [class@FwReq] / [class@FwResp].
 */

/**
 * hinawa_fw_fcp_error_quark:
 *
 * Return the [alias@GLib.Quark] for [struct@GLib.Error] which has code in Hinawa.FwFcpError.
 *
 * Since: 2.1
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinawa-fw-fcp-error-quark, hinawa_fw_fcp_error)

static const char *const local_err_msgs[] = {
	[HINAWA_FW_FCP_ERROR_TIMEOUT]		= "The transaction is canceled due to response timeout",
	[HINAWA_FW_FCP_ERROR_LARGE_RESP]	= "The size of response is larger than expected",
};

#define generate_local_error(error, code)						\
	g_set_error_literal(error, HINAWA_FW_FCP_ERROR, code, local_err_msgs[code])

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

typedef struct {
	HinawaFwNode *node;

	guint timeout;
} HinawaFwFcpPrivate;
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

enum fw_fcp_sig_type {
	FW_FCP_SIG_TYPE_RESPONDED = 1,
	FW_FCP_SIG_TYPE_COUNT,
};
static guint fw_fcp_sigs[FW_FCP_SIG_TYPE_COUNT] = { 0 };

// Define later.
static HinawaFwRcode handle_requested2_signal(HinawaFwResp *resp, HinawaFwTcode tcode, guint64 offset,
					      guint32 src, guint32 dst, guint32 card, guint32 generation,
					      const guint8 *frame, guint length);

static void hinawa_fw_fcp_class_init(HinawaFwFcpClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	HINAWA_FW_RESP_CLASS(klass)->requested2 = handle_requested2_signal;

	gobject_class->get_property = fw_fcp_get_property;
	gobject_class->set_property = fw_fcp_set_property;
	gobject_class->finalize = fw_fcp_finalize;

	/**
	 * HinawaFwFcp:timeout:
	 *
	 * Since 1.4
	 * Deprecated: 2.1: Use timeout_ms parameter of [method@FwFcp.avc_transaction].
	 */
	fw_fcp_props[FW_FCP_PROP_TYPE_TIMEOUT] =
		g_param_spec_uint("timeout", "timeout",
				  "An elapse to expire waiting for response "
				  "by msec unit.",
				  10, G_MAXUINT,
				  200,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_DEPRECATED);

	/**
	 * HinawaFwFcp:is-bound:
	 *
	 * Whether this protocol is bound to any instance of HinawaFwNode.
	 *
	 * Since: 2.0
	 */
	fw_fcp_props[FW_FCP_PROP_TYPE_IS_BOUND] =
		g_param_spec_boolean("is-bound", "is-bound",
				     "Whether this protocol is bound to any "
				     "instance of HinawaFwNode.",
				     FALSE,
				     G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_FCP_PROP_TYPE_COUNT,
					  fw_fcp_props);

	/**
	 * HinawaFwFcp::responded:
	 * @self: A [class@FwFcp].
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for byte
	 *	   data of response for FCP.
	 * @frame_size: The number of elements of the array.
	 *
	 * Emitted when the node transfers asynchronous packet as response for FCP and the process
	 * successfully read the content of packet.
	 *
	 * Since: 2.1
	 */
	fw_fcp_sigs[FW_FCP_SIG_TYPE_RESPONDED] =
		g_signal_new("responded",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwFcpClass, responded),
			     NULL, NULL,
			     hinawa_sigs_marshal_VOID__POINTER_UINT,
			     G_TYPE_NONE,
			     2, G_TYPE_POINTER, G_TYPE_UINT);
}

static void hinawa_fw_fcp_init(HinawaFwFcp *self)
{
	return;
}

/**
 * hinawa_fw_fcp_new:
 *
 * Instantiate [class@FwFcp] object and return the instance.
 *
 * Returns: an instance of [class@FwFcp].
 * Since: 1.3.
 */
HinawaFwFcp *hinawa_fw_fcp_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_FCP, NULL);
}

/**
 * hinawa_fw_fcp_command:
 * @self: A [class@FwFcp].
 * @cmd: (array length=cmd_size): An array with elements for request byte data. The value of this
 *	 argument should point to the array and immutable.
 * @cmd_size: The size of array for request in byte unit.
 * @timeout_ms: The timeout to wait for response subaction of transaction for command frame.
 * @error: A [struct@GLib.Error]. Error can be generated with four domains; Hinoko.FwNodeError and
 *	   Hinoko.FwReqError.
 *
 * Transfer command frame for FCP. When receiving response frame for FCP, [signal@FwFcp::responded]
 * signal is emitted.
 *
 * Since: 2.1.
 */
void hinawa_fw_fcp_command(HinawaFwFcp *self, const guint8 *cmd, gsize cmd_size,
			   guint timeout_ms, GError **error)
{
	HinawaFwFcpPrivate *priv;
	HinawaFwReq *req;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	g_return_if_fail(cmd != NULL);
	g_return_if_fail(cmd_size > 0 && cmd_size < FCP_MAXIMUM_FRAME_BYTES);
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_fw_fcp_get_instance_private(self);

	req = g_object_new(HINAWA_TYPE_FW_REQ, NULL);

	// Finish transaction for command frame.
	hinawa_fw_req_transaction_sync(req, priv->node, HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST,
				       FCP_REQUEST_ADDR, cmd_size, (guint8 *const *)&cmd, &cmd_size,
				       timeout_ms, error);
	g_object_unref(req);
}

struct waiter {
	guint8 *frame;
	guint frame_size;
	GCond cond;
	GMutex mutex;
};

static void handle_responded_signal(HinawaFwFcp *self, const guint8 *frame, guint frame_size,
				    gpointer user_data)
{
	struct waiter *w = (struct waiter *)user_data;

	g_mutex_lock(&w->mutex);

	if (w->frame[1] == frame[1] && w->frame[2] == frame[2]) {
		if (frame_size <= w->frame_size)
			memcpy(w->frame, frame, frame_size);
		w->frame_size = frame_size;

		g_cond_signal(&w->cond);
	}

	g_mutex_unlock(&w->mutex);
}

/**
 * hinawa_fw_fcp_avc_transaction:
 * @self: A [class@FwFcp].
 * @cmd: (array length=cmd_size)(in): An array with elements for request byte data. The value of
 *	 this argument should point to the array and immutable.
 * @cmd_size: The size of array for request in byte unit.
 * @resp: (array length=resp_size)(inout): An array with elements for response byte data. Callers
 *	  should give it for buffer with enough space against the request since this library
 *	  performs no reallocation. Due to the reason, the value of this argument should point to
 *	  the pointer to the array and immutable. The content of array is mutable.
 * @resp_size: The size of array for response in byte unit. The value of this argument should point to
 *	       the numerical number and mutable.
 * @timeout_ms: The timeout to wait for response transaction since command transactions finishes.
 * @error: A [struct@GLib.Error]. Error can be generated with four domains; Hinawa.FwNodeError,
 *	   Hinawa.FwReqError, and Hinawa.FwFcpError.
 *
 * Finish the pair of asynchronous transaction for AV/C command and response transactions. The
 * timeout_ms parameter is used to wait for response transaction since the command transaction is
 * initiated, ignoring [property@FwFcp:timeout] property of instance. The timeout is not expanded in
 * the case that AV/C INTERIM status is arrived, thus the caller should expand the timeout in
 * advance for the case.
 *
 * Since: 2.1.
 */
void hinawa_fw_fcp_avc_transaction(HinawaFwFcp *self, const guint8 *cmd, gsize cmd_size,
				   guint8 *const *resp, gsize *resp_size, guint timeout_ms,
				   GError **error)
{
	gulong handler_id;
	struct waiter w = {0};
	gint64 expiration;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	g_return_if_fail(cmd != NULL);
	g_return_if_fail(cmd_size > 2 && cmd_size < FCP_MAXIMUM_FRAME_BYTES);
	g_return_if_fail(resp != NULL);
	g_return_if_fail(resp_size != NULL && *resp_size > 0);
	g_return_if_fail(error == NULL || *error == NULL);

	w.frame = *resp;
	w.frame_size = *resp_size;
	g_cond_init(&w.cond);
	g_mutex_init(&w.mutex);

	// This predicates against suprious wakeup.
	w.frame[0] = 0xff;
	// The two bytes are used to match response and request.
	w.frame[1] = cmd[1];
	w.frame[2] = cmd[2];
	handler_id = g_signal_connect(self, "responded", (GCallback)handle_responded_signal, &w);
	expiration = g_get_monotonic_time() + timeout_ms * G_TIME_SPAN_MILLISECOND;

	g_mutex_lock(&w.mutex);

	// Finish transaction for command frame.
	hinawa_fw_fcp_command(self, cmd, cmd_size, timeout_ms, error);
	if (*error)
		goto end;
deferred:
	while (w.frame[0] == 0xff) {
		// NOTE: Timeout at bus-reset, illegally.
		if (!g_cond_wait_until(&w.cond, &w.mutex, expiration))
			break;
	}

	if (w.frame[0] == 0xff) {
		generate_local_error(error, HINAWA_FW_FCP_ERROR_TIMEOUT);
	} else if (w.frame[0] == AVC_STATUS_INTERIM) {
		// It's a deffered transaction, wait again.
		w.frame[0] = 0x00;
		w.frame_size = *resp_size;
		// Although the timeout is infinite in 1394 TA specification,
		// use the finite value for safe.
		goto deferred;
	} else if (w.frame_size > *resp_size) {
		generate_local_error(error, HINAWA_FW_FCP_ERROR_LARGE_RESP);
	} else {
		*resp_size = w.frame_size;
	}
end:
	g_signal_handler_disconnect(self, handler_id);

	g_mutex_unlock(&w.mutex);

	g_cond_clear(&w.cond);
	g_mutex_clear(&w.mutex);
}

/**
 * hinawa_fw_fcp_transaction:
 * @self: A [class@FwFcp].
 * @req_frame: (array length=req_frame_size)(in): An array with elements for request byte data. The
 *	       value of this argument should point to the array and immutable.
 * @req_frame_size: The size of array for request in byte unit.
 * @resp_frame: (array length=resp_frame_size)(inout): An array with elements for response byte
 *		data. Callers should give it for buffer with enough space against the request
 *		since this library performs no reallocation. Due to the reason, the value of this
 *		argument should point to the pointer to the array and immutable. The content of
 *		array is mutable.
 * @resp_frame_size: The size of array for response in byte unit. The value of this argument should
 *		     point to the numerical number and mutable.
 * @error: A [struct@GLib.Error]. Error can be generated with four domains; Hinawa.FwNodeError,
 *	   Hinawa.FwReqError, and Hinawa.FwFcpError.
 *
 * Finish the pair of command and response transactions for FCP. The value of
 * [property@FwFcp:timeout] property is used to wait for response transaction since the command
 * transaction is initiated.
 *
 * Since: 1.4.
 * Deprecated: 2.1: Use [method@FwFcp.avc_transaction], instead.
 */
void hinawa_fw_fcp_transaction(HinawaFwFcp *self,
			       const guint8 *req_frame, gsize req_frame_size,
			       guint8 *const *resp_frame, gsize *resp_frame_size,
			       GError **error)
{
	guint timeout_ms;

	g_object_get(G_OBJECT(self), "timeout", &timeout_ms, NULL);

	hinawa_fw_fcp_avc_transaction(self, req_frame, req_frame_size, resp_frame, resp_frame_size,
				      timeout_ms, error);
}

static HinawaFwRcode handle_requested2_signal(HinawaFwResp *resp, HinawaFwTcode tcode, guint64 offset,
					      guint32 src, guint32 dst, guint32 card, guint32 generation,
					      const guint8 *frame, guint length)
{
	HinawaFwFcp *self = HINAWA_FW_FCP(resp);
	HinawaFwFcpPrivate *priv = hinawa_fw_fcp_get_instance_private(self);
	guint32 node_id;

	g_object_get(priv->node, "node-id", &node_id, NULL);
	if (offset == FCP_RESPOND_ADDR && tcode == HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST && src == node_id)
		g_signal_emit(self, fw_fcp_sigs[FW_FCP_SIG_TYPE_RESPONDED], 0, frame, length);

	// MEMO: Linux firewire subsystem already send response subaction to finish the transaction,
	// thus the rcode is just ignored.

	return HINAWA_FW_RCODE_COMPLETE;
}

/**
 * hinawa_fw_fcp_bind:
 * @self: A [class@FwFcp].
 * @node: A [class@FwNode].
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinawa.FwFcpError.
 *
 * Start to listen to FCP responses.
 *
 * Since: 1.4
 */
void hinawa_fw_fcp_bind(HinawaFwFcp *self, HinawaFwNode *node, GError **error)
{
	HinawaFwFcpPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_FCP(self));
	g_return_if_fail(node != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_fw_fcp_get_instance_private(self);

	if (priv->node == NULL) {
		hinawa_fw_resp_reserve(HINAWA_FW_RESP(self), node,
				FCP_RESPOND_ADDR, FCP_MAXIMUM_FRAME_BYTES,
				error);
		if (*error != NULL)
			return;
		priv->node = g_object_ref(node);
	}
}

/**
 * hinawa_fw_fcp_unbind:
 * @self: A [class@FwFcp].
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
