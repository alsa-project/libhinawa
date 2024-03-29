// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

/**
 * HinawaFwResp:
 * A transaction responder for request subaction initiated by node in IEEE 1394 bus.
 *
 * [class@FwResp] responds to request subaction initiated by node in IEEE 1394 bus.
 */

/**
 * hinawa_fw_resp_error_quark:
 *
 * Return the [alias@GLib.Quark] for error domain of [struct@GLib.Error] which has code in
 * [error@FwRespError].
 *
 * Since: 2.2
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinawa-fw-resp-error-quark, hinawa_fw_resp_error)

static const char *const err_msgs[] = {
	[HINAWA_FW_RESP_ERROR_RESERVED] = "Reservation of address space is already done",
	[HINAWA_FW_RESP_ERROR_ADDR_SPACE_USED] = "The requested range of address is already used exclusively",
};

#define generate_local_error(error, code) \
	g_set_error_literal(error, HINAWA_FW_RESP_ERROR, code, err_msgs[code])

#define generate_syscall_error(error, errno, format, arg)			\
	g_set_error(error, HINAWA_FW_RESP_ERROR, HINAWA_FW_RESP_ERROR_FAILED,	\
		    format " %d(%s)", arg, errno, strerror(errno))

typedef struct {
	HinawaFwNode *node;

	guint64 offset;
	guint width;
	guint64 addr_handle;

	guint8 *req_frame;
	gsize req_length;
	guint8 *resp_frame;
	gsize resp_length;
} HinawaFwRespPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwResp, hinawa_fw_resp, G_TYPE_OBJECT)

// This object has one property.
enum fw_resp_prop_type {
	FW_RESP_PROP_TYPE_IS_RESERVED = 1,
	FW_RESP_PROP_TYPE_OFFSET,
	FW_RESP_PROP_TYPE_WIDTH,
	FW_RESP_PROP_TYPE_COUNT,
};
static GParamSpec *fw_resp_props[FW_RESP_PROP_TYPE_COUNT] = { NULL, };

// This object has one signal.
enum fw_resp_sig_type {
	FW_RESP_SIG_TYPE_REQ = 0,
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
	case FW_RESP_PROP_TYPE_OFFSET:
		g_value_set_uint64(val, priv->offset);
		break;
	case FW_RESP_PROP_TYPE_WIDTH:
		g_value_set_uint(val, priv->width);
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

	/**
	 * HinawaFwResp:is-reserved:
	 *
	 * Whether a range of address is reserved or not.
	 *
	 * Since: 2.0
	 */
	fw_resp_props[FW_RESP_PROP_TYPE_IS_RESERVED] =
		g_param_spec_boolean("is-reserved", "is-reserved",
				     "Whether a range of address is reserved or not.",
				     FALSE,
				     G_PARAM_READABLE);

	/**
	 * HinawaFwResp:offset:
	 *
	 * The start offset of reserved address range.
	 *
	 * Since: 2.3
	 */
	fw_resp_props[FW_RESP_PROP_TYPE_OFFSET] =
		g_param_spec_uint64("offset", "offset",
				    "The start offset of reserved address range.",
				    0, G_MAXUINT64,
				    0,
				    G_PARAM_READABLE);

	/**
	 * HinawaFwResp:width:
	 *
	 * The width of reserved address range.
	 *
	 * Since: 2.3
	 */
	fw_resp_props[FW_RESP_PROP_TYPE_WIDTH] =
		g_param_spec_uint("width", "width",
				  "The width of reserved address range.",
				  0, G_MAXUINT,
				  0,
				  G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_RESP_PROP_TYPE_COUNT,
					  fw_resp_props);

	/**
	 * HinawaFwResp::requested:
	 * @self: A [class@FwResp]
	 * @tcode: One of [enum@FwTcode] enumerations
	 * @offset: The address offset at which the transaction arrives.
	 * @src_node_id: The node ID of source for the transaction.
	 * @dst_node_id: The node ID of destination for the transaction.
	 * @card_id: The index of card specific to the 1394 OHCI hardware at which the request
	 *	     subaction arrived.
	 * @generation: The generation of bus when the transaction is transferred.
	 * @tstamp: The isochronous cycle at which the request arrived.
	 * @frame: (element-type guint8)(array length=length): The array with elements for byte
	 *	   data.
	 * @length: The length of bytes for the frame.
	 *
	 * Emitted when any node transfers request subaction to local nodes within the address
	 * range reserved in Linux system.
	 *
	 * The handler is expected to call [method@FwResp.set_resp_frame] with frame and return
	 * [enum@FwRcode] for response subaction.
	 *
	 * The value of @tstamp is unsigned 16 bit integer including higher 3 bits for three low
	 * order bits of second field and the rest 13 bits for cycle field in the format of IEEE
	 * 1394 CYCLE_TIMER register.
	 *
	 * If the version of kernel ABI for Linux FireWire subsystem is less than 6, the value of
	 * tstamp argument has invalid value (=G_MAXUINT). Furthermore, if the version is less than
	 * 4, the src, dst, card, generation arguments have invalid value (=G_MAXUINT).
	 *
	 * Returns: One of [enum@FwRcode] enumerations corresponding to rcodes defined in IEEE 1394
	 *	    specification.
	 * Since: 4.0
	 */
	fw_resp_sigs[FW_RESP_SIG_TYPE_REQ] =
		g_signal_new("requested",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwRespClass, requested),
			     NULL, NULL,
			     hinawa_sigs_marshal_ENUM__ENUM_UINT64_UINT_UINT_UINT_UINT_UINT_POINTER_UINT,
			     HINAWA_TYPE_FW_RCODE, 9, HINAWA_TYPE_FW_TCODE, G_TYPE_UINT64,
			     G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
			     G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_UINT);
}

