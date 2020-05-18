/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "internal.h"

// For error handling.
G_DEFINE_QUARK("HinawaFwUnit", hinawa_fw_unit)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_unit_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

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

// This object has two deprecated signals.
enum fw_unit_sig_type {
	FW_UNIT_SIG_TYPE_BUS_UPDATE = 0,
	FW_UNIT_SIG_TYPE_DISCONNECTED,
	FW_UNIT_SIG_TYPE_COUNT,
};
static guint fw_unit_sigs[FW_UNIT_SIG_TYPE_COUNT] = { 0 };

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

	/**
	 * HinawaFwUnit::bus-update:
	 * @self: A #HinawaFwUnit
	 *
	 * When IEEE 1394 bus is updated, the ::bus-update signal is generated.
	 * Handlers can read current generation in the bus via 'generation'
	 * property.
	 *
	 * Deprecated: 1.4: Use an instance of HinawaFwNode retrieved by a call
	 *		    of hinawa_fw_unit_get_node().
	 */
	fw_unit_sigs[FW_UNIT_SIG_TYPE_BUS_UPDATE] =
		g_signal_new("bus-update",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwUnitClass, bus_update),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0, G_TYPE_NONE);

	/**
	 * HinawaFwUnit::disconnected:
	 * @self: A #HinawaFwUnit
	 *
	 * When physical FireWire devices are disconnected from IEEE 1394 bus,
	 * the #HinawaFwUnit becomes unlistening and emits this signal.
	 *
	 * Deprecated: 1.4: Use an instance of HinawaFwNode retrieved by a call
	 *		    of hinawa_fw_unit_get_node().
	 */
	fw_unit_sigs[FW_UNIT_SIG_TYPE_DISCONNECTED] =
		g_signal_new("disconnected",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaFwUnitClass, disconnected),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0, G_TYPE_NONE);
}

static void hinawa_fw_unit_init(HinawaFwUnit *self)
{
	HinawaFwUnitPrivate *priv= hinawa_fw_unit_get_instance_private(self);
	priv->node = hinawa_fw_node_new();
}

/**
 * hinawa_fw_unit_new:
 *
 * Instantiate #HinawaFwUnit object and return the instance.
 *
 * Returns: an instance of #HinawaFwUnit.
 * Since: 1.3.
 * Deprecated: 1.4: HinawaFwUnit is planned to be an abstract class in future
 *		    release. Please instantiate for derived class, instead.
 */
HinawaFwUnit *hinawa_fw_unit_new(void)
{
	return g_object_new(HINAWA_TYPE_FW_UNIT, NULL);
}

static void handle_bus_update(HinawaFwNode *node, gpointer arg)
{
	HinawaFwUnit *self = (HinawaFwUnit *)arg;

	g_signal_emit(self, fw_unit_sigs[FW_UNIT_SIG_TYPE_BUS_UPDATE], 0, NULL);
}

static void handle_disconnected(HinawaFwNode *node, gpointer arg)
{
	HinawaFwUnit *self = (HinawaFwUnit *)arg;

	g_signal_emit(self, fw_unit_sigs[FW_UNIT_SIG_TYPE_DISCONNECTED], 0);
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
	if (*exception != NULL)
		return;

	g_signal_connect(G_OBJECT(priv->node), "bus_update",
			 G_CALLBACK(handle_bus_update), self);
	g_signal_connect(G_OBJECT(priv->node), "disconnected",
			 G_CALLBACK(handle_disconnected), self);
}

/**
 * hinawa_fw_unit_get_config_rom:
 * @self: A #HinawaFwUnit
 * @length: (out)(optional): the number of bytes consists of the config rom
 *
 * Get cached content of configuration ROM.
 *
 * Returns: (element-type guint8)(array length=length)(transfer none): config
 *	    rom image.
 *
 * Deprecated: 1.4: Instead, use hinawa_fw_node_get_config_rom() for an instance
 *		    of HinawaFwNode retrieved by a call of
 *		    hinawa_fw_unit_get_node().
 */
const guint8 *hinawa_fw_unit_get_config_rom(HinawaFwUnit *self, guint *length)
{
	HinawaFwUnitPrivate *priv;
	const guint8 *image;
	GError *exception;

	g_return_val_if_fail(HINAWA_IS_FW_UNIT(self), NULL);
	priv = hinawa_fw_unit_get_instance_private(self);

	exception = NULL;
	hinawa_fw_node_get_config_rom(priv->node, &image, length, &exception);
	if (exception != NULL)
		return NULL;

	return image;
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
