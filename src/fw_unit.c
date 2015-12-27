#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "internal.h"
#include "hinawa_context.h"

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

typedef struct {
	GSource src;
	HinawaFwUnit *unit;
	gpointer tag;
} FwUnitSource;

struct _HinawaFwUnitPrivate {
	int fd;
	struct fw_cdev_get_info info;

	GMutex mutex;
	guint32 *config_rom;
	struct fw_cdev_event_bus_reset generation;

	unsigned int len;
	void *buf;
	FwUnitSource *src;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwUnit, hinawa_fw_unit, G_TYPE_OBJECT)

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

	switch (id) {
	case FW_UNIT_PROP_TYPE_NODE_ID:
		g_mutex_lock(&priv->mutex);
		g_value_set_ulong(val, priv->generation.node_id);
		g_mutex_unlock(&priv->mutex);
		break;
	case FW_UNIT_PROP_TYPE_LOCAL_NODE_ID:
		g_mutex_lock(&priv->mutex);
		g_value_set_ulong(val, priv->generation.local_node_id);
		g_mutex_unlock(&priv->mutex);
		break;
	case FW_UNIT_PROP_TYPE_BUS_MANAGER_NODE_ID:
		g_mutex_lock(&priv->mutex);
		g_value_set_ulong(val, priv->generation.bm_node_id);
		g_mutex_unlock(&priv->mutex);
		break;
	case FW_UNIT_PROP_TYPE_IR_MANAGER_NODE_ID:
		g_mutex_lock(&priv->mutex);
		g_value_set_ulong(val, priv->generation.irm_node_id);
		g_mutex_unlock(&priv->mutex);
		break;
	case FW_UNIT_PROP_TYPE_ROOT_NODE_ID:
		g_mutex_lock(&priv->mutex);
		g_value_set_ulong(val, priv->generation.root_node_id);
		g_mutex_unlock(&priv->mutex);
		break;
	case FW_UNIT_PROP_TYPE_GENERATION:
		g_mutex_lock(&priv->mutex);
		g_value_set_ulong(val, priv->generation.generation);
		g_mutex_unlock(&priv->mutex);
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

	g_free(priv->config_rom);
	g_mutex_clear(&priv->mutex);

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
			     0,
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
			     0,
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0, G_TYPE_NONE);
}

static void hinawa_fw_unit_init(HinawaFwUnit *self)
{
	HinawaFwUnitPrivate *priv= hinawa_fw_unit_get_instance_private(self);
	g_mutex_init(&priv->mutex);
}

/**
 * hinawa_fw_unit_open:
 * @self: A #HinawaFwUnit
 * @path: A path to Linux FireWire character device
 * @exception: A #GError
 */
void hinawa_fw_unit_open(HinawaFwUnit *self, gchar *path, GError **exception)
{
	HinawaFwUnitPrivate *priv;
	int fd;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		raise(exception, errno);
		return;
	}

	g_mutex_lock(&priv->mutex);

	/* Duplicate generation parameters in userspace. */
	priv->info.version = 4;
	priv->info.bus_reset = (guint64)&priv->generation;
	priv->info.bus_reset_closure = (guint64)self;
	if (ioctl(fd, FW_CDEV_IOC_GET_INFO, &priv->info) < 0) {
		raise(exception, errno);
		close(fd);
		goto end;
	}

	/* Duplicate contents of Config ROM in userspace. */
	priv->config_rom = g_malloc0(priv->info.rom_length);
	if (priv->config_rom == NULL) {
		raise(exception, ENOMEM);
		close(fd);
		goto end;
	}
	priv->info.rom = (__u64)priv->config_rom;

	if (ioctl(fd, FW_CDEV_IOC_GET_INFO, &priv->info) < 0) {
		raise(exception, errno);
		close(fd);
		goto end;
	}

	priv->fd = fd;
end:
	g_mutex_unlock(&priv->mutex);
}

/**
 * hinawa_fw_unit_get_config_rom:
 * @self: A #HinawaFwUnit
 * @quads: (out) (optional): the number of quadlet consists of the config rom
 *
 * Returns: (element-type guint32) (array length=quads) (transfer none): config rom image
 */
const guint32 *hinawa_fw_unit_get_config_rom(HinawaFwUnit *self, guint *quads)
{
	HinawaFwUnitPrivate *priv;

	g_return_val_if_fail(HINAWA_IS_FW_UNIT(self), NULL);
	priv = hinawa_fw_unit_get_instance_private(self);

	if (quads != NULL)
		*quads = priv->info.rom_length / 4;

	return priv->config_rom;
}

