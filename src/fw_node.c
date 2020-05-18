/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <linux/firewire-cdev.h>

/**
 * SECTION:fw_node
 * @Title: HinawaFwNode
 * @Short_description: An event listener for FireWire node
 *
 * A #HinawaFwNode is an event listener for a specified node on IEEE 1394 bus.
 * This class is an application of Linux FireWire subsystem.
 * All of operations utilize ioctl(2) with subsystem specific request commands.
 *
 * Since: 1.4.
 */

// 256 comes from an actual implementation in kernel land. Read
// 'drivers/firewire/core-device.c'. This value is calculated by a range for
// configuration ROM in ISO/IEC 13213 (IEEE 1212).
#define MAX_CONFIG_ROM_SIZE	256
#define MAX_CONFIG_ROM_LENGTH	(MAX_CONFIG_ROM_SIZE * 4)

struct _HinawaFwNodePrivate {
	int fd;

	GMutex mutex;
	guint8 config_rom[MAX_CONFIG_ROM_LENGTH];
	unsigned int config_rom_length;
	struct fw_cdev_event_bus_reset generation;

	GList *transactions;
	GMutex transactions_mutex;
};

G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwNode, hinawa_fw_node, G_TYPE_OBJECT)

// For error handling.
G_DEFINE_QUARK("HinawaFwNode", hinawa_fw_node)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_node_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

typedef struct {
	GSource src;
	HinawaFwNode *self;
	gpointer tag;
	unsigned int len;
	void *buf;
} FwNodeSource;

enum fw_node_prop_type {
	FW_NODE_PROP_TYPE_NODE_ID = 1,
	FW_NODE_PROP_TYPE_LOCAL_NODE_ID,
	FW_NODE_PROP_TYPE_BUS_MANAGER_NODE_ID,
	FW_NODE_PROP_TYPE_IR_MANAGER_NODE_ID,
	FW_NODE_PROP_TYPE_ROOT_NODE_ID,
	FW_NODE_PROP_TYPE_GENERATION,
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

	fw_node_props[FW_NODE_PROP_TYPE_NODE_ID] =
		g_param_spec_uint("node-id", "node-id",
				  "Node-ID of this node at this generation.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_LOCAL_NODE_ID] =
		g_param_spec_uint("local-node-id", "local-node-id",
				  "Node-ID for a node which this node use to "
				  "communicate to the other nodes on the bus "
				  "at this generation.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_BUS_MANAGER_NODE_ID] =
		g_param_spec_uint("bus-manager-node-id", "bus-manager-node-id",
				  "Node-ID for bus manager on the bus at this "
				  "generation.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_IR_MANAGER_NODE_ID] =
		g_param_spec_uint("ir-manager-node-id", "ir-manager-node-id",
				  "Node-ID for isochronous resource manager "
				  "on the bus at this generation",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_ROOT_NODE_ID] =
		g_param_spec_uint("root-node-id", "root-node-id",
				  "Node-ID for root of bus topology at this "
				  "generation.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_GENERATION] =
		g_param_spec_uint("generation", "generation",
				  "current level of generation on this bus.",
				  0, G_MAXUINT32, 0,
				  G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_NODE_PROP_TYPE_COUNT,
					  fw_node_props);

	/**
	 * HinawaFwNode::bus-update:
	 * @self: A #HinawaFwNode.
	 *
	 * When IEEE 1394 bus is updated, the ::bus-update signal is generated.
	 * Handlers can read current generation in the bus via 'generation'
	 * property.
	 *
	 * Since: 1.4.
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
	 * @self: A #HinawaFwNode.
	 *
	 * When phicical FireWire devices are disconnected from IEEE 1394 bus,
	 * the ::disconnected signal is generated.
	 *
	 * Since: 1.4.
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
 * Instantiate #HinawaFwNode object and return the instance.
 *
 * Returns: an instance of #HinawaFwNode.
 * Since: 1.4.
 */
HinawaFwNode *hinawa_fw_node_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_NODE, NULL);
}

static void update_info(HinawaFwNode *self, GError **exception)
{
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);
	struct fw_cdev_get_info info = {0};
	guint32 *rom;
	unsigned int quads;
	int i;

	// Duplicate generation parameters in userspace.
	info.version = 4;
	info.rom = (__u64)priv->config_rom;
	info.rom_length = MAX_CONFIG_ROM_LENGTH;
	info.bus_reset = (__u64)&priv->generation;
	info.bus_reset_closure = (__u64)self;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		raise(exception, errno);
		return;
	}

	// Align buffer for configuration ROM according to host endianness,
	// because Linux firewire subsystem copies raw data to it.
	rom = (guint32 *)priv->config_rom;
	quads = (info.rom_length + 3) / 4;
	for (i = 0; i < quads; ++i)
		rom[i] = GUINT32_FROM_BE(rom[i]);
	priv->config_rom_length = info.rom_length;
}

/**
 * hinawa_fw_node_open:
 * @self: A #HinawaFwNode
 * @path: A path to Linux FireWire character device
 * @exception: A #GError
 *
 * Open Linux FireWire character device to operate node on IEEE 1394 bus.
 *
 * Since: 1.4.
 */
void hinawa_fw_node_open(HinawaFwNode *self, const gchar *path,
			 GError **exception)
{
	HinawaFwNodePrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_NODE(self));
	priv = hinawa_fw_node_get_instance_private(self);

	if (priv->fd >= 0) {
		raise(exception, EBUSY);
		return;
	}

