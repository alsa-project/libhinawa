/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "internal.h"
#include "hinawa_sigs_marshal.h"
#include "hinawa_enums.h"

/**
 * SECTION:fw_resp
 * @Title: HinawaFwResp
 * @Short_description: A transaction responder for a FireWire node.
 * @include: fw_resp.h
 *
 * A HinawaFwResp responds requests from any units.
 *
 * This class is an application of Linux FireWire subsystem. All of operations
 * utilize ioctl(2) with subsystem specific request commands.
 */

/**
 * hinawa_fw_resp_error_quark:
 *
 * Return the GQuark for error domain of GError which has code in #HinawaFwRespError.
 *
 * Returns: A #GQuark.
 */
G_DEFINE_QUARK(hinawa-fw-resp-error-quark, hinawa_fw_resp_error)

static const char *const err_msgs[] = {
	[HINAWA_FW_RESP_ERROR_RESERVED] = "Reservation of address space is already done",
	[HINAWA_FW_RESP_ERROR_ADDR_SPACE_USED] = "The requested range of address is already used exclusively",
};

#define generate_local_error(exception, code) \
	g_set_error_literal(exception, HINAWA_FW_RESP_ERROR, code, err_msgs[code])

#define generate_syscall_error(exception, errno, format, arg)				\
	g_set_error(exception, HINAWA_FW_RESP_ERROR, HINAWA_FW_RESP_ERROR_FAILED,	\
		    format " %d(%s)", arg, errno, strerror(errno))

struct _HinawaFwRespPrivate {
	HinawaFwNode *node;

	guint width;
	guint64 addr_handle;

	guint8 *req_frame;
	gsize req_length;
	guint8 *resp_frame;
	gsize resp_length;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwResp, hinawa_fw_resp, G_TYPE_OBJECT)

// This object has one property.
enum fw_resp_prop_type {
	FW_RESP_PROP_TYPE_IS_RESERVED = 1,
	FW_RESP_PROP_TYPE_COUNT,
};
static GParamSpec *fw_resp_props[FW_RESP_PROP_TYPE_COUNT] = { NULL, };

// This object has one signal.
enum fw_resp_sig_type {
	FW_RESP_SIG_TYPE_REQ = 0,
	FW_RESP_SIG_TYPE_REQ2,
	FW_RESP_SIG_TYPE_COUNT,
};
static guint fw_resp_sigs[FW_RESP_SIG_TYPE_COUNT] = { 0 };

