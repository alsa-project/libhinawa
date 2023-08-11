// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

/**
 * HinawaFwNode:
 * An event listener for FireWire node
 *
 * A [class@FwNode] is an event listener for a specified node on IEEE 1394 bus. This class is an
 * application of Linux FireWire subsystem. All of operations utilize ioctl(2) with subsystem
 * specific request commands.
 *
 * Since: 1.4
 */

// 256 comes from an actual implementation in kernel land. Read
// 'drivers/firewire/core-device.c'. This value is calculated by a range for
// configuration ROM in ISO/IEC 13213 (IEEE 1212).
#define MAX_CONFIG_ROM_SIZE	256
#define MAX_CONFIG_ROM_LENGTH	(MAX_CONFIG_ROM_SIZE * 4)

typedef struct {
	int fd;

	GMutex mutex;
	guint8 config_rom[MAX_CONFIG_ROM_LENGTH];
	gsize config_rom_length;
	struct fw_cdev_event_bus_reset generation;
	guint32 card_id;

	GList *transactions;
	GMutex transactions_mutex;
} HinawaFwNodePrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwNode, hinawa_fw_node, G_TYPE_OBJECT)

/**
 * hinawa_fw_node_error_quark:
 *
 * Return the [alias@GLib.Quark] for [struct@GLib.Error] with Hinawa.FwNodeError domain.
 *
 * Since: 2.1
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinawa-fw-node-error-quark, hinawa_fw_node_error)

static const char *const err_msgs[] = {
	[HINAWA_FW_NODE_ERROR_DISCONNECTED] = "The associated node is not available for communication",
	[HINAWA_FW_NODE_ERROR_OPENED] = "The instance is already associated to node",
	[HINAWA_FW_NODE_ERROR_NOT_OPENED] = "The instance is not associated to node",
};

#define generate_local_error(error, code) \
	g_set_error_literal(error, HINAWA_FW_NODE_ERROR, code, err_msgs[code])

#define generate_file_error(error, code, format, arg) \
	g_set_error(error, G_FILE_ERROR, code, format, arg)

#define generate_syscall_error(error, errno, format, arg)				\
	g_set_error(error, HINAWA_FW_NODE_ERROR, HINAWA_FW_NODE_ERROR_FAILED,	\
		    format " %d(%s)", arg, errno, strerror(errno))

typedef struct {
	GSource src;
	HinawaFwNode *self;
	gpointer tag;
	size_t len;
	void *buf;
} FwNodeSource;

enum fw_node_prop_type {
	FW_NODE_PROP_TYPE_NODE_ID = 1,
	FW_NODE_PROP_TYPE_LOCAL_NODE_ID,
	FW_NODE_PROP_TYPE_BUS_MANAGER_NODE_ID,
	FW_NODE_PROP_TYPE_IR_MANAGER_NODE_ID,
	FW_NODE_PROP_TYPE_ROOT_NODE_ID,
	FW_NODE_PROP_TYPE_GENERATION,
	FW_NODE_PROP_TYPE_CARD_ID,
	FW_NODE_PROP_TYPE_COUNT,
};
static GParamSpec *fw_node_props[FW_NODE_PROP_TYPE_COUNT] = { NULL, };

// This object has two signals.
enum fw_node_sig_type {
	FW_NODE_SIG_TYPE_BUS_UPDATE = 0,
	FW_NODE_SIG_TYPE_DISCONNECTED,
	FW_NODE_SIG_TYPE_COUNT,
};
static guint fw_node_sigs[FW_NODE_SIG_TYPE_COUNT] = { 0 };

static void fw_node_finalize(GObject *obj)
{
	HinawaFwNode *self = HINAWA_FW_NODE(obj);
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);

	if (priv->fd >= 0)
		close(priv->fd);

	g_list_free(priv->transactions);

	G_OBJECT_CLASS(hinawa_fw_node_parent_class)->finalize(obj);
}

static void fw_node_get_property(GObject *obj, guint id,
				 GValue *val, GParamSpec *spec)
{
	HinawaFwNode *self = HINAWA_FW_NODE(obj);
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);

	g_mutex_lock(&priv->mutex);

	switch (id) {
	case FW_NODE_PROP_TYPE_NODE_ID:
		g_value_set_uint(val, priv->generation.node_id);
		break;
	case FW_NODE_PROP_TYPE_LOCAL_NODE_ID:
		g_value_set_uint(val, priv->generation.local_node_id);
		break;
	case FW_NODE_PROP_TYPE_BUS_MANAGER_NODE_ID:
		g_value_set_uint(val, priv->generation.bm_node_id);
		break;
	case FW_NODE_PROP_TYPE_IR_MANAGER_NODE_ID:
		g_value_set_uint(val, priv->generation.irm_node_id);
		break;
	case FW_NODE_PROP_TYPE_ROOT_NODE_ID:
		g_value_set_uint(val, priv->generation.root_node_id);
		break;
	case FW_NODE_PROP_TYPE_GENERATION:
		g_value_set_uint(val, priv->generation.generation);
		break;
	case FW_NODE_PROP_TYPE_CARD_ID:
		g_value_set_uint(val, priv->card_id);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}

	g_mutex_unlock(&priv->mutex);
}

static void hinawa_fw_node_class_init(HinawaFwNodeClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_node_finalize;
	gobject_class->get_property = fw_node_get_property;

	/**
	 * HinawaFwNode:node-id:
	 *
	 * Node ID of node associated to instance of object at current generation of bus topology.
	 * This parameter is effective after the association.
	 *
	 * Since: 1.4
	 */
	fw_node_props[FW_NODE_PROP_TYPE_NODE_ID] =
		g_param_spec_uint("node-id", "node-id",
				  "Node ID of node associated to instance of object at current "
				  "generation of bus topology. This parameter is effective after "
				  "the association.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	/**
	 * HinawaFwNode:local-node-id:
	 *
	 * Node ID of node which application uses to communicate to node associated to instance of
	 * object at current generation of bus topology. In general, it is for 1394 OHCI controller.
	 *
	 * Since: 1.4
	 */
	fw_node_props[FW_NODE_PROP_TYPE_LOCAL_NODE_ID] =
		g_param_spec_uint("local-node-id", "local-node-id",
				  "Node ID of node which application uses to communicate to node "
				  "associated to instance of object at current generation of bus "
				  "topology. In general, it is for OHCI 1394 host controller.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	/**
	 * HinawaFwNode:bus-manager-node-id:
	 *
	 * Node ID of node which plays role of bus manager at current generation of bus topology.
	 *
	 * Since: 1.4
	 */
	fw_node_props[FW_NODE_PROP_TYPE_BUS_MANAGER_NODE_ID] =
		g_param_spec_uint("bus-manager-node-id", "bus-manager-node-id",
				  "Node ID of node which plays role of bus manager at current "
				  "generation of bus topology.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	/**
	 * HinawaFwNode:ir-manager-node-id:
	 *
	 * Node ID of node which plays role of isochronous resource manager at current generation
	 * of bus topology.
	 *
	 * Since: 1.4
	 */
	fw_node_props[FW_NODE_PROP_TYPE_IR_MANAGER_NODE_ID] =
		g_param_spec_uint("ir-manager-node-id", "ir-manager-node-id",
				  "Node ID of node which plays role of isochronous resource "
				  "manager at current generation of bus topology.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	/**
	 * HinawaFwNode:root-node-id:
	 *
	 * Node ID of root node in bus topology at current generation of the bus topology.
	 *
	 * Since: 1.4
	 */
	fw_node_props[FW_NODE_PROP_TYPE_ROOT_NODE_ID] =
		g_param_spec_uint("root-node-id", "root-node-id",
				  "Node ID of root node in bus topology at current generation of "
				  "the bus topology.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	/**
	 * HinawaFwNode:generation:
	 *
	 * Current generation of bus topology.
	 *
	 * Since: 1.4
	 */
	fw_node_props[FW_NODE_PROP_TYPE_GENERATION] =
		g_param_spec_uint("generation", "generation",
				  "Current generation of bus topology",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	/**
	 * HinawaFwNode:card-id:
	 *
	 * The numeric index for 1394 OHCI hardware used for the communication to the node. The
	 * value is stable against bus generation.
	 *
	 * Since: 3.0
	 */
	fw_node_props[FW_NODE_PROP_TYPE_CARD_ID] =
		g_param_spec_uint("card-id", "card-id",
				  "The numeric index for 1394 OHCI hardware used for the "
				  "communication to the node",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_NODE_PROP_TYPE_COUNT,
					  fw_node_props);

	/**
	 * HinawaFwNode::bus-update:
	 * @self: A [class@FwNode].
	 *
	 * Emitted when IEEE 1394 bus is updated. Handlers can read current generation in the bus
	 * via [property@FwNode:generation] property.
	 *
	 * Since: 1.4
	 */
	fw_node_sigs[FW_NODE_SIG_TYPE_BUS_UPDATE] =
		g_signal_new("bus-update",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwNodeClass, bus_update),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0, G_TYPE_NONE);

	/**
	 * HinawaFwNode::disconnected:
	 * @self: A [class@FwNode].
	 *
	 * Emitted when the node is not available anymore due to removal from IEEE 1394 bus. It's
	 * preferable to call [method@GObject.Object.unref] immediately to release file descriptor.
	 *
	 * Since: 1.4
	 */
	fw_node_sigs[FW_NODE_SIG_TYPE_DISCONNECTED] =
		g_signal_new("disconnected",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwNodeClass, disconnected),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0, G_TYPE_NONE);
}

static void hinawa_fw_node_init(HinawaFwNode *self)
{
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);

	priv->fd = -1;
	g_mutex_init(&priv->mutex);
	g_mutex_init(&priv->transactions_mutex);
}

/**
 * hinawa_fw_node_new:
 *
 * Instantiate [class@FwNode] object and return the instance.
 *
 * Returns: an instance of [class@FwNode].
 * Since: 1.4
 */
HinawaFwNode *hinawa_fw_node_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_NODE, NULL);
}