static void hinawa_fw_resp_init(HinawaFwResp *self)
{
	return;
}

/**
 * hinawa_fw_resp_new:
 *
 * Instantiate [class@FwResp] object and return the instance.
 *
 * Returns: a new instance of [class@FwResp].
 * Since: 1.3
 */
HinawaFwResp *hinawa_fw_resp_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_RESP, NULL);
}

/**
 * hinawa_fw_resp_reserve_within_region:
 * @self: A [class@FwResp].
 * @node: A [class@FwNode].
 * @region_start:  Start offset of address region in which range of address is looked up.
 * @region_end:  End offset of address region in which range of address is looked up.
 * @width: The width for range of address to be looked up.
 * @error: A [struct@GLib.Error]. Error can be generated with two domain of [error@FwNodeError] and
 *	   [error@FwRespError].
 *
 * Allocate an address range within Linux system for local nodes, each of which expresses 1394
 * OHCI hardware. Once successful, [signal@FwResp::requested] signal will be emitted whenever any
 * request subactions arrive at the 1394 OHCI hardware within the dedicated range.
 *
 * The range is reserved between the values specified by @region_start and @region_end with the size
 * indicated by @width. The starting offset may vary every time.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 4.0
 */
gboolean hinawa_fw_resp_reserve_within_region(HinawaFwResp *self, HinawaFwNode *node,
					      guint64 region_start, guint64 region_end,
					      guint width, GError **error)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_allocate allocate = {0};
	int err;

	g_return_val_if_fail(HINAWA_IS_FW_RESP(self), FALSE);
	g_return_val_if_fail(width > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinawa_fw_resp_get_instance_private(self);
	if (priv->node != NULL) {
		generate_local_error(error, HINAWA_FW_RESP_ERROR_RESERVED);
		return FALSE;
	}

	allocate.offset = region_start;
	allocate.closure = (guint64)self;
	allocate.length = width;
	allocate.region_end = region_end;

	err = hinawa_fw_node_ioctl(node, FW_CDEV_IOC_ALLOCATE, &allocate, error);
	if (*error != NULL)
		return FALSE;
	if (err > 0) {
		if (err == EBUSY)
			generate_local_error(error, HINAWA_FW_RESP_ERROR_ADDR_SPACE_USED);
		else
			generate_syscall_error(error, err, "ioctl(%s)", "FW_CDEV_IOC_ALLOCATE");
		return FALSE;
	}

	priv->node = g_object_ref(node);

	priv->req_frame = g_malloc(allocate.length);

	priv->resp_frame = g_malloc0(allocate.length);

	priv->offset = allocate.offset;
	priv->width = allocate.length;
	priv->addr_handle = allocate.handle;

	return TRUE;
}

/**
 * hinawa_fw_resp_reserve:
 * @self: A [class@FwResp].
 * @node: A [class@FwNode].
 * @addr: A start address to listen to in 1394 OHCI hardware.
 * @width: The byte width of address to listen to 1394 OHCI hardware.
 * @error: A [struct@GLib.Error]. Error can be generated with two domain of [error@FwNodeError] and
 *	   and [error@FwRespError].
 *
 * Allocate an address range within Linux system for local nodes, each of which expresses 1394
 * OHCI hardware. Once successful, [signal@FwResp::requested] signal will be emitted whenever any
 * request subactions arrive at the 1394 OHCI hardware within the dedicated range.
 *
 * The range is precisely reserved at the address specified by @addr with the size indicated by
 * @width. In essence, this function is a variant of [method@FwResp.reserve_within_region] in
 * which the specified address range is reserved as provided.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 4.0
 */
gboolean hinawa_fw_resp_reserve(HinawaFwResp *self, HinawaFwNode *node, guint64 addr, guint width,
				GError **error)
{
	return hinawa_fw_resp_reserve_within_region(self, node, addr, addr + width, width, error);
}