static void fw_resp_get_property(GObject *obj, guint id, GValue *val,
				 GParamSpec *spec)
{
	HinawaFwResp *self = HINAWA_FW_RESP(obj);
	HinawaFwRespPrivate *priv = hinawa_fw_resp_get_instance_private(self);

	switch (id) {
	case FW_RESP_PROP_TYPE_IS_RESERVED:
		g_value_set_boolean(val, priv->node != NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_resp_finalize(GObject *obj)
{
	HinawaFwResp *self = HINAWA_FW_RESP(obj);

	hinawa_fw_resp_release(self);

	G_OBJECT_CLASS(hinawa_fw_resp_parent_class)->finalize(obj);
}

static void hinawa_fw_resp_class_init(HinawaFwRespClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_resp_get_property;
	gobject_class->finalize = fw_resp_finalize;

	fw_resp_props[FW_RESP_PROP_TYPE_IS_RESERVED] =
		g_param_spec_boolean("is-reserved", "is-reserved",
				     "Whether a range of address is reserved "
				     "or not in host controller. ",
				     FALSE,
				     G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_RESP_PROP_TYPE_COUNT,
					  fw_resp_props);

	/**
	 * HinawaFwResp::requested:
	 * @self: A #HinawaFwResp
	 * @tcode: One of #HinawaTcode enumerators
	 *
	 * When any node transfers requests to the range of address to which this object listening,
	 * the #HinawaFwResp::requested signal handler is called with #HinawaFwTcode, without the
	 * case that #HinawaFwResp::requested2 signal handler is already assigned.
	 *
	 * The handler can get data frame by a call of #hinawa_fw_resp_get_req_frame() and set data
	 * frame by a call of #hinawa_fw_resp_set_resp_frame(), then returns rcode.
	 *
	 * Returns: One of #HinawaRcode enumerators corresponding to rcodes defined in IEEE 1394
	 * specification.
	 *
	 * Deprecated: 2.2: Use #HinawaFwResp::requested2, instead.
	 */
	fw_resp_sigs[FW_RESP_SIG_TYPE_REQ] =
		g_signal_new("requested",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwRespClass, requested),
			     NULL, NULL,
			     hinawa_sigs_marshal_ENUM__ENUM,
			     HINAWA_TYPE_FW_RCODE, 1, HINAWA_TYPE_FW_TCODE);

	/**
	 * HinawaFwResp::requested2:
	 * @self: A #HinawaFwResp
	 * @tcode: One of #HinawaTcode enumerations
	 * @offset: The address offset at which the transaction arrives.
	 * @src: The node ID of source for the transaction.
	 * @dst: The node ID of destination for the transaction.
	 * @card: The index of card corresponding to 1394 OHCI controller.
	 * @generation: The generation of bus when the transaction is transferred.
	 * @frame: (element-type guint8)(array length=length): The array with elements for byte
	 *	   data.
	 * @length: The length of bytes for the frame.
	 *
	 * When any node transfers request subaction to the range of address to which this object
	 * listening, the #HinawaFwResp::requested signal handler is called with arrived frame for
	 * the subaction. The handler is expected to call #hinawa_fw_resp_set_resp_frame() with
	 * frame and return rcode for response subaction.
	 *
	 * Returns: One of #HinawaRcode enumerators corresponding to rcodes defined in IEEE 1394
	 *	    specification.
	 */
	fw_resp_sigs[FW_RESP_SIG_TYPE_REQ2] =
		g_signal_new("requested2",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwRespClass, requested2),
			     NULL, NULL,
			     hinawa_sigs_marshal_ENUM__ENUM_UINT64_UINT_UINT_UINT_UINT_POINTER_UINT,
			     HINAWA_TYPE_FW_RCODE, 8, HINAWA_TYPE_FW_TCODE, G_TYPE_UINT64,
			     G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
			     G_TYPE_POINTER, G_TYPE_UINT);
}

static void hinawa_fw_resp_init(HinawaFwResp *self)
{
	return;
}

/**
 * hinawa_fw_resp_new:
 *
 * Instantiate #HinawaFwResp object and return the instance.
 *
 * Returns: a new instance of #HinawaFwResp.
 * Since: 1.3.
 */
HinawaFwResp *hinawa_fw_resp_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_RESP, NULL);
}

/**
 * hinawa_fw_resp_reserve:
 * @self: A #HinawaFwResp.
 * @node: A #HinawaFwNode.
 * @addr: A start address to listen to in host controller.
 * @width: The byte width of address to listen to host controller.
 * @exception: A #GError. Error can be generated with two domain of #hinawa_fw_node_error_quark()
 *	       and #hinawa_fw_resp_error_quark().
 *
 * Start to listen to a range of address in host controller which connects to
 * the node.
 *
 * Since: 1.4.
 */
void hinawa_fw_resp_reserve(HinawaFwResp *self, HinawaFwNode*node,
			    guint64 addr, guint width, GError **exception)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_allocate allocate = {0};
	int err;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	g_return_if_fail(width > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_fw_resp_get_instance_private(self);
	if (priv->node != NULL) {
		generate_local_error(exception, HINAWA_FW_RESP_ERROR_RESERVED);
		return;
	}

	allocate.offset = addr;
	allocate.closure = (guint64)self;
	allocate.length = width;
	allocate.region_end = addr + width;

	err = hinawa_fw_node_ioctl(node, FW_CDEV_IOC_ALLOCATE, &allocate, exception);
	if (*exception != NULL)
		return;
	if (err > 0) {
		if (err == EBUSY)
			generate_local_error(exception, HINAWA_FW_RESP_ERROR_ADDR_SPACE_USED);
		else
			generate_syscall_error(exception, err, "ioctl(%s)", "FW_CDEV_IOC_ALLOCATE");
		return;
	}

	priv->node = g_object_ref(node);

	priv->req_frame = g_malloc(allocate.length);

	priv->resp_frame = g_malloc0(allocate.length);

	priv->width = allocate.length;
	priv->addr_handle = allocate.handle;
}

