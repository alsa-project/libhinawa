#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "hinawa_context.h"
#include "fw_unit.h"
#include "internal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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
	guint64 generation;

	unsigned int len;
	void *buf;
	FwUnitSource *src;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaFwUnit, hinawa_fw_unit, G_TYPE_OBJECT)
#define FW_UNIT_GET_PRIVATE(obj)					\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), 				\
				     HINAWA_TYPE_FW_UNIT, HinawaFwUnitPrivate))

/* This object has properties. */
enum fw_unit_prop_type {
	FW_UNIT_PROP_TYPE_GENERATION = 1,
	FW_UNIT_PROP_TYPE_COUNT,
};
static GParamSpec *fw_unit_props[FW_UNIT_PROP_TYPE_COUNT] = { NULL, };

/* This object has one signal. */
enum fw_unit_sig_type {
	FW_UNIT_SIG_TYPE_BUS_UPDATE = 0,
	FW_UNIT_SIG_TYPE_COUNT,
};
static guint fw_unit_sigs[FW_UNIT_SIG_TYPE_COUNT] = { 0 };

static void fw_unit_get_property(GObject *obj, guint id,
				 GValue *val, GParamSpec *spec)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);
	HinawaFwUnitPrivate *priv = FW_UNIT_GET_PRIVATE(self);

	switch (id) {
	case FW_UNIT_PROP_TYPE_GENERATION:
		g_value_set_uint64(val, priv->generation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_unit_set_property(GObject *obj, guint id,
				 const GValue *val, GParamSpec *spec)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);
	HinawaFwUnitPrivate *priv = FW_UNIT_GET_PRIVATE(self);

	switch (id) {
	case FW_UNIT_PROP_TYPE_GENERATION:
		priv->generation = g_value_get_uint64(val);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void fw_unit_dispose(GObject *obj)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);
	HinawaFwUnitPrivate *priv = FW_UNIT_GET_PRIVATE(self);

	hinawa_fw_unit_unlisten(self);

	close(priv->fd);

	G_OBJECT_CLASS(hinawa_fw_unit_parent_class)->dispose(obj);
}

static void fw_unit_finalize(GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_fw_unit_parent_class)->finalize(gobject);
}

static void hinawa_fw_unit_class_init(HinawaFwUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = fw_unit_get_property;
	gobject_class->set_property = fw_unit_set_property;
	gobject_class->dispose = fw_unit_dispose;
	gobject_class->finalize = fw_unit_finalize;

	fw_unit_props[FW_UNIT_PROP_TYPE_GENERATION] =
		g_param_spec_uint64("generation", "generation",
				    "current level of generation on this bus.",
				    0, ULONG_MAX, 0,
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
}

static void hinawa_fw_unit_init(HinawaFwUnit *self)
{
	self->priv = hinawa_fw_unit_get_instance_private(self);
}

/**
 * hinawa_fw_unit_new:
 * @path: A path to Linux FireWire charactor device
 * @exception: A #GError
 * 
 * Returns: An instance of #HinawaFwUnit
 */
HinawaFwUnit *hinawa_fw_unit_new(gchar *path, GError **exception)
{
	HinawaFwUnit *self;
	HinawaFwUnitPrivate *priv;
	int fd;
	struct fw_cdev_get_info info = {0};
	struct fw_cdev_event_bus_reset br = {0};

	fd = open(path, O_RDWR);
	if (fd < 0) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    errno, "%s", strerror(errno));
		return NULL;
	}

	self = g_object_new(HINAWA_TYPE_FW_UNIT, NULL);
	if (self == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}
	priv = FW_UNIT_GET_PRIVATE(self);

	info.version = 4;
	info.bus_reset = (guint64)&br;
	info.bus_reset_closure = (guint64)self;
	if (ioctl(fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    errno, "%s", strerror(errno));
		g_clear_object(&self);
		close(fd);
		return NULL;
	}

	priv->fd = fd;
	priv->generation = br.generation;

	return self;
}

/* Internal use only. */
void hinawa_fw_unit_ioctl(HinawaFwUnit *self, int req, void *args, int *err)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = FW_UNIT_GET_PRIVATE(self);

	*err = 0;
	if (ioctl(priv->fd, req, args) < 0)
		*err = errno;
}

static void handle_update(HinawaFwUnit *self,
			  struct fw_cdev_event_bus_reset *event)
{
	HinawaFwUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = FW_UNIT_GET_PRIVATE(self);

	priv->generation = event->generation;

	g_signal_emit(self, fw_unit_sigs[FW_UNIT_SIG_TYPE_BUS_UPDATE], 0,
		      NULL);
}

static gboolean prepare_src(GSource *src, gint *timeout)
{
	/* Set 2msec for poll(2) timeout. */
	*timeout = 2;

	/* This source is not ready, let's poll(2) */
	return FALSE;
}

static gboolean check_src(GSource *gsrc)
{
	FwUnitSource *src = (FwUnitSource *)gsrc;
	HinawaFwUnit *unit = src->unit;
	HinawaFwUnitPrivate *priv = FW_UNIT_GET_PRIVATE(unit);
	struct fw_cdev_event_common *common;
	int len;
	GIOCondition condition;

	if (unit == NULL)
		goto end;

	/* Let's process this source if any inputs are available. */
	condition = g_source_query_unix_fd((GSource *)src, src->tag);
	if (!(condition & G_IO_IN))
		goto end;

	len = read(priv->fd, priv->buf, priv->len);
	if (len <= 0)
		goto end;

	common = (struct fw_cdev_event_common *)priv->buf;

	if (HINAWA_IS_FW_UNIT(common->closure) &&
	    common->type == FW_CDEV_EVENT_BUS_RESET)
		handle_update(HINAWA_FW_UNIT(common->closure),
				(struct fw_cdev_event_bus_reset *)common);
	else if (HINAWA_IS_FW_REQ(common->closure) &&
		 common->type == FW_CDEV_EVENT_RESPONSE)
		hinawa_fw_req_handle_response(HINAWA_FW_REQ(common->closure),
				(struct fw_cdev_event_response *)common);
	else if (HINAWA_IS_FW_RESP(common->closure) &&
		 common->type == FW_CDEV_EVENT_REQUEST2)
		hinawa_fw_resp_handle_request(HINAWA_FW_RESP(common->closure),
				(struct fw_cdev_event_request2 *)common);
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

static void finalize_src(GSource *src)
{
	/* Do nothing paticular. */
	return;
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
		.finalize	= finalize_src,
	};
	HinawaFwUnitPrivate *priv;
	void *buf;
	GSource *src;

	g_return_if_fail(HINAWA_IS_FW_UNIT(self));
	priv = FW_UNIT_GET_PRIVATE(self);

	/*
	 * MEMO: allocate one page because we cannot assume the size of
	 * transaction frame.
	 */
	buf = g_malloc0(getpagesize());
	if (buf == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return;
	}

	src = g_source_new(&funcs, sizeof(FwUnitSource));
	if (src == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
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
	priv = FW_UNIT_GET_PRIVATE(self);

	if (priv->src == NULL)
		return;

	g_source_destroy((GSource *)priv->src);
	g_free(priv->src);
	priv->src = NULL;

	g_free(priv->buf);
	priv->buf = NULL;
	priv->len = 0;
}
