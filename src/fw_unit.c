#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "fw_unit.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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
G_DEFINE_TYPE_WITH_PRIVATE (HinawaFwUnit, hinawa_fw_unit, G_TYPE_OBJECT)

/* This object has properties. */
enum fw_unit_prop_type {
	FW_UNIT_PROP_TYPE_FD = 1,
	FW_UNIT_PROP_TYPE_GENERATION,
	FW_UNIT_PROP_TYPE_COUNT,
};
static GParamSpec *fw_unit_props[FW_UNIT_PROP_TYPE_COUNT] = { NULL, };

static void hinawa_fw_unit_get_property(GObject *obj, guint id,
					GValue *val, GParamSpec *spec)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);

	switch (id) {
	case FW_UNIT_PROP_TYPE_FD:
		g_value_set_int(val, self->priv->fd);
		break;
	case FW_UNIT_PROP_TYPE_GENERATION:
		g_value_set_uint64(val, self->priv->generation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, id, spec);
		break;
	}
}

static void hinawa_fw_unit_set_property(GObject *obj, guint id,
					const GValue *val, GParamSpec *spec)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);

	switch (id) {
	case FW_UNIT_PROP_TYPE_FD:
		self->priv->fd = g_value_get_int(val);
		break;
	case FW_UNIT_PROP_TYPE_GENERATION:
		self->priv->generation = g_value_get_uint64(val);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, id, spec);
		break;
	}
}

static void hinawa_fw_unit_dispose(GObject *obj)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(obj);

	close(self->priv->fd);
	self->priv->fd = 0;

	G_OBJECT_CLASS (hinawa_fw_unit_parent_class)->dispose(obj);
}

static void hinawa_fw_unit_finalize (GObject *gobject)
{
	HinawaFwUnit *self = HINAWA_FW_UNIT(gobject);

	G_OBJECT_CLASS(hinawa_fw_unit_parent_class)->finalize (gobject);
}

static void hinawa_fw_unit_class_init(HinawaFwUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->get_property = hinawa_fw_unit_get_property;
	gobject_class->set_property = hinawa_fw_unit_set_property;
	gobject_class->dispose = hinawa_fw_unit_dispose;
	gobject_class->finalize = hinawa_fw_unit_finalize;

	fw_unit_props[FW_UNIT_PROP_TYPE_FD] =
		g_param_spec_int("fd", "fd",
				 "a file descriptor for fw cdev.",
				 INT_MIN, INT_MAX, 0,
				 G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	fw_unit_props[FW_UNIT_PROP_TYPE_GENERATION] =
		g_param_spec_uint64("generation", "generation",
				    "current level of generation on this bus.",
				    0, ULONG_MAX, 0,
				    G_PARAM_READWRITE);

	g_object_class_install_properties(gobject_class,
					  FW_UNIT_PROP_TYPE_COUNT,
					  fw_unit_props);
}

static void
hinawa_fw_unit_init(HinawaFwUnit *self)
{
	self->priv = hinawa_fw_unit_get_instance_private(self);
}

HinawaFwUnit *hinawa_fw_unit_new(gchar *path, GError **exception)
{
	HinawaFwUnit *self;
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

	self->priv->fd = fd;
	self->priv->generation = br.generation;

	return self;
}

static void handling_update(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_bus_reset *event = &ev->bus_reset;
	HinawaFwUnit *unit = (HinawaFwUnit *)event->closure;

	if (unit == NULL)
		return;

	unit->priv->generation = event->generation;
}

static gboolean preparing_src(GSource *src, gint *timeout)
{
	/* Set 2msec for poll(2) timeout. */
	*timeout = 2;

	/* This source is not ready, let's poll(2) */
	return FALSE;
}

static gboolean checking_src(GSource *gsrc)
{
	FwUnitSource *src = (FwUnitSource *)gsrc;
	HinawaFwUnit *unit = src->unit;
	struct fw_cdev_event_common *common;
	int len;
	GIOCondition condition;

	HinawaFwUnitHandle handles[] = {
		[FW_CDEV_EVENT_BUS_RESET] = handling_update,
		[FW_CDEV_EVENT_RESPONSE]  = hinawa_fw_req_handle_response,
		[FW_CDEV_EVENT_REQUEST2]  = hinawa_fw_resp_handle_request
	};

	if (unit == NULL)
		goto end;

	/* Let's process this source if any inputs are available. */
	condition = g_source_query_unix_fd((GSource *)src, src->tag);
	if (!(condition & G_IO_IN))
		goto end;

	len = read(unit->priv->fd, unit->priv->buf, unit->priv->len);
	if (len <= 0)
		goto end;

	common = (struct fw_cdev_event_common *)unit->priv->buf;

	if (common->type != FW_CDEV_EVENT_BUS_RESET &&
	    common->type != FW_CDEV_EVENT_RESPONSE &&
	    common->type != FW_CDEV_EVENT_REQUEST2)
		goto end;

	handles[common->type](unit->priv->fd,
			      (union fw_cdev_event *)unit->priv->buf);
end:
	/* Don't go to dispatch, then continue to process this source. */
	return FALSE;
}

static gboolean dispatching_src(GSource *src, GSourceFunc callback,
				gpointer user_data)
{
	/* Just be sure to continue to process this source. */
	return TRUE;
}

static void finalizing_src(GSource *src)
{
	/* Do nothing paticular. */
	return;
}

void hinawa_fw_unit_listen(HinawaFwUnit *self, GError **exception)
{
	static GSourceFuncs funcs = {
		.prepare	= preparing_src,
		.check		= checking_src,
		.dispatch	= dispatching_src,
		.finalize	= finalizing_src,
	};
	void *buf;
	GSource *src;
	GMainContext *ctx;

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
	self->priv->src = (FwUnitSource *)src;
	self->priv->buf = buf;
	self->priv->len = getpagesize();

	ctx = g_main_context_default();
	/* NOTE: The returned ID is never used. */
	g_source_attach(src, ctx);
	((FwUnitSource *)src)->tag =
			g_source_add_unix_fd(src, self->priv->fd, G_IO_IN);
}

void hinawa_fw_unit_unlisten(HinawaFwUnit *self)
{
	g_source_destroy((GSource *)self->priv->src);
	close(self->priv->fd);

	g_free(self->priv->buf);
	self->priv->buf = NULL;
	self->priv->len = 0;
}
