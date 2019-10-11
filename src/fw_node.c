/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "fw_node.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

struct _HinawaFwNodePrivate {
	int fd;
};

G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwNode, hinawa_fw_node, G_TYPE_OBJECT)

// For error handling.
G_DEFINE_QUARK("HinawaFwNode", hinawa_fw_node)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_node_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

static void fw_node_finalize(GObject *obj)
{
	HinawaFwNode *self = HINAWA_FW_NODE(obj);
	HinawaFwNodePrivate *priv = hinawa_fw_node_get_instance_private(self);

	if (priv->fd >= 0)
		close(priv->fd);

	G_OBJECT_CLASS(hinawa_fw_node_parent_class)->finalize(obj);
}

static void hinawa_fw_node_class_init(HinawaFwNodeClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_node_finalize;
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
}
