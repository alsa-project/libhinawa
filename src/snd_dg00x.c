// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>

/**
 * HinawaSndDg00x:
 * A notification listener for Digidesign Digi 00x models.
 *
 * A [class@SndDg00x] listen to Dg00x notification and generates signal when received.
 *
 * Deprecated: 2.5. libhitaki library provides [class@Hitaki.SndDigi00x] as the alternative.
 */

G_DEFINE_TYPE(HinawaSndDg00x, hinawa_snd_dg00x, HINAWA_TYPE_SND_UNIT)

/* This object has one signal. */
enum dg00x_sig_type {
	DG00X_SIG_TYPE_MESSAGE,
	DG00X_SIG_TYPE_COUNT,
};
static guint dg00x_sigs[DG00X_SIG_TYPE_COUNT] = { 0 };

static void hinawa_snd_dg00x_class_init(HinawaSndDg00xClass *klass)
{
	/**
	 * HinawaSndDg00x::message:
	 * @self: A [class@SndDg00x]
	 * @message: A message
	 *
	 * Emitted when Dg00x models transfer notification.
	 *
	 * Since: 0.7
	 * Deprecated: 2.5. Use implementation of [signal@Hitaki.AlsaFirewire::notified] in
	 *	       [class@Hitaki.SndDigi00x] instead.
	 */
	dg00x_sigs[DG00X_SIG_TYPE_MESSAGE] =
		g_signal_new("message",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
			     G_STRUCT_OFFSET(HinawaSndDg00xClass, message),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__UINT,
			     G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void hinawa_snd_dg00x_init(HinawaSndDg00x *self)
{
	return;
}

/**
 * hinawa_snd_dg00x_new:
 *
 * Instantiate [class@SndDg00x] object and return the instance.
 *
 * Returns: an instance of [class@SndDg00x].
 *
 * Since: 1.3.
 * Deprecated: 2.5. Use [method@Hitaki.SndDigi00x.new] instead.
 */
HinawaSndDg00x *hinawa_snd_dg00x_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_DG00X, NULL);
}

/**
 * hinawa_snd_dg00x_open:
 * @self: A [class@SndDg00x]
 * @path: A full path of a special file for ALSA hwdep character device
 * @error: A [struct@GLib.Error]. Error can be generated with three domains; GLib.FileError,
 *	   Hinawa.FwNodeError, and Hinawa.SndUnitError.
 *
 * Open ALSA hwdep character device and check it for Dg00x devices.
 *
 * Since: 0.7
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.open]
 *	       [class@Hitaki.SndDigi00x] for instead.
 */
void hinawa_snd_dg00x_open(HinawaSndDg00x *self, gchar *path, GError **error)
{
	g_return_if_fail(HINAWA_IS_SND_DG00X(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(error == NULL || *error == NULL);

	hinawa_snd_unit_open(&self->parent_instance, path, error);
}

void hinawa_snd_dg00x_handle_msg(HinawaSndDg00x *self, const void *buf,
				 ssize_t len)
{
	const struct snd_firewire_event_digi00x_message *event = buf;

	g_return_if_fail(HINAWA_IS_SND_DG00X(self));

	g_signal_emit(self, dg00x_sigs[DG00X_SIG_TYPE_MESSAGE], 0,
		      event->message);
}
