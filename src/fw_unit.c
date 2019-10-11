/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "internal.h"

/* For error handling. */
G_DEFINE_QUARK("HinawaFwUnit", hinawa_fw_unit)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_fw_unit_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

/**
 * SECTION:fw_unit
 * @Title: HinawaFwUnit
 * @Short_description: An event listener for FireWire unit
 *
 * A #HinawaFwUnit is an event listener for a certain FireWire unit.
 * This class is an application of Linux FireWire subsystem.
 * All of operations utilize ioctl(2) with subsystem specific request commands.
 */

struct _HinawaFwUnitPrivate {
	HinawaFwNode *node;
	GSource *src;
	int fd;

	GSource *unit_src;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwUnit, hinawa_fw_unit, G_TYPE_OBJECT)

typedef struct {
	GSource src;
	HinawaFwUnit *unit;
	gpointer tag;
	unsigned int len;
	void *buf;
} FwUnitSource;

/* This object has properties. */
enum fw_unit_prop_type {
	FW_UNIT_PROP_TYPE_NODE_ID = 1,
	FW_UNIT_PROP_TYPE_LOCAL_NODE_ID,
	FW_UNIT_PROP_TYPE_BUS_MANAGER_NODE_ID,
	FW_UNIT_PROP_TYPE_IR_MANAGER_NODE_ID,
	FW_UNIT_PROP_TYPE_ROOT_NODE_ID,
	FW_UNIT_PROP_TYPE_GENERATION,
	FW_UNIT_PROP_TYPE_LISTENING,
	FW_UNIT_PROP_TYPE_COUNT,
};
static GParamSpec *fw_unit_props[FW_UNIT_PROP_TYPE_COUNT] = { NULL, };

/* This object has one signal. */
enum fw_unit_sig_type {
	FW_UNIT_SIG_TYPE_BUS_UPDATE = 0,
	FW_UNIT_SIG_TYPE_DISCONNECTED,
	FW_UNIT_SIG_TYPE_COUNT,
};
static guint fw_unit_sigs[FW_UNIT_SIG_TYPE_COUNT] = { 0 };

static void fw_unit_get_property(GObject *obj, guint id,
				 GValue *val, GParamSpec *spec)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);
	HinawaFwUnitPrivate *priv = hinawa_fw_unit_get_instance_private(self);
	GObject *node = G_OBJECT(priv->node);

	switch (id) {
	case FW_UNIT_PROP_TYPE_NODE_ID:
	case FW_UNIT_PROP_TYPE_LOCAL_NODE_ID:
	case FW_UNIT_PROP_TYPE_BUS_MANAGER_NODE_ID:
	case FW_UNIT_PROP_TYPE_IR_MANAGER_NODE_ID:
	case FW_UNIT_PROP_TYPE_ROOT_NODE_ID:
	case FW_UNIT_PROP_TYPE_GENERATION:
		g_object_get_property(node, fw_unit_props[id]->name, val);
		break;
	case FW_UNIT_PROP_TYPE_LISTENING:
		g_value_set_boolean(val, priv->src != NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_unit_set_property(GObject *obj, guint id,
				 const GValue *val, GParamSpec *spec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
}

static void fw_unit_finalize(GObject *obj)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);
	HinawaFwUnitPrivate *priv = hinawa_fw_unit_get_instance_private(self);

	hinawa_fw_unit_unlisten(self);

	close(priv->fd);

	g_object_unref(priv->node);

	G_OBJECT_CLASS(hinawa_fw_unit_parent_class)->finalize(obj);
}

