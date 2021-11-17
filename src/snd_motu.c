/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "hinawa_sigs_marshal.h"

/**
 * SECTION:snd_motu
 * @Title: HinawaSndMotu
 * @Short_description: A notification listener for Motu models
 * @include: snd_motu.h
 *
 * A #HinawaSndMotu is an application of asynchronous notification defined by
 * Mark of the Unicorn (MOTU). This inherits #HinawaSndUnit.
 */

struct _HinawaSndMotuPrivate {
	struct snd_firewire_motu_status *status;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndMotu, hinawa_snd_motu, HINAWA_TYPE_SND_UNIT)

/* This object has one signal. */
enum motu_sig_type {
	MOTU_SIG_TYPE_NOTIFIED,
	MOTU_SIG_TYPE_REGISTER_DSP_CHANGED,
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
	 * When Motu models transfer notification, the #HinawaSndMotu::notified signal is
	 * generated.
	 *
	 * Since: 0.8
	 */
	motu_sigs[MOTU_SIG_TYPE_NOTIFIED] =
		g_signal_new("notified",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaSndMotuClass, notified),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__UINT,
			     G_TYPE_NONE, 1, G_TYPE_UINT);

	/**
	 * HinawaSndMotu::register-dsp-changed:
	 * @self: A #HinawaSndMotu
	 * @events: (element-type guint32)(array length=length): The array with element for
	 *	    unsigned 32 bit encoded data.
	 * @length: The length of events.
	 *
	 * When MOTU register DSP models transfer events by messages in the sequence of isochronous
	 * packet, the #HinawaSndMotu::register-dsp-changed signal is emit.
	 * The event consists of encoded data. The most significant byte is the type of message. The
	 * next two bytes are identifier 0 and 1. The least significant byte is value. The meaning
	 * of identifier 0, 1 and value is decided depending on the type. For detail, see
	 * `sound/firewire/motu/motu-register-dsp-message-parser.c` in Linux kernel.
	 *
	 * Since: 2.4
	 */
	motu_sigs[MOTU_SIG_TYPE_REGISTER_DSP_CHANGED] =
		g_signal_new("register-dsp-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaSndMotuClass, register_dsp_changed),
			     NULL, NULL,
			     hinawa_sigs_marshal_VOID__POINTER_UINT,
			     G_TYPE_NONE,
			     2, G_TYPE_POINTER, G_TYPE_UINT);
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
 * @exception: A #GError. Error can be generated with three domains; #g_file_error_quark(),
 *	       #hinawa_fw_node_error_quark(), and #hinawa_snd_unit_error_quark().
 *
 * Open ALSA hwdep character device and check it for Motu devices.
 *
 * Since: 0.8
 */
void hinawa_snd_motu_open(HinawaSndMotu *self, gchar *path, GError **exception)
{
	g_return_if_fail(HINAWA_IS_SND_MOTU(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
}

/**
 * hinawa_snd_motu_read_register_dsp_meter:
 * @self: A #HinawaSndMotu
 * @meter: (array fixed-size=48)(inout): The data of meter. Index 0 to 23 for inputs and index 24
 *	   to 47 for outputs.
 * @exception: A #GError. Error can be generated with two domains; #g_file_error_quark(), and
 *	       #hinawa_snd_unit_error_quark().
 *
 * Read data of meter information for register DSP models.
 *
 * Since: 2.4
 */
void hinawa_snd_motu_read_register_dsp_meter(HinawaSndMotu *self, guint8 *const meter[48],
					     GError **exception)
{
	struct snd_firewire_motu_register_dsp_meter *arg;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));
	g_return_if_fail(*meter != NULL);
	g_return_if_fail(exception == NULL || *exception == NULL);

	arg = (struct snd_firewire_motu_register_dsp_meter *)(*meter);

	hinawa_snd_unit_ioctl(HINAWA_SND_UNIT(self), SNDRV_FIREWIRE_IOCTL_MOTU_REGISTER_DSP_METER,
			      arg, exception);
}

void hinawa_snd_motu_handle_notification(HinawaSndMotu *self,
					 const void *buf, ssize_t len)
{
	const struct snd_firewire_event_motu_notification *event = buf;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));

	g_signal_emit(self, motu_sigs[MOTU_SIG_TYPE_NOTIFIED], 0,
		      event->message);
}

void hinawa_snd_motu_handle_register_dsp_change(HinawaSndMotu *self, const void *buf, ssize_t len)
{
	const struct snd_firewire_event_motu_register_dsp_change *ev = buf;

	g_signal_emit(self, motu_sigs[MOTU_SIG_TYPE_REGISTER_DSP_CHANGED], 0,  &ev->changes,
		      ev->count);
}