static int update_info(HinawaFwNode *self)
{
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);
	struct fw_cdev_get_info info = {0};
	guint32 *rom;
	unsigned int quads;
	int i;

	// The interface version 4 is used for:
	//    - struct fw_cdev_allocate.region_end
	// The interface version 6 is used for:
	//    - struct fw_cdev_event_request3
	//    - struct fw_cdev_event_response2
	info.version = 6;
	info.rom = (__u64)priv->config_rom;
	info.rom_length = MAX_CONFIG_ROM_LENGTH;
	info.bus_reset = (__u64)&priv->generation;
	info.bus_reset_closure = (__u64)self;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_INFO, &info) < 0)
		return errno;

	// Linux FireWire subsystem caches the content of configuration ROM by host-endian.
	rom = (guint32 *)priv->config_rom;
	quads = (info.rom_length + 3) / 4;
	for (i = 0; i < quads; ++i)
		rom[i] = GUINT32_TO_BE(rom[i]);
	priv->config_rom_length = info.rom_length;

	priv->card_id = info.card;

	return 0;
}

/**
 * hinawa_fw_node_open:
 * @self: A [class@FwNode]
 * @path: A path to Linux FireWire character device
 * @open_flag: The flag of `open(2)` system call. `O_RDONLY` is fulfilled internally.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; GLib.Error and
 *	   Hinawa.FwNodeError.
 *
 * Open Linux FireWire character device to operate node on IEEE 1394 bus.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 3.0
 */
