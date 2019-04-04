/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_TOOLS_HINAWA_FW_UNIT_H__
#define __ALSA_TOOLS_HINAWA_FW_UNIT_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_UNIT	(hinawa_fw_unit_get_type())

#define HINAWA_FW_UNIT(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_FW_UNIT,	\
				    HinawaFwUnit))
#define HINAWA_IS_FW_UNIT(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_FW_UNIT))

#define HINAWA_FW_UNIT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_FW_UNIT,		\
				 HinawaFwUnitClass))
#define HINAWA_IS_FW_UNIT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_FW_UNIT))
#define HINAWA_FW_UNIT_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_FW_UNIT,		\
				   HinawaFwUnitClass))

typedef struct _HinawaFwUnit		HinawaFwUnit;
typedef struct _HinawaFwUnitClass	HinawaFwUnitClass;
typedef struct _HinawaFwUnitPrivate	HinawaFwUnitPrivate;

struct _HinawaFwUnit {
	GObject parent_instance;

	HinawaFwUnitPrivate *priv;
};

struct _HinawaFwUnitClass {
	GObjectClass parent_class;

	/**
	 * HinawaFwUnitClass::bus_update:
	 * @self: A #HinawaFwUnit
	 *
	 * When IEEE 1394 bus is updated, the ::bus-update handler is called.
	 * The handlers can read current generation in the bus via 'generation'
	 * property.
	 */
	void (*bus_update)(HinawaFwUnit *self);

	/**
	 * HinawaFwUnitClass::disconnected:
	 * @self: A #HinawaFwUnit
	 *
	 * When phicical FireWire devices are disconnected from IEEE 1394 bus,
	 * the #HinawaFwUnit becomes unlistening and the ::disconnected
	 * handler is called.
	 */
	void (*disconnected)(HinawaFwUnit *self);
};

GType hinawa_fw_unit_get_type(void) G_GNUC_CONST;

HinawaFwUnit *hinawa_fw_unit_new(void);

void hinawa_fw_unit_open(HinawaFwUnit *self, gchar *path, GError **exception);

const guint8 *hinawa_fw_unit_get_config_rom(HinawaFwUnit *self, guint *length);

void hinawa_fw_unit_listen(HinawaFwUnit *self, GError **exception);
void hinawa_fw_unit_unlisten(HinawaFwUnit *self);

G_END_DECLS

#endif