static void hinawa_fw_unit_class_init(HinawaFwUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_unit_get_property;
	gobject_class->set_property = fw_unit_set_property;
	gobject_class->finalize = fw_unit_finalize;

	fw_unit_props[FW_UNIT_PROP_TYPE_NODE_ID] =
		g_param_spec_ulong("node-id", "node-id",
				   "Node-ID of this unit at this generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_unit_props[FW_UNIT_PROP_TYPE_LOCAL_NODE_ID] =
		g_param_spec_ulong("local-node-id", "local-node-id",
				   "Node-ID for a unit which this unit use to "
				   "communicate to the other units on the bus "
				   "at this generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_unit_props[FW_UNIT_PROP_TYPE_BUS_MANAGER_NODE_ID] =
		g_param_spec_ulong("bus-manager-node-id", "bus-manager-node-id",
				   "Node-ID for bus manager on the bus at this "
				   "generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_unit_props[FW_UNIT_PROP_TYPE_IR_MANAGER_NODE_ID] =
		g_param_spec_ulong("ir-manager-node-id", "ir-manager-node-id",
				   "Node-ID for isochronous resource manager "
				   "on the bus at this generation",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_unit_props[FW_UNIT_PROP_TYPE_ROOT_NODE_ID] =
		g_param_spec_ulong("root-node-id", "root-node-id",
				   "Node-ID for root of bus topology at this "
				   "generation.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_unit_props[FW_UNIT_PROP_TYPE_GENERATION] =
		g_param_spec_ulong("generation", "generation",
				   "current level of generation on this bus.",
				   0, ULONG_MAX, 0,
				   G_PARAM_READABLE);
	fw_unit_props[FW_UNIT_PROP_TYPE_LISTENING] =
		g_param_spec_boolean("listening", "listening",
				     "Whether this device is under listening.",
				     FALSE,
				     G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  FW_UNIT_PROP_TYPE_COUNT,
					  fw_unit_props);

	/**
	 * HinawaFwUnit::bus-update:
	 * @self: A #HinawaFwUnit
	 *
	 * When IEEE 1394 bus is updated, the ::bus-update signal is generated.
	 * Handlers can read current generation in the bus via 'generation'
	 * property.
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
	 * When phicical FireWire devices are disconnected from IEEE 1394 bus,
	 * the #HinawaFwUnit becomes unlostening and emits this signal.
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
	struct fw_cdev_get_info info = {0};
	HinawaFwUnitPrivate *priv;
	int fd;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		raise(exception, errno);
		return;
	}
	priv->fd = fd;

	info.version = 4;
	if (ioctl(priv->fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		raise(exception, errno);
		close(priv->fd);
		priv->fd = -1;
		return;
	}

	hinawa_fw_node_open(priv->node, path, exception);
	if (*exception != NULL) {
		close(priv->fd);
		priv->fd = -1;
	}

	g_signal_connect(G_OBJECT(priv->node), "bus_update",
			 G_CALLBACK(handle_bus_update), self);
	g_signal_connect(G_OBJECT(priv->node), "disconnected",
			 G_CALLBACK(handle_disconnected), self);
}

/**
 * hinawa_fw_unit_get_config_rom:
 * @self: A #HinawaFwUnit
 * @length: (out) (optional): the number of bytes consists of the config rom
 *
 * Get cached content of configuration ROM.
 *
 * Returns: (element-type guint8) (array length=length) (transfer none): config rom image
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

/* Internal use only. */
void hinawa_fw_unit_ioctl(HinawaFwUnit *self, unsigned long req, void *args,
			  int *err)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	*err = 0;
	if (ioctl(priv->fd, req, args) < 0)
		*err = errno;
}

static gboolean prepare_src(GSource *src, gint *timeout)
{
	// Use blocking poll(2) to save CPU usage.
	*timeout = -1;

	/* This source is not ready, let's poll(2) */
	return FALSE;
}

static gboolean check_src(GSource *gsrc)
{
	FwUnitSource *src = (FwUnitSource *)gsrc;
	GIOCondition condition;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR) {
		HinawaFwUnit *unit = src->unit;

		if (unit != NULL)
			hinawa_fw_unit_unlisten(unit);
	}

	// Don't go to dispatch if nothing available.
	return !!(condition & G_IO_IN);
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	FwUnitSource *src = (FwUnitSource *)gsrc;
	HinawaFwUnit *unit = src->unit;
	HinawaFwUnitPrivate *priv;
	struct fw_cdev_event_common *common;
	int len;

	if (unit == NULL)
		goto end;
	priv = hinawa_fw_unit_get_instance_private(unit);

	len = read(priv->fd, src->buf, src->len);
	if (len <= 0)
		goto end;

	common = (struct fw_cdev_event_common *)src->buf;

	if (HINAWA_IS_FW_RESP(common->closure) &&
		 common->type == FW_CDEV_EVENT_REQUEST2)
		hinawa_fw_resp_handle_request(HINAWA_FW_RESP(common->closure),
				(struct fw_cdev_event_request2 *)common);
	else if (HINAWA_IS_FW_REQ(common->closure) &&
		 common->type == FW_CDEV_EVENT_RESPONSE)
		hinawa_fw_req_handle_response(HINAWA_FW_REQ(common->closure),
				(struct fw_cdev_event_response *)common);
end:
	/* Just be sure to continue to process this source. */
	return G_SOURCE_CONTINUE;
}

static void finalize_src(GSource *gsrc)
{
	FwUnitSource *src = (FwUnitSource *)gsrc;

	g_free(src->buf);
}

static void fw_unit_create_source(HinawaFwUnit *self, GSource **gsrc,
				  GError **exception)
{
	static GSourceFuncs funcs = {
		.prepare	= prepare_src,
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	HinawaFwUnitPrivate *priv;
	FwUnitSource *src;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	*gsrc = g_source_new(&funcs, sizeof(FwUnitSource));

	g_source_set_name(*gsrc, "HinawaFwUnit");
	g_source_set_priority(*gsrc, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*gsrc, TRUE);

	// MEMO: allocate one page because we cannot assume the size of
	// transaction frame.
	src = (FwUnitSource *)(*gsrc);
	src->len = sysconf(_SC_PAGESIZE);
	src->buf = g_malloc0(src->len);
	if (src->buf == NULL) {
		raise(exception, ENOMEM);
		g_source_unref(*gsrc);
		return;
	}

	src->unit = self;
	src->tag = g_source_add_unix_fd(*gsrc, priv->fd, G_IO_IN);
}

/**
 * hinawa_fw_unit_listen:
 * @self: A #HinawaFwUnit
 * @exception: A #GError
 *
 * Start to listen to any events from the unit.
 */
void hinawa_fw_unit_listen(HinawaFwUnit *self, GError **exception)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	if (priv->unit_src == NULL) {
		fw_unit_create_source(self, &priv->unit_src, exception);
		if (*exception != NULL)
			return;

		hinawa_context_add_src(priv->unit_src, exception);
		if (*exception != NULL) {
			hinawa_fw_unit_unlisten(self);
			return;
		}
	}

	if (priv->src == NULL) {
		hinawa_fw_node_create_source(priv->node, &priv->src, exception);
		if (*exception != NULL) {
			hinawa_fw_unit_unlisten(self);
			return;
		}

		hinawa_context_add_src(priv->src, exception);
		if (*exception != NULL) {
			hinawa_fw_unit_unlisten(self);
			return;
		}
	}
}

/**
 * hinawa_fw_unit_unlisten:
 * @self: A #HinawaFwUnit
 *
 * Stop to listen to any events from the unit.
 */
void hinawa_fw_unit_unlisten(HinawaFwUnit *self)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	if (priv->unit_src != NULL) {
		hinawa_context_remove_src(priv->unit_src);
		g_source_unref(priv->unit_src);
		priv->unit_src = NULL;
	}

	if (priv->src != NULL) {
		hinawa_context_remove_src(priv->src);
		g_source_unref(priv->src);
		priv->src = NULL;
	}
}