gboolean hinawa_fw_node_open(HinawaFwNode *self, const gchar *path, gint open_flag, GError **error)
{
	HinawaFwNodePrivate *priv;
	int err;

	g_return_val_if_fail(HINAWA_IS_FW_NODE(self), FALSE);
	g_return_val_if_fail(path != NULL && strlen(path) > 0, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinawa_fw_node_get_instance_private(self);
	if (priv->fd >= 0) {
		generate_local_error(error, HINAWA_FW_NODE_ERROR_OPENED);
		return FALSE;
	}

	open_flag |= O_RDONLY;
	priv->fd = open(path, open_flag);
	if (priv->fd < 0) {
		if (errno == ENODEV) {
			generate_local_error(error, HINAWA_FW_NODE_ERROR_DISCONNECTED);
		} else {
			GFileError code = g_file_error_from_errno(errno);
			if (code != G_FILE_ERROR_FAILED)
				generate_file_error(error, code, "open(%s)", path);
			else
				generate_syscall_error(error, errno, "open(%s)", path);
		}
		return FALSE;
	}

	g_mutex_lock(&priv->mutex);
	err = update_info(self);
	g_mutex_unlock(&priv->mutex);

	if (err > 0) {
		if (err == ENODEV)
			generate_local_error(error, HINAWA_FW_NODE_ERROR_DISCONNECTED);
		else
			generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_GET_INFO");
		close(priv->fd);
		priv->fd = -1;

		return FALSE;
	}

	return TRUE;
}

/**
 * hinawa_fw_node_get_config_rom:
 * @self: A [class@FwNode]
 * @image: (array length=length)(out)(transfer none): The content of configuration ROM.
 * @length: (out): The number of bytes consists of the configuration rom.
 * @error: A [struct@GLib.Error].
 *
 * Get cached content of configuration ROM aligned to big-endian.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 3.0
 */
gboolean hinawa_fw_node_get_config_rom(HinawaFwNode *self, const guint8 **image, gsize *length,
				       GError **error)
{
	HinawaFwNodePrivate *priv;

	g_return_val_if_fail(HINAWA_IS_FW_NODE(self), FALSE);
	g_return_val_if_fail(image != NULL, FALSE);
	g_return_val_if_fail(length != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinawa_fw_node_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_FW_NODE_ERROR_NOT_OPENED);
		return FALSE;
	}

	g_mutex_lock(&priv->mutex);

	*image = priv->config_rom;
	*length = priv->config_rom_length;

	g_mutex_unlock(&priv->mutex);

	return TRUE;
}

