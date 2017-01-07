#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "internal.h"
#include "hinawa_context.h"

/**
 * SECTION:snd_unit
 * @Title: HinawaSndUnit
 * @Short_description: An event listener for ALSA FireWire sound devices
 *
 * This class is an application of ALSA FireWire stack. Any functionality which
 * ALSA drivers in the stack can be available.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaSndUnit", hinawa_snd_unit)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_unit_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

typedef struct {
	GSource src;
	HinawaSndUnit *unit;
	gpointer tag;
} SndUnitSource;

struct _HinawaSndUnitPrivate {
	int fd;
	struct snd_firewire_get_info info;

	gboolean streaming;

	void *buf;
	unsigned int len;
	SndUnitSource *src;

	HinawaFwReq *req;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndUnit, hinawa_snd_unit, HINAWA_TYPE_FW_UNIT)

enum snd_unit_prop_type {
	SND_UNIT_PROP_TYPE_FW_TYPE = 1,
	SND_UNIT_PROP_TYPE_CARD,
	SND_UNIT_PROP_TYPE_DEVICE,
	SND_UNIT_PROP_TYPE_GUID,
	SND_UNIT_PROP_TYPE_STREAMING,
	SND_UNIT_PROP_TYPE_LISTENING,
	SND_UNIT_PROP_TYPE_COUNT,
};
static GParamSpec *snd_unit_props[SND_UNIT_PROP_TYPE_COUNT] = { NULL, };

/* This object has one signal. */
enum snd_unit_sig_type {
	SND_UNIT_SIG_TYPE_LOCK_STATUS = 0,
	SND_UNIT_SIG_TYPE_COUNT,
};
static guint snd_unit_sigs[SND_UNIT_SIG_TYPE_COUNT] = { 0 };

static void snd_unit_get_property(GObject *obj, guint id,
				  GValue *val, GParamSpec *spec)
{
	HinawaSndUnit *self = HINAWA_SND_UNIT(obj);
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);

	switch (id) {
	case SND_UNIT_PROP_TYPE_FW_TYPE:
		g_value_set_int(val, priv->info.type);
		break;
	case SND_UNIT_PROP_TYPE_CARD:
		g_value_set_int(val, priv->info.card);
		break;
	case SND_UNIT_PROP_TYPE_DEVICE:
		g_value_set_string(val, (const gchar *)priv->info.device_name);
		break;
	case SND_UNIT_PROP_TYPE_GUID:
		g_value_set_uint64(val,
				GUINT64_FROM_BE(*((guint64 *)priv->info.guid)));
		break;
	case SND_UNIT_PROP_TYPE_STREAMING:
		g_value_set_boolean(val, priv->streaming);
		break;
	case SND_UNIT_PROP_TYPE_LISTENING:
		g_value_set_boolean(val, priv->src != NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void snd_unit_set_property(GObject *obj, guint id,
				  const GValue *val, GParamSpec *spec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
}

static void snd_unit_finalize(GObject *obj)
{
	HinawaSndUnit *self = HINAWA_SND_UNIT(obj);
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);

	hinawa_snd_unit_unlisten(self);

	close(priv->fd);
	g_clear_object(&priv->req);

	G_OBJECT_CLASS(hinawa_snd_unit_parent_class)->finalize(obj);
}

static void hinawa_snd_unit_class_init(HinawaSndUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = snd_unit_get_property;
	gobject_class->set_property = snd_unit_set_property;
	gobject_class->finalize = snd_unit_finalize;

	snd_unit_props[SND_UNIT_PROP_TYPE_FW_TYPE] =
		g_param_spec_int("type", "type",
				 "The value of SNDRV_FIREWIRE_TYPE_XXX",
				 0, INT_MAX,
				 0,
				 G_PARAM_READABLE);
	snd_unit_props[SND_UNIT_PROP_TYPE_CARD] =
		g_param_spec_int("card", "card",
				 "A numerical ID for ALSA sound card",
				 0, INT_MAX,
				 0,
				 G_PARAM_READABLE);
	snd_unit_props[SND_UNIT_PROP_TYPE_DEVICE] =
		g_param_spec_string("device", "device",
				    "A name of special file as FireWire unit.",
				    NULL,
				    G_PARAM_READABLE);
	snd_unit_props[SND_UNIT_PROP_TYPE_STREAMING] =
		g_param_spec_boolean("streaming", "streaming",
				     "Whether this device is streaming or not",
				     FALSE,
				     G_PARAM_READABLE);
	snd_unit_props[SND_UNIT_PROP_TYPE_GUID] =
		g_param_spec_uint64("guid", "guid",
				    "Global unique ID for this firewire unit.",
				    0, ULONG_MAX, 0,
				    G_PARAM_READABLE);
	snd_unit_props[SND_UNIT_PROP_TYPE_LISTENING] =
		g_param_spec_boolean("listening", "listening",
				     "Whether this device is under listening.",
				     FALSE,
				     G_PARAM_READABLE);

	g_object_class_install_properties(gobject_class,
					  SND_UNIT_PROP_TYPE_COUNT,
					  snd_unit_props);

	/**
	 * HinawaSndUnit::lock-status:
	 * @self: A #HinawaSndUnit
	 * @state: %TRUE when locked, %FALSE when unlocked.
	 *
	 * When ALSA kernel-streaming status is changed, this ::lock-status
	 * signal is generated.
	 */
	snd_unit_sigs[SND_UNIT_SIG_TYPE_LOCK_STATUS] =
		g_signal_new("lock-status",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL, NULL,
			     g_cclosure_marshal_VOID__BOOLEAN,
			     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void hinawa_snd_unit_init(HinawaSndUnit *self)
{
	return;
}

/**
 * hinawa_snd_unit_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for FireWire sound devices.
 */
void hinawa_snd_unit_open(HinawaSndUnit *self, gchar *path, GError **exception)
{
	HinawaSndUnitPrivate *priv;
	char fw_cdev[32];

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	priv->fd = open(path, O_RDWR);
	if (priv->fd < 0) {
		raise(exception, errno);
		goto end;
	}

	/* Get FireWire sound device information. */
	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_GET_INFO, &priv->info) < 0) {
		raise(exception, errno);
		goto end;
	}

	snprintf(fw_cdev, sizeof(fw_cdev), "/dev/%s", priv->info.device_name);
	hinawa_fw_unit_open(&self->parent_instance, fw_cdev, exception);
	if (*exception != NULL)
		goto end;

	priv->req = g_object_new(HINAWA_TYPE_FW_REQ, "timeout", 40, NULL);
end:
	if (*exception != NULL)
		close(priv->fd);
}

/**
 * hinawa_snd_unit_lock:
 * @self: A #HinawaSndUnit
 * @exception: A #GError
 *
 * Disallow ALSA to start kernel-streaming.
 */
void hinawa_snd_unit_lock(HinawaSndUnit *self, GError **exception)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_LOCK, NULL) < 0)
		raise(exception, errno);
}