/* Internal use only. */
void hinawa_fw_unit_ioctl(HinawaFwUnit *self, int req, void *args, int *err)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	*err = 0;
	if (ioctl(priv->fd, req, args) < 0)
		*err = errno;
}

static void handle_update(HinawaFwUnit *self,
			  struct fw_cdev_event_bus_reset *event)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	g_mutex_lock(&priv->mutex);
	memcpy(&priv->generation, event, sizeof(struct fw_cdev_get_info));
	g_mutex_unlock(&priv->mutex);

	g_signal_emit(self, fw_unit_sigs[FW_UNIT_SIG_TYPE_BUS_UPDATE], 0, NULL);
}

static gboolean prepare_src(GSource *src, gint *timeout)
{
	/* Use blocking poll(2) to save CPU usage. */
	*timeout = -1;

	/* This source is not ready, let's poll(2) */
	return FALSE;
}

static gboolean check_src(GSource *gsrc)
{
	FwUnitSource *src = (FwUnitSource *)gsrc;
	HinawaFwUnit *unit = src->unit;
	HinawaFwUnitPrivate *priv = hinawa_fw_unit_get_instance_private(unit);
	struct fw_cdev_event_common *common;
	int len;
	GIOCondition condition;

	if (unit == NULL)
		goto end;

	condition = g_source_query_unix_fd((GSource *)src, src->tag);
	if (condition & G_IO_ERR) {
		hinawa_fw_unit_unlisten(unit);
		g_signal_emit(unit,
			      fw_unit_sigs[FW_UNIT_SIG_TYPE_DISCONNECTED], 0);
		goto end;
	} else if (!(condition & G_IO_IN)) {
		goto end;
	}

	len = read(priv->fd, priv->buf, priv->len);
	if (len <= 0)
		goto end;

	common = (struct fw_cdev_event_common *)priv->buf;

	if (HINAWA_IS_FW_UNIT(common->closure) &&
	    common->type == FW_CDEV_EVENT_BUS_RESET)
		handle_update(HINAWA_FW_UNIT(common->closure),
				(struct fw_cdev_event_bus_reset *)common);
	else if (HINAWA_IS_FW_RESP(common->closure) &&
		 common->type == FW_CDEV_EVENT_REQUEST2)
		hinawa_fw_resp_handle_request(HINAWA_FW_RESP(common->closure),
				(struct fw_cdev_event_request2 *)common);
	else if (HINAWA_IS_FW_REQ(common->closure) &&
		 common->type == FW_CDEV_EVENT_RESPONSE)
		hinawa_fw_req_handle_response(HINAWA_FW_REQ(common->closure),
				(struct fw_cdev_event_response *)common);
end:
	/* Don't go to dispatch, then continue to process this source. */
	return FALSE;
}

static gboolean dispatch_src(GSource *src, GSourceFunc callback,
			     gpointer user_data)
{
	/* Just be sure to continue to process this source. */
	return TRUE;
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
	static GSourceFuncs funcs = {
		.prepare	= prepare_src,
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= NULL,
	};
	HinawaFwUnitPrivate *priv;
	void *buf;
	GSource *src;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = hinawa_fw_unit_get_instance_private(self);

	/*
	 * MEMO: allocate one page because we cannot assume the size of
	 * transaction frame.
	 */
	buf = g_malloc0(getpagesize());
	if (buf == NULL) {
		raise(exception, ENOMEM);
		return;
	}

	src = g_source_new(&funcs, sizeof(FwUnitSource));
	if (src == NULL) {
		raise(exception, ENOMEM);
		g_free(buf);
		return;
	}

	g_source_set_name(src, "HinawaFwUnit");
	g_source_set_priority(src, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(src, TRUE);

	((FwUnitSource *)src)->unit = self;
	priv->src = (FwUnitSource *)src;
	priv->buf = buf;
	priv->len = getpagesize();

	((FwUnitSource *)src)->tag =
		hinawa_context_add_src(src, priv->fd, G_IO_IN, exception);
	if (*exception != NULL) {
		g_free(buf);
		g_source_destroy(src);
		priv->buf = NULL;
		priv->len = 0;
		priv->src = NULL;
		return;
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

	if (priv->src == NULL)
		return;

	g_source_destroy((GSource *)priv->src);
	g_free(priv->src);
	priv->src = NULL;

	g_free(priv->buf);
	priv->buf = NULL;
	priv->len = 0;
}
