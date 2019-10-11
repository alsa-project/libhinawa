/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "fw_node.h"

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

G_DEFINE_TYPE(HinawaFwNode, hinawa_fw_node, G_TYPE_OBJECT)

// For error handling.
G_DEFINE_QUARK("HinawaFwNode", hinawa_fw_node)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_node_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

static void hinawa_fw_node_class_init(HinawaFwNodeClass *klass)
{
	return;
}

static void hinawa_fw_node_init(HinawaFwNode *self)
{
	return;
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