/**
 * hinawa_snd_unit_unlock:
 * @self: A #HinawaSndUnit
 * @exception: A #GError
 *
 * Allow ALSA to start kernel-streaming.
 */
void hinawa_snd_unit_unlock(HinawaSndUnit *self, GError **exception)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL) < 0)
		raise(exception, errno);
}

/**
 * hinawa_snd_unit_read_transact:
 * @self: A #HinawaSndUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint32) (array) (out caller-allocates): a 32bit array
 * @len: the bytes to read
 * @exception: A #GError
 *
 * Execute read transaction to the given unit.
 */
void hinawa_snd_unit_read_transact(HinawaSndUnit *self,
				   guint64 addr, GArray *frame, guint len,
				   GError **exception)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	hinawa_fw_req_read(priv->req, &self->parent_instance, addr, frame, len,
			   exception);
}

/**
 * hinawa_snd_unit_write_transact:
 * @self: A #HinawaSndUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint32) (array) (in): a 32bit array
 * @exception: A #GError
 *
 * Execute write transactions to the given unit.
 */
void hinawa_snd_unit_write_transact(HinawaSndUnit *self,
				    guint64 addr, GArray *frame,
				    GError **exception)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	hinawa_fw_req_write(priv->req, &self->parent_instance, addr, frame,
			    exception);
}

/**
 * hinawa_snd_unit_lock_transact:
 * @self: A #HinawaSndUnit
 * @addr: A destination address of target device
 * @frame: (element-type guint32) (array) (inout): a 32bit array
 * @exception: A #GError
 *
 * Execute lock transaction to the given unit.
 */
void hinawa_snd_unit_lock_transact(HinawaSndUnit *self,
				   guint64 addr, GArray **frame,
				   GError **exception)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	hinawa_fw_req_lock(priv->req, &self->parent_instance, addr, frame,
			   exception);
}

/* For internal use. */
void hinawa_snd_unit_write(HinawaSndUnit *self,
			   const void *buf, unsigned int length,
			   GError **exception)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	if (write(priv->fd, buf, length) != length)
		raise(exception, errno);
}