/**
 * hinawa_fw_node_read_cycle_time:
 * @self: A [class@FwNode]
 * @clock_id: The numeric ID of clock source for the reference timestamp. One of CLOCK_REALTIME(0),
 *	      CLOCK_MONOTONIC(1), and CLOCK_MONOTONIC_RAW(4) is available in UAPI of Linux kernel.
 * @cycle_time: (inout): A [struct@CycleTime].
 * @error: A [struct@GLib.Error].
 *
 * Read current value of CYCLE_TIME register in 1394 OHCI controller.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 3.0
 */
gboolean hinawa_fw_node_read_cycle_time(HinawaFwNode *self, gint clock_id,
					HinawaCycleTime **cycle_time, GError **error)
{
	int err;

	g_return_val_if_fail(cycle_time != NULL, FALSE);

	(*cycle_time)->clk_id = clock_id;
	err = hinawa_fw_node_ioctl(self, FW_CDEV_IOC_GET_CYCLE_TIMER2, *cycle_time, error);
	if (err < 0 && *error == NULL)
		generate_syscall_error(error, errno, "ioctl(%s)", "FW_CDEV_IOC_GET_CYCLE_TIMER2");

	return err == 0;
}

static void handle_update(HinawaFwNode *self)
{
	HinawaFwNodePrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_NODE(self));
	priv = hinawa_fw_node_get_instance_private(self);

	g_mutex_lock(&priv->mutex);
	update_info(self);
	g_mutex_unlock(&priv->mutex);

	g_signal_emit(self, fw_node_sigs[FW_NODE_SIG_TYPE_BUS_UPDATE], 0, NULL);
}

static gboolean check_src(GSource *gsrc)
{
	FwNodeSource *src = (FwNodeSource *)gsrc;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an error, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(gsrc, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	FwNodeSource *src = (FwNodeSource *)gsrc;
	HinawaFwNodePrivate *priv;
	GIOCondition condition;
	const union fw_cdev_event *event;
	gpointer instance;
	__u32 event_type;
	ssize_t len;

	priv = hinawa_fw_node_get_instance_private(src->self);
	if (priv->fd < 0)
		return G_SOURCE_REMOVE;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR) {
		g_signal_emit(src->self,
			fw_node_sigs[FW_NODE_SIG_TYPE_DISCONNECTED], 0);
		return G_SOURCE_REMOVE;
	}

	len = read(priv->fd, src->buf, src->len);
	if (len < 0) {
		if (errno == EAGAIN)
			return G_SOURCE_CONTINUE;

		return G_SOURCE_REMOVE;
	}

	event = (const union fw_cdev_event *)src->buf;
	instance = (gpointer)event->common.closure;
	event_type = event->common.type;

	if (HINAWA_IS_FW_NODE(instance) && event_type == FW_CDEV_EVENT_BUS_RESET) {
		handle_update(src->self);
	} else if (HINAWA_IS_FW_RESP(instance)) {
		HinawaFwResp *resp = HINAWA_FW_RESP(instance);

		switch (event_type) {
		case FW_CDEV_EVENT_REQUEST:
			hinawa_fw_resp_handle_request(resp, &event->request);
			break;
		case FW_CDEV_EVENT_REQUEST2:
			hinawa_fw_resp_handle_request2(resp, &event->request2);
			break;
		case FW_CDEV_EVENT_REQUEST3:
			hinawa_fw_resp_handle_request3(resp, &event->request3);
			break;
		default:
			break;
		}
	} else if (HINAWA_IS_FW_REQ(instance)) {
		HinawaFwReq *req = HINAWA_FW_REQ(instance);
		GList *entry;

		// Don't process request invalidated in advance.
		g_mutex_lock(&priv->transactions_mutex);
		entry = g_list_find(priv->transactions, req);
		if (entry) {
			priv->transactions = g_list_delete_link(priv->transactions, entry);

			switch (event_type) {
			case FW_CDEV_EVENT_RESPONSE:
				hinawa_fw_req_handle_response(req, &event->response);
				break;
			case FW_CDEV_EVENT_RESPONSE2:
				hinawa_fw_req_handle_response2(req, &event->response2);
				break;
			default:
				break;
			}
		}
		g_mutex_unlock(&priv->transactions_mutex);
	}

	// Just be sure to continue to process this source.
	return G_SOURCE_CONTINUE;
}