	priv->fd = open(path, O_RDONLY);
	if (priv->fd < 0)
		raise(exception, errno);

	g_mutex_lock(&priv->mutex);
	update_info(self, exception);
	g_mutex_unlock(&priv->mutex);
}

/**
 * hinawa_fw_node_get_config_rom:
 * @self: A #HinawaFwNode
 * @image: (array length=length)(out)(transfer none): The content of
 *	   configuration ROM.
 * @length: (out): The number of bytes consists of the configuration rom.
 * @exception: A #GError.
 *
 * Get cached content of configuration ROM.
 *
 * Since: 1.4.
 */
void hinawa_fw_node_get_config_rom(HinawaFwNode *self, const guint8 **image,
				   guint *length, GError **exception)
{
	HinawaFwNodePrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_NODE(self));
	priv = hinawa_fw_node_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

	if (image == NULL || length == NULL) {
		raise(exception, EINVAL);
		return;
	}

	g_mutex_lock(&priv->mutex);

	*image = priv->config_rom;
	*length = priv->config_rom_length;

	g_mutex_unlock(&priv->mutex);
}

static void handle_update(HinawaFwNode *self, GError **exception)
{
	HinawaFwNodePrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_NODE(self));
	priv = hinawa_fw_node_get_instance_private(self);

	g_mutex_lock(&priv->mutex);
	update_info(self, exception);
	g_mutex_unlock(&priv->mutex);

	g_signal_emit(self, fw_node_sigs[FW_NODE_SIG_TYPE_BUS_UPDATE], 0, NULL);
}

static gboolean check_src(GSource *gsrc)
{
	FwNodeSource *src = (FwNodeSource *)gsrc;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an exception, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(gsrc, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	FwNodeSource *src = (FwNodeSource *)gsrc;
	HinawaFwNodePrivate *priv;
	GIOCondition condition;
	struct fw_cdev_event_common *common;
	GError *exception = NULL;
	int len;

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

	common = (struct fw_cdev_event_common *)src->buf;

	if (HINAWA_IS_FW_NODE(common->closure) &&
	    common->type == FW_CDEV_EVENT_BUS_RESET)
		handle_update(src->self, &exception);
	else if (HINAWA_IS_FW_RESP(common->closure) &&
		 common->type == FW_CDEV_EVENT_REQUEST2)
		hinawa_fw_resp_handle_request(HINAWA_FW_RESP(common->closure),
				(struct fw_cdev_event_request2 *)common);
	else if (HINAWA_IS_FW_REQ(common->closure) &&
		 common->type == FW_CDEV_EVENT_RESPONSE) {
		struct fw_cdev_event_response *ev =
				(struct fw_cdev_event_response *)common;
		HinawaFwReq *req = HINAWA_FW_REQ(ev->closure);
		GList *entry;

		// Don't process request invalidated in advance.
		g_mutex_lock(&priv->transactions_mutex);
		entry = g_list_find(priv->transactions, req);
		if (entry) {
			priv->transactions =
				g_list_delete_link(priv->transactions, entry);
			hinawa_fw_req_handle_response(req, ev);
		}
		g_mutex_unlock(&priv->transactions_mutex);
	}

	if (exception != NULL)
		return G_SOURCE_REMOVE;

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
 * @self: A #HinawaFwNode.
 * @gsrc: (out): A #GSource.
 * @exception: A #GError.
 *
 * Create Gsource for GMainContext to dispatch events for the node on IEEE 1394
 * bus.
 *
 * Since: 1.4.
 */
void hinawa_fw_node_create_source(HinawaFwNode *self, GSource **gsrc,
				  GError **exception)
{
        static GSourceFuncs funcs = {
                .check          = check_src,
                .dispatch       = dispatch_src,
                .finalize       = finalize_src,
        };
	HinawaFwNodePrivate *priv;
	FwNodeSource *src;

	g_return_if_fail(HINAWA_IS_FW_NODE(self));
	priv = hinawa_fw_node_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

        *gsrc = g_source_new(&funcs, sizeof(FwNodeSource));
	src = (FwNodeSource *)(*gsrc);

        g_source_set_name(*gsrc, "HinawaFwNode");
        g_source_set_priority(*gsrc, G_PRIORITY_HIGH_IDLE);
        g_source_set_can_recurse(*gsrc, TRUE);

        // MEMO: allocate one page because we cannot assume the size of
        // transaction frame.
        src->len = sysconf(_SC_PAGESIZE);
        src->buf = g_malloc0(src->len);
        if (src->buf == NULL) {
                raise(exception, ENOMEM);
		g_source_unref(*gsrc);
                return;
        }

	src->self = self;
	src->tag = g_source_add_unix_fd(*gsrc, priv->fd, G_IO_IN);
}

// Internal use only.
void hinawa_fw_node_ioctl(HinawaFwNode *self, unsigned long req, void *args,
			  int *err)
{
	HinawaFwNodePrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_NODE(self));
	priv = hinawa_fw_node_get_instance_private(self);

	// To invalidate the transaction in a case of timeout.
	if (req == FW_CDEV_IOC_SEND_REQUEST) {
		struct fw_cdev_send_request *data = args;
		HinawaFwReq *req = (HinawaFwReq *)data->closure;
		priv->transactions = g_list_prepend(priv->transactions, req);
	}

	*err = 0;
	if (ioctl(priv->fd, req, args) < 0)
		*err = -errno;
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