/**
 * hinawa_fw_resp_release:
 * @self: A [class@FwResp].
 *
 * Stop listening to the address range in Linux system for local nodes.
 *
 * Since: 1.4
 */
void hinawa_fw_resp_release(HinawaFwResp *self)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_deallocate deallocate = {0};
	GError *error = NULL;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);

	if (priv->node == NULL)
		return;

	// Ignore ioctl error.
	deallocate.handle = priv->addr_handle;
	hinawa_fw_node_ioctl(priv->node, FW_CDEV_IOC_DEALLOCATE, &deallocate, &error);
	g_clear_error(&error);
	g_object_unref(priv->node);
	priv->node = NULL;

	if (priv->req_frame != NULL)
		g_free(priv->req_frame);
	priv->req_frame = NULL;

	if (priv->resp_frame)
		g_free(priv->resp_frame);
	priv->resp_frame = NULL;
	priv->resp_length = 0;

	priv->offset = 0;
	priv->width = 0;
}

/**
 * hinawa_fw_resp_set_resp_frame:
 * @self: A [class@FwResp]
 * @frame: (element-type guint8)(array length=length): a 8 bit array for response frame.
 * @length: The length of bytes for the frame.
 *
 * Register byte frame for the response subaction of transaction.
 *
 * Since: 2.0
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

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_resp_handle_request(HinawaFwResp *self, const struct fw_cdev_event_request *event)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_send_response resp = {0};
	HinawaFwRcode rcode;
	GError *error = NULL;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);

	memset(priv->resp_frame, 0, priv->width);
	priv->resp_length = 0;

	if (!priv->node || event->length > priv->width) {
		rcode = RCODE_CONFLICT_ERROR;
	} else {
		g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0, event->tcode,
			      event->offset, G_MAXUINT, G_MAXUINT, G_MAXUINT, G_MAXUINT,
			      G_MAXUINT, event->data, event->length, &rcode);
	}

	if (priv->resp_length > 0) {
		resp.length = priv->resp_length;
		resp.data = (guint64)priv->resp_frame;
	}

	// Ignore ioctl error.
	resp.rcode = (__u32)rcode;
	resp.handle = event->handle;
	hinawa_fw_node_ioctl(priv->node, FW_CDEV_IOC_SEND_RESPONSE, &resp, &error);
	g_clear_error(&error);
}

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_resp_handle_request2(HinawaFwResp *self, const struct fw_cdev_event_request2 *event)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_send_response resp = {0};
	HinawaFwRcode rcode;
	GError *error = NULL;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);

	memset(priv->resp_frame, 0, priv->width);
	priv->resp_length = 0;

	if (!priv->node || event->length > priv->width) {
		rcode = RCODE_CONFLICT_ERROR;
	} else {
		rcode = HINAWA_FW_RCODE_ADDRESS_ERROR;

		g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0, event->tcode,
			      event->offset, event->source_node_id, event->destination_node_id,
			      event->card, event->generation, G_MAXUINT, event->data, event->length,
			      &rcode);
	}

	if (priv->resp_length > 0) {
		resp.length = priv->resp_length;
		resp.data = (guint64)priv->resp_frame;
	}

	// Ignore ioctl error.
	resp.rcode = (__u32)rcode;
	resp.handle = event->handle;
	hinawa_fw_node_ioctl(priv->node, FW_CDEV_IOC_SEND_RESPONSE, &resp, &error);
	g_clear_error(&error);
}

// NOTE: For HinawaFwNode, internal.
void hinawa_fw_resp_handle_request3(HinawaFwResp *self, const struct fw_cdev_event_request3 *event)
{
	HinawaFwRespPrivate *priv;
	struct fw_cdev_send_response resp = {0};
	HinawaFwRcode rcode;
	GError *error = NULL;

	g_return_if_fail(HINAWA_IS_FW_RESP(self));
	priv = hinawa_fw_resp_get_instance_private(self);

	memset(priv->resp_frame, 0, priv->width);
	priv->resp_length = 0;

	if (!priv->node || event->length > priv->width) {
		rcode = RCODE_CONFLICT_ERROR;
	} else {
		rcode = HINAWA_FW_RCODE_ADDRESS_ERROR;

		g_signal_emit(self, fw_resp_sigs[FW_RESP_SIG_TYPE_REQ], 0, event->tcode,
			      event->offset, event->source_node_id, event->destination_node_id,
			      event->card, event->generation, event->tstamp, event->data,
			      event->length, &rcode);
	}

	if (priv->resp_length > 0) {
		resp.length = priv->resp_length;
		resp.data = (guint64)priv->resp_frame;
	}

	// Ignore ioctl error.
	resp.rcode = (__u32)rcode;
	resp.handle = event->handle;
	hinawa_fw_node_ioctl(priv->node, FW_CDEV_IOC_SEND_RESPONSE, &resp, &error);
	g_clear_error(&error);
}