static void handle_lock_event(HinawaSndUnit *self,
			      void *buf, unsigned int length)
{
	struct snd_firewire_event_lock_status *event =
			(struct snd_firewire_event_lock_status *)buf;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));

	g_signal_emit(self, snd_unit_sigs[SND_UNIT_SIG_TYPE_LOCK_STATUS], 0,
		      event->status);
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
	SndUnitSource *src = (SndUnitSource *)gsrc;
	HinawaSndUnit *unit = src->unit;
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(unit);
	GIOCondition condition;
	GValue val = G_VALUE_INIT;

	struct snd_firewire_event_common *common;
	int len;

	if (unit == NULL)
		goto end;

	condition = g_source_query_unix_fd((GSource *)src, src->tag);
	if (condition & G_IO_ERR) {
		/* For emitting one signal. */
		g_value_init(&val, G_TYPE_BOOLEAN);
		g_object_get_property(G_OBJECT(&unit->parent_instance),
				      "listening", &val);
		hinawa_snd_unit_unlisten(unit);
		if (g_value_get_boolean(&val))
			g_signal_emit_by_name(&unit->parent_instance,
					      "disconnected", NULL);
		goto end;
	/* The other events never occur. */
	} else if (!(condition & G_IO_IN)) {
		goto end;
	}

	len = read(priv->fd, priv->buf, priv->len);
	if (len <= 0)
		goto end;

	common = (struct snd_firewire_event_common *)priv->buf;

	if (common->type == SNDRV_FIREWIRE_EVENT_LOCK_STATUS)
		handle_lock_event(unit, priv->buf, len);
#if HAVE_SND_DICE
	else if (HINAWA_IS_SND_DICE(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_DICE_NOTIFICATION)
		hinawa_snd_dice_handle_notification(HINAWA_SND_DICE(unit),
						    priv->buf, len);
#endif
#if HAVE_SND_EFW
	else if (HINAWA_IS_SND_EFW(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_EFW_RESPONSE)
		hinawa_snd_efw_handle_response(HINAWA_SND_EFW(unit),
					       priv->buf, len);
#endif
#if HAVE_SND_DG00X
	else if (HINAWA_IS_SND_DG00X(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_DIGI00X_MESSAGE)
		hinawa_snd_dg00x_handle_msg(HINAWA_SND_DG00X(unit),
					    priv->buf, len);
#endif
#if HAVE_SND_MOTU
	else if (HINAWA_IS_SND_MOTU(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_MOTU_NOTIFICATION)
		hinawa_snd_motu_handle_notification(HINAWA_SND_MOTU(unit),
						    priv->buf, len);
#endif
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
 * hinawa_snd_unit_listen:
 * @self: A #HinawaSndUnit
 * @exception: A #GError
 *
 * Start listening to events.
 */
void hinawa_snd_unit_listen(HinawaSndUnit *self, GError **exception)
{
	static GSourceFuncs funcs = {
		.prepare	= prepare_src,
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= NULL,
	};
	HinawaSndUnitPrivate *priv;
	void *buf;
	GSource *src;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	/*
	 * MEMO: allocate one page because we cannot assume the size of
	 * transaction frame.
	 */
	buf = g_malloc(getpagesize());
	if (buf == NULL) {
		raise(exception, ENOMEM);
		return;
	}

	src = g_source_new(&funcs, sizeof(SndUnitSource));
	if (src == NULL) {
		raise(exception, ENOMEM);
		g_free(buf);
		return;
	}

	g_source_set_name(src, "HinawaSndUnit");
	g_source_set_priority(src, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(src, TRUE);

	((SndUnitSource *)src)->unit = self;
	priv->src = (SndUnitSource *)src;
	priv->buf = buf;
	priv->len = getpagesize();

	((SndUnitSource *)src)->tag =
		hinawa_context_add_src(src, priv->fd, G_IO_IN, exception);
	if (*exception != NULL) {
		g_free(buf);
		g_source_destroy(src);
		priv->buf = NULL;
		priv->len = 0;
		priv->src = NULL;
		return;
	}

	hinawa_fw_unit_listen(&self->parent_instance, exception);
	if (*exception != NULL) {
		hinawa_snd_unit_unlisten(self);
		return;
	}

	/* Check locked or not. */
	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_LOCK, NULL) < 0) {
		if (errno == EBUSY)
			priv->streaming = true;
		return;
	}
	ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL);
}

/**
 * hinawa_snd_unit_unlisten:
 * @self: A #HinawaSndUnit
 *
 * Stop listening to events.
 */
void hinawa_snd_unit_unlisten(HinawaSndUnit *self)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	priv = hinawa_snd_unit_get_instance_private(self);

	if (priv->src == NULL)
		return;

	if (priv->streaming)
		ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL);

	g_source_destroy((GSource *)priv->src);
	g_free(priv->src);
	priv->src = NULL;

	g_free(priv->buf);
	priv->buf = NULL;
	priv->len = 0;

	hinawa_fw_unit_unlisten(&self->parent_instance);
}
