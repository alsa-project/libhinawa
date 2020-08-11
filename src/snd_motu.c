/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"

/**
 * SECTION:snd_motu
 * @Title: HinawaSndMotu
 * @Short_description: A notification listener for Motu models
 * @include: snd_motu.h
 *
 * A #HinawaSndMotu is an application of asynchronous notification defined by
 * Mark of the Unicorn (MOTU). This inherits #HinawaSndUnit.
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
			     G_STRUCT_OFFSET(HinawaSndMotuClass, notified),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__UINT,
			     G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void hinawa_snd_motu_init(HinawaSndMotu *self)
{
	return;
}

/**
 * hinawa_snd_motu_new:
 *
 * Instantiate #HinawaSndMotu object and return the instance.
 *
 * Returns: an instance of #HinawaSndMotu.
 * Since: 1.3.
 */
HinawaSndMotu *hinawa_snd_motu_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_MOTU, NULL);
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
	g_return_if_fail(HINAWA_IS_SND_MOTU(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
}

void hinawa_snd_motu_handle_notification(HinawaSndMotu *self,
					 const void *buf, ssize_t len)
{
	const struct snd_firewire_event_motu_notification *event = buf;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));

	g_signal_emit(self, motu_sigs[MOTU_SIG_TYPE_NOTIFIED], 0,
		      event->message);
}