static void finalize_src(GSource *gsrc)
{
	FwNodeSource *src = (FwNodeSource *)gsrc;

	g_free(src->buf);
}

/**
 * hinawa_fw_node_create_source:
 * @self: A [class@FwNode].
 * @gsrc: (out): A [struct@GLib.Source].
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinawa.FwNodeError.
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for the node on
 * IEEE 1394 bus.
 *
 * Returns: TRUE if the overall operation finishes successfully, otherwise FALSE.
 *
 * Since: 3.0
 */
gboolean hinawa_fw_node_create_source(HinawaFwNode *self, GSource **gsrc, GError **error)
{
        static GSourceFuncs funcs = {
                .check          = check_src,
                .dispatch       = dispatch_src,
                .finalize       = finalize_src,
        };
	HinawaFwNodePrivate *priv;
	FwNodeSource *src;

	g_return_val_if_fail(HINAWA_IS_FW_NODE(self), FALSE);
	g_return_val_if_fail(gsrc != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	priv = hinawa_fw_node_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_FW_NODE_ERROR_NOT_OPENED);
		return FALSE;
	}

        *gsrc = g_source_new(&funcs, sizeof(FwNodeSource));
	src = (FwNodeSource *)(*gsrc);

        g_source_set_name(*gsrc, "HinawaFwNode");

        // MEMO: allocate one page because we cannot assume the size of
        // transaction frame.
        src->len = sysconf(_SC_PAGESIZE);
        src->buf = g_malloc0(src->len);

	src->self = self;
	src->tag = g_source_add_unix_fd(*gsrc, priv->fd, G_IO_IN);

	return TRUE;
}

// Internal use only.
int hinawa_fw_node_ioctl(HinawaFwNode *self, unsigned long req, void *args, GError **error)
{
	HinawaFwNodePrivate *priv;

	g_return_val_if_fail(HINAWA_IS_FW_NODE(self), ENXIO);
	g_return_val_if_fail(error != NULL, EINVAL);

	priv = hinawa_fw_node_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_FW_NODE_ERROR_NOT_OPENED);
		return ENXIO;
	}

	// To invalidate the transaction in a case of timeout.
	if (req == FW_CDEV_IOC_SEND_REQUEST) {
		struct fw_cdev_send_request *data = args;
		HinawaFwReq *req = (HinawaFwReq *)data->closure;
		priv->transactions = g_list_prepend(priv->transactions, req);
	}

	if (ioctl(priv->fd, req, args) < 0) {
		if (errno == ENODEV)
			generate_local_error(error, HINAWA_FW_NODE_ERROR_DISCONNECTED);
		return errno;
	}

	return 0;
}

void hinawa_fw_node_invalidate_transaction(HinawaFwNode *self, HinawaFwReq *req)
{
	HinawaFwNodePrivate *priv;
	GList *entry;

	g_return_if_fail(HINAWA_IS_FW_NODE(self));
	priv = hinawa_fw_node_get_instance_private(self);

	g_mutex_lock(&priv->transactions_mutex);
	entry = g_list_find(priv->transactions, req);
	if (entry) {
		priv->transactions =
				g_list_delete_link(priv->transactions, entry);
	}
	g_mutex_unlock(&priv->transactions_mutex);
}
