/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "internal.h"

/**
 * SECTION:fw_unit
 * @Title: HinawaFwUnit
 * @Short_description: An object to maintain association between IEEE 1394 unit
 *		       and its parent.
 *
 * A #HinawaFwUnit object has a reference to an instance of HinawaFwNode for
 * Linux FireWire character device, corresponding to parent node on IEEE 1394
 * bus. This object is expected to be used by inheritance from subclasses; e.g.
 * HinawaSndUnit, and should not be instantiated directly for newly written
 * code since the object is planned to be abstract class in future libhinawa
 * release.
 *
 * All of operations are done by associated HinawaFwNode. Some APIs and
 * properties which #HinawaFwUnit has are maintained just for backward
 * compatibility and already deprecated. Instead, use associated HinawaFwNode.
 */

struct _HinawaFwUnitPrivate {
	HinawaFwNode *node;
};
// TODO: use G_DEFINE_ABSTRACT_TYPE().
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwUnit, hinawa_fw_unit, G_TYPE_OBJECT)

static void fw_unit_finalize(GObject *obj)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);
	HinawaFwUnitPrivate *priv = hinawa_fw_unit_get_instance_private(self);

	g_object_unref(priv->node);

	G_OBJECT_CLASS(hinawa_fw_unit_parent_class)->finalize(obj);
}

static void hinawa_fw_unit_class_init(HinawaFwUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = fw_unit_finalize;
}

static void hinawa_fw_unit_init(HinawaFwUnit *self)
{
	HinawaFwUnitPrivate *priv= hinawa_fw_unit_get_instance_private(self);
	priv->node = hinawa_fw_node_new();
}

/**
 * hinawa_fw_unit_open:
 * @self: A #HinawaFwUnit
 * @path: A path to Linux FireWire character device
 * @exception: A #GError
 *
 * Open Linux FireWire character device to operate for node on IEEE 1394 bus.
 */
void hinawa_fw_unit_open(HinawaFwUnit *self, gchar *path, GError **exception)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	hinawa_fw_node_open(priv->node, path, exception);
}

/**
 * hinawa_fw_unit_get_node:
 * @self: A #HinawaFwUnit.
 * @node: (out)(transfer none): A #HinawaFwNode.
 *
 * Retrieve an instance of #HinawaFwNode associated to the given unit.
 *
 * Since: 1.4.
 */
void hinawa_fw_unit_get_node(HinawaFwUnit *self, HinawaFwNode **node)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	*node = priv->node;
}
