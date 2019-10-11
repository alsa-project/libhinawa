/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "fw_node.h"

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
 * Since: 1.4
 */

// 256 comes from an actual implementation in kernel land. Read
// 'drivers/firewire/core-device.c'. This value is calculated by a range for
// configuration ROM in ISO/IEC 13213 (IEEE 1212).
#define MAX_CONFIG_ROM_SIZE	256
#define MAX_CONFIG_ROM_LENGTH	(MAX_CONFIG_ROM_SIZE * 4)

struct _HinawaFwNodePrivate {
	int fd;

	guint8 config_rom[MAX_CONFIG_ROM_LENGTH];
	unsigned int config_rom_length;
	struct fw_cdev_event_bus_reset generation;
};

G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwNode, hinawa_fw_node, G_TYPE_OBJECT)

// For error handling.
G_DEFINE_QUARK("HinawaFwNode", hinawa_fw_node)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_node_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

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

static void fw_node_finalize(GObject *obj)
{
	HinawaFwNode *self = HINAWA_FW_NODE(obj);
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);

	if (priv->fd >= 0)
		close(priv->fd);

	G_OBJECT_CLASS(hinawa_fw_node_parent_class)->finalize(obj);
}

static void fw_node_get_property(GObject *obj, guint id,
				 GValue *val, GParamSpec *spec)
{
	HinawaFwNode *self = HINAWA_FW_NODE(obj);
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);

	switch (id) {
	case FW_NODE_PROP_TYPE_NODE_ID:
		g_value_set_ulong(val, priv->generation.node_id);
		break;
	case FW_NODE_PROP_TYPE_LOCAL_NODE_ID:
		g_value_set_ulong(val, priv->generation.local_node_id);
		break;
	case FW_NODE_PROP_TYPE_BUS_MANAGER_NODE_ID:
		g_value_set_ulong(val, priv->generation.bm_node_id);
		break;
	case FW_NODE_PROP_TYPE_IR_MANAGER_NODE_ID:
		g_value_set_ulong(val, priv->generation.irm_node_id);
		break;
	case FW_NODE_PROP_TYPE_ROOT_NODE_ID:
		g_value_set_ulong(val, priv->generation.root_node_id);
		break;
	case FW_NODE_PROP_TYPE_GENERATION:
		g_value_set_ulong(val, priv->generation.generation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_node_set_property(GObject *obj, guint id,
				 const GValue *val, GParamSpec *spec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
}

static void hinawa_fw_node_class_init(HinawaFwNodeClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_node_finalize;
	gobject_class->get_property = fw_node_get_property;
	gobject_class->set_property = fw_node_set_property;

	fw_node_props[FW_NODE_PROP_TYPE_NODE_ID] =
		g_param_spec_ulong("node-id", "node-id",
				   "Node-ID of this node at this generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_LOCAL_NODE_ID] =
		g_param_spec_ulong("local-node-id", "local-node-id",
				   "Node-ID for a node which this node use to "
				   "communicate to the other nodes on the bus "
				   "at this generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_BUS_MANAGER_NODE_ID] =
		g_param_spec_ulong("bus-manager-node-id", "bus-manager-node-id",
				   "Node-ID for bus manager on the bus at this "
				   "generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_IR_MANAGER_NODE_ID] =
		g_param_spec_ulong("ir-manager-node-id", "ir-manager-node-id",
				   "Node-ID for isochronous resource manager "
				   "on the bus at this generation",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_ROOT_NODE_ID] =
		g_param_spec_ulong("root-node-id", "root-node-id",
				   "Node-ID for root of bus topology at this "
				   "generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_node_props[FW_NODE_PROP_TYPE_GENERATION] =
		g_param_spec_ulong("generation", "generation",
				   "current level of generation on this bus.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_NODE_PROP_TYPE_COUNT,
					  fw_node_props);
}

static void hinawa_fw_node_init(HinawaFwNode *self)
{
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);

	priv->fd = -1;
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

	update_info(self, exception);
}

/**
 * hinawa_fw_node_get_config_rom:
 * @self: A #HinawaFwNode
 * @image: (element-type guint8)(array length=length)(out): The content of
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

	*image = priv->config_rom;
	*length = priv->config_rom_length;
}
