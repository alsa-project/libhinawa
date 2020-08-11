/* SPDX-License-Identifier: LGPL-2.1-or-later */
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
#include "hinawa_enums.h"

/**
 * SECTION:snd_unit
 * @Title: HinawaSndUnit
 * @Short_description: An event listener for ALSA FireWire sound devices
 * @include: snd_unit.h
 *
 * This class is an application of ALSA FireWire stack. Any functionality which
 * ALSA drivers in the stack can be available.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaSndUnit", hinawa_snd_unit)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_unit_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

struct _HinawaSndUnitPrivate {
	int fd;
	struct snd_firewire_get_info info;
	HinawaFwNode *node;

	gboolean streaming;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndUnit, hinawa_snd_unit, G_TYPE_OBJECT)

typedef struct {
	GSource src;
	HinawaSndUnit *unit;
	gpointer tag;
	void *buf;
	size_t len;
} SndUnitSource;

enum snd_unit_prop_type {
	SND_UNIT_PROP_TYPE_FW_TYPE = 1,
	SND_UNIT_PROP_TYPE_CARD_ID,
	SND_UNIT_PROP_TYPE_DEVICE,
	SND_UNIT_PROP_TYPE_GUID,
	SND_UNIT_PROP_TYPE_STREAMING,
	SND_UNIT_PROP_TYPE_COUNT,
};
static GParamSpec *snd_unit_props[SND_UNIT_PROP_TYPE_COUNT] = { NULL, };

// This object has two signals.
enum snd_unit_sig_type {
	SND_UNIT_SIG_TYPE_LOCK_STATUS = 0,
	SND_UNIT_SIG_TYPE_DISCONNECTED,
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
		g_value_set_enum(val, (HinawaSndUnitType)priv->info.type);
		break;
	case SND_UNIT_PROP_TYPE_CARD_ID:
		g_value_set_uint(val, priv->info.card);
		break;
	case SND_UNIT_PROP_TYPE_DEVICE:
		g_value_set_static_string(val,
					(const gchar *)priv->info.device_name);
		break;
	case SND_UNIT_PROP_TYPE_GUID:
		g_value_set_uint64(val,
				GUINT64_FROM_BE(*((guint64 *)priv->info.guid)));
		break;
	case SND_UNIT_PROP_TYPE_STREAMING:
		g_value_set_boolean(val, priv->streaming);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void snd_unit_finalize(GObject *obj)
{
	HinawaSndUnit *self = HINAWA_SND_UNIT(obj);
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);

	close(priv->fd);
	priv->fd = -1;
	g_object_unref(priv->node);

	G_OBJECT_CLASS(hinawa_snd_unit_parent_class)->finalize(obj);
}

static void hinawa_snd_unit_class_init(HinawaSndUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = snd_unit_get_property;
	gobject_class->finalize = snd_unit_finalize;

	snd_unit_props[SND_UNIT_PROP_TYPE_FW_TYPE] =
		g_param_spec_enum("type", "type",
				  "The value of HinawaSndUnitType enumerators",
				  HINAWA_TYPE_SND_UNIT_TYPE,
				  HINAWA_SND_UNIT_TYPE_DICE,
				  G_PARAM_READABLE);
	snd_unit_props[SND_UNIT_PROP_TYPE_CARD_ID] =
		g_param_spec_uint("card", "card",
				  "The numerical ID for ALSA sound card",
				  0, G_MAXUINT,
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
				    0, G_MAXUINT64, 0,
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
			     G_STRUCT_OFFSET(HinawaSndUnitClass, lock_status),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__BOOLEAN,
			     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	/**
	 * HinawaSndUnit::disconnected:
	 * @self: A #HinawaSndUnit
	 *
	 * When the sound card is not available anymore due to unbinding driver
	 * or hot unplugging, this signal is emit. The owner of this object
	 * should call g_object_free() as quickly as possible to release ALSA
	 * hwdep character device.
	 */
	snd_unit_sigs[SND_UNIT_SIG_TYPE_DISCONNECTED] =
		g_signal_new("disconnected",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaSndUnitClass, disconnected),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0, G_TYPE_NONE, 0);
}

static void hinawa_snd_unit_init(HinawaSndUnit *self)
{
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);

	priv->fd = -1;
	priv->node = g_object_new(HINAWA_TYPE_FW_NODE, NULL);
}

/**
 * hinawa_snd_unit_new:
 *
 * Instantiate #HinawaSndUnit object and return the instance.
 *
 * Returns: an instance of #HinawaSndUnit.
 * Since: 1.3.
 */
