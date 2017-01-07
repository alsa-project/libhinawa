#include <string.h>
#include <errno.h>

#include "internal.h"

/**
 * SECTION:snd_motu
 * @Title: HinawaSndMotu
 * @Short_description: A notification listener for Motu models
 *
 * TODO: A #HinawaSndMotu inherits #HinawaSndUnit.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaSndMotu", hinawa_snd_motu)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_motu_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

struct _HinawaSndMotuPrivate {
	struct snd_firewire_motu_status *status;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndMotu, hinawa_snd_motu, HINAWA_TYPE_SND_UNIT)

/* This object has one signal. */
enum motu_sig_type {
	MOTU_SIG_TYPE_NOTIFIED,
	MOTU_SIG_TYPE_COUNT,
};
static guint motu_sigs[MOTU_SIG_TYPE_COUNT] = { 0 };

static void hinawa_snd_motu_class_init(HinawaSndMotuClass *klass)
{
	/**
	 * HinawaSndMotu::notified:
	 * @self: A #HinawaSndMotu
	 * @message: A notification message
	 *
	 * When Motu models transfer notification, the ::notified signal is
	 * generated.
	 */
	motu_sigs[MOTU_SIG_TYPE_NOTIFIED] =
		g_signal_new("notified",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL, NULL,
			     g_cclosure_marshal_VOID__ULONG,
			     G_TYPE_NONE, 1, G_TYPE_ULONG);
}

static void hinawa_snd_motu_init(HinawaSndMotu *self)
{
	return;
}

/**
 * hinawa_snd_motu_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Motu devices.
 */
void hinawa_snd_motu_open(HinawaSndMotu *self, gchar *path, GError **exception)
{
	int type;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if (type != SNDRV_FIREWIRE_TYPE_MOTU) {
		raise(exception, EINVAL);
		return;
	}
}

void hinawa_snd_motu_handle_notification(HinawaSndMotu *self,
					 const void *buf, unsigned int len)
{
	struct snd_firewire_event_motu_notification *event =
			(struct snd_firewire_event_motu_notification *)buf;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));

	g_signal_emit(self, motu_sigs[MOTU_SIG_TYPE_NOTIFIED], 0,
		      event->message);
}