/**
 * hinawa_fw_resp_release:
 * @self: A HinawaFwResp.
 *
 * stop to listen to a range of address in host controller.
 *
 * Since: 1.4.
 */
void hinawa_fw_resp_release(HinawaFwResp *self)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_deallocate deallocate = {0};
	GError *exception = NULL;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);

	if (priv->node == NULL)
		return;

	// Ignore ioctl error.
	deallocate.handle = priv->addr_handle;
	hinawa_fw_node_ioctl(priv->node, FW_CDEV_IOC_DEALLOCATE, &deallocate, &exception);
	g_clear_error(&exception);
	g_object_unref(priv->node);
	priv->node = NULL;

	if (priv->req_frame != NULL)
		g_free(priv->req_frame);
	priv->req_frame = NULL;

	if (priv->resp_frame)
		g_free(priv->resp_frame);
	priv->resp_frame = NULL;
	priv->resp_length = 0;
}

/**
 * hinawa_fw_resp_get_req_frame:
 * @self: A #HinawaFwResp
 * @frame: (array length=length)(out)(transfer none): a 8bit array for response
 * 	   frame.
 * @length: (out): The length of bytes for the frame.
 *
 * Retrieve byte frame to be requested.
 *
 * Deprecated: 2.2: handler for #HinawaFwResp::requested2 signal can receive the frame in its
 *		    argument.
 */
void hinawa_fw_resp_get_req_frame(HinawaFwResp *self, const guint8 **frame,
				  gsize *length)
{
	HinawaFwRespPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	g_return_if_fail(frame != NULL);
	g_return_if_fail(length != NULL);

	priv = hinawa_fw_resp_get_instance_private(self);

	if (frame && length && priv->req_length > 0) {
		*frame = (const guint8 *)priv->req_frame;
		*length = priv->req_length;
	}
}

/**
 * hinawa_fw_resp_set_resp_frame:
 * @self: A #HinawaFwResp
 * @frame: (element-type guint8)(array length=length): a 8bit array for response
 * 	   frame.
 * @length: The length of bytes for the frame.
 *
 * Register byte frame as response.
 */
void hinawa_fw_resp_set_resp_frame(HinawaFwResp *self, guint8 *frame,
				   gsize length)
{
	HinawaFwRespPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	g_return_if_fail(frame != NULL);
	g_return_if_fail(length > 0);

	priv = hinawa_fw_resp_get_instance_private(self);

	if (frame && length <= priv->width) {
		memcpy(priv->resp_frame, frame, length);
		priv->resp_length = length;
	}
}

// NOTE: For HinawaFwNodee, internal.
void hinawa_fw_resp_handle_request(HinawaFwResp *self,
				   struct fw_cdev_event_request2 *event)
{
	HinawaFwRespPrivate *priv;
	HinawaFwRespClass *klass;
	struct fw_cdev_send_response resp = {0};
	HinawaFwRcode rcode;
	GError *exception = NULL;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);
	klass = HINAWA_FW_RESP_GET_CLASS(self);

	memset(priv->resp_frame, 0, priv->width);
	priv->resp_length = 0;

	if (!priv->node || event->length > priv->width) {
		rcode = RCODE_CONFLICT_ERROR;
	} else if (klass->requested2 != NULL ||
	    g_signal_has_handler_pending(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ2], 0, TRUE)) {
		g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ2], 0, event->tcode,
			      event->offset, event->source_node_id, event->destination_node_id,
			      event->card, event->generation, event->data, event->length, &rcode);
	} else if (klass->requested != NULL ||
		   g_signal_has_handler_pending(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0, TRUE)) {
		// For backward compatibility.
		memcpy(priv->req_frame, event->data, event->length);
		priv->req_length = event->length;

		g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0, event->tcode, &rcode);
	} else {
		rcode = HINAWA_FW_RCODE_ADDRESS_ERROR;
	}

	if (priv->resp_length > 0) {
		resp.length = priv->resp_length;
		resp.data = (guint64)priv->resp_frame;
	}

	// Ignore ioctl error.
	resp.rcode = (__u32)rcode;
	resp.handle = event->handle;
	hinawa_fw_node_ioctl(priv->node, FW_CDEV_IOC_SEND_RESPONSE, &resp, &exception);
	g_clear_error(&exception);
}
