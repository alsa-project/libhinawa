// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_FW_NODE_H__
#define __ALSA_HINAWA_FW_NODE_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_NODE	(hinawa_fw_node_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaFwNode, hinawa_fw_node, HINAWA, FW_NODE, GObject);

#define HINAWA_FW_NODE_ERROR	hinawa_fw_node_error_quark()

GQuark hinawa_fw_node_error_quark();

struct _HinawaFwNodeClass {
	GObjectClass parent_class;

	/**
	 * HinawaFwNodeClass::bus_update:
	 * @self: A #HinawaFwNode.
	 *
	 * When IEEE 1394 bus is updated, the #HinawaFwNodeClass::bus-update handler is called.
	 * The handlers can read current generation in the bus via 'generation'
	 * property.
	 *
	 * Since: 1.4.
	 */
	void (*bus_update)(HinawaFwNode *self);

	/**
	 * HinawaFwNodeClass::disconnected:
	 * @self: A #HinawaFwNode
	 *
	 * When phicical FireWire devices are disconnected from IEEE 1394 bus,
	 * the #HinawaFwNodeClass::disconnected handler is called.
	 *
	 * Since: 1.4.
	 */
	void (*disconnected)(HinawaFwNode *self);

};

HinawaFwNode *hinawa_fw_node_new(void);

void hinawa_fw_node_open(HinawaFwNode *self, const gchar *path,
			 GError **exception);

void hinawa_fw_node_get_config_rom(HinawaFwNode *self, const guint8 **image,
				   gsize *length, GError **exception);

void hinawa_fw_node_create_source(HinawaFwNode *self, GSource **gsrc,
				  GError **exception);

G_END_DECLS

#endif