HinawaSndUnit *hinawa_snd_unit_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_UNIT, NULL);
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
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);

	if (priv->fd >= 0) {
		raise(exception, EBUSY);
		return;
	}

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

	if ((HINAWA_IS_SND_DICE(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_DICE) ||
	    (HINAWA_IS_SND_EFW(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_FIREWORKS) ||
	    (HINAWA_IS_SND_DG00X(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_DIGI00X) ||
	    (HINAWA_IS_SND_MOTU(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_MOTU) ||
	    (HINAWA_IS_SND_TSCM(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_TASCAM)) {
		raise(exception, EINVAL);
		goto end;
	}

	snprintf(fw_cdev, sizeof(fw_cdev), "/dev/%s", priv->info.device_name);
	hinawa_fw_node_open(priv->node, fw_cdev, exception);
end:
	if (*exception != NULL) {
		close(priv->fd);
		priv->fd = -1;
	}
}

/**
 * hinawa_snd_unit_get_node:
 * @self: A #HinawaSndUnit.
 * @node: (out)(transfer none): A #HinawaFwNode.
 *
 * Retrieve an instance of #HinawaFwNode associated to the given unit.
 *
 * Since: 2.0.
 */
void hinawa_snd_unit_get_node(HinawaSndUnit *self, HinawaFwNode **node)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(node != NULL);

	priv = hinawa_snd_unit_get_instance_private(self);

	*node = priv->node;
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
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

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
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL) < 0)
		raise(exception, errno);
}

/* For internal use. */
void hinawa_snd_unit_write(HinawaSndUnit *self, const void *buf, size_t length,
			   GError **exception)
{
	HinawaSndUnitPrivate *priv;
	ssize_t len;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

	len = write(priv->fd, buf, length);
	if (len < 0)
		raise(exception, errno);
	else if (len != length)
		raise(exception, EIO);
}

void hinawa_snd_unit_ioctl(HinawaSndUnit *self, unsigned long request,
			   void *arg, GError **exception)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

	if (ioctl(priv->fd, request, arg) < 0)
		raise(exception, errno);
}

static void handle_lock_event(HinawaSndUnit *self,
			      const void *buf, ssize_t length)
{
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);
	const struct snd_firewire_event_lock_status *event = buf;

	priv->streaming = event->status;

	g_signal_emit(self, snd_unit_sigs[SND_UNIT_SIG_TYPE_LOCK_STATUS], 0,
		      event->status);
}

static gboolean check_src(GSource *gsrc)
{
	SndUnitSource *src = (SndUnitSource *)gsrc;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an exception, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(gsrc, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	SndUnitSource *src = (SndUnitSource *)gsrc;
	HinawaSndUnit *unit = src->unit;
	HinawaSndUnitPrivate *priv;
	GIOCondition condition;
	struct snd_firewire_event_common *common;
	ssize_t len;

	priv = hinawa_snd_unit_get_instance_private(unit);
	if (priv->fd < 0)
		return G_SOURCE_REMOVE;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR) {
		g_signal_emit(unit,
			      snd_unit_sigs[SND_UNIT_SIG_TYPE_DISCONNECTED], 0,
			      NULL);

		return G_SOURCE_REMOVE;
	}

	len = read(priv->fd, src->buf, src->len);
	if (len <= 0)
		goto end;

	common = (struct snd_firewire_event_common *)src->buf;

	if (common->type == SNDRV_FIREWIRE_EVENT_LOCK_STATUS)
		handle_lock_event(unit, src->buf, len);
	else if (HINAWA_IS_SND_DICE(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_DICE_NOTIFICATION)
		hinawa_snd_dice_handle_notification(HINAWA_SND_DICE(unit),
						    src->buf, len);
	else if (HINAWA_IS_SND_EFW(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_EFW_RESPONSE)
		hinawa_snd_efw_handle_response(HINAWA_SND_EFW(unit),
					       src->buf, len);
	else if (HINAWA_IS_SND_DG00X(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_DIGI00X_MESSAGE)
		hinawa_snd_dg00x_handle_msg(HINAWA_SND_DG00X(unit),
					    src->buf, len);
	else if (HINAWA_IS_SND_MOTU(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_MOTU_NOTIFICATION)
		hinawa_snd_motu_handle_notification(HINAWA_SND_MOTU(unit),
						    src->buf, len);
	else if (HINAWA_IS_SND_TSCM(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_TASCAM_CONTROL)
		hinawa_snd_tscm_handle_control(HINAWA_SND_TSCM(unit),
						    src->buf, len);
end:
	/* Just be sure to continue to process this source. */
	return G_SOURCE_CONTINUE;
}

static void finalize_src(GSource *gsrc)
{
	SndUnitSource *src = (SndUnitSource *)gsrc;
	HinawaSndUnitPrivate *priv =
				hinawa_snd_unit_get_instance_private(src->unit);

	if (priv->streaming)
		ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL);

	g_free(src->buf);
}

/**
 * hinawa_snd_unit_create_source:
 * @self: A #HinawaSndUnit.
 * @gsrc: (out): A #GSource.
 * @exception: A #GError.
 *
 * Create Gsource for GMainContext to dispatch events for the sound device.
 *
 * Since: 1.4.
 */
void hinawa_snd_unit_create_source(HinawaSndUnit *self, GSource **gsrc,
				   GError **exception)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	HinawaSndUnitPrivate *priv;
	SndUnitSource *src;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(gsrc != NULL);
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);

	if (priv->fd < 0) {
		raise(exception, ENXIO);
		return;
	}

	*gsrc = g_source_new(&funcs, sizeof(SndUnitSource));

	g_source_set_name(*gsrc, "HinawaSndUnit");
	g_source_set_priority(*gsrc, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*gsrc, TRUE);

	// MEMO: allocate one page because we cannot assume the size of
	// transaction frame.
	src = (SndUnitSource *)(*gsrc);
	src->len = sysconf(_SC_PAGESIZE);
	src->buf = g_malloc(src->len);

	src->unit = self;
	src->tag = g_source_add_unix_fd(*gsrc, priv->fd, G_IO_IN);

	// Check locked or not.
	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_LOCK, NULL) < 0) {
		if (errno == EBUSY)
			priv->streaming = true;
		return;
	}
	ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL);
}
