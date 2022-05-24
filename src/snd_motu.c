// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>

/**
 * HinawaSndMotu:
 * A notification listener for Motu models.
 *
 * A [class@SndMotu] is an application of asynchronous notification defined by Mark of the Unicorn
 * (MOTU).
 *
 * Deprecated: 2.5. libhitaki library provides [class@Hitaki.SndMotu] as the alternative.
 */

typedef struct {
	struct snd_firewire_motu_status *status;
} HinawaSndMotuPrivate;
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
	 * @self: A [class@SndMotu]
	 * @message: A notification message
	 *
	 * Emitted when Motu models transfer notification.
	 *
	 * Since: 0.8
	 * Deprecated: 2.5. Use implementation of [signal@Hitaki.QuadletNotification::notified]
	 *	       in [class@Hitaki.SndMotu] instead.
	 */
	motu_sigs[MOTU_SIG_TYPE_NOTIFIED] =
		g_signal_new("notified",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
			     G_STRUCT_OFFSET(HinawaSndMotuClass, notified),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__UINT,
			     G_TYPE_NONE, 1, G_TYPE_UINT);

	/**
	 * HinawaSndMotu::register-dsp-changed:
	 * @self: A [class@SndMotu]
	 * @events: (element-type guint32)(array length=length): The array with element for
	 *	    unsigned 32 bit encoded data.
	 * @length: The length of events.
	 *
	 * Emitted when MOTU register DSP models transfer events by messages in the sequence of
	 * isochronous packet. The event consists of encoded data. The most significant byte is the
	 * type of message. The next two bytes are identifier 0 and 1. The least significant byte
	 * is value. The meaning of identifier 0, 1 and value is decided depending on the type.
	 * For detail, see `sound/firewire/motu/motu-register-dsp-message-parser.c` in Linux kernel.
	 *
	 * Since: 2.4
	 * Deprecated: 2.5. Use implementation [signal@Hitaki.MotuRegisterDsp::changed] in
	 *	       [class@Hitaki.SndMotu] instead.
	 */
	motu_sigs[MOTU_SIG_TYPE_REGISTER_DSP_CHANGED] =
		g_signal_new("register-dsp-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
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
 * Instantiate [class@SndMotu] object and return the instance.
 *
 * Returns: an instance of [class@SndMotu].
 *
 * Since: 1.3.
 * Deprecated: 2.5. Use [method@Hitaki.SndMotu.new] instead.
 */
HinawaSndMotu *hinawa_snd_motu_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_MOTU, NULL);
}

/**
 * hinawa_snd_motu_open:
 * @self: A [class@SndMotu]
 * @path: A full path of a special file for ALSA hwdep character device
 * @error: A [struct@GLib.Error]. Error can be generated with three domains; GLib.FileError,
 *	   Hinawa.FwNodeError, and Hinawa.SndUnitError.
 *
 * Open ALSA hwdep character device and check it for Motu devices.
 *
 * Since: 0.8
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.open] in
 *	       [class@Hitaki.SndMotu] instead.
 */
void hinawa_snd_motu_open(HinawaSndMotu *self, gchar *path, GError **error)
{
	g_return_if_fail(HINAWA_IS_SND_MOTU(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(error == NULL || *error == NULL);

	hinawa_snd_unit_open(&self->parent_instance, path, error);
}

/**
 * hinawa_snd_motu_read_register_dsp_parameter:
 * @self: A [class@SndMotu].
 * @param: (inout): A [struct@SndMotuRegisterDspParameter].
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; GLib.FileError and
 *	   Hinawa.SndUnitError.
 *
 * Read parameter for register DSP models.
 *
 * Since: 2.4
 * Deprecated: 2.5. Use implementation of [method@Hitaki.MotuRegisterDsp.read_parameter] in
 *	       [class@Hitaki.SndMotu] instead.
 */
void hinawa_snd_motu_read_register_dsp_parameter(HinawaSndMotu *self,
						 HinawaSndMotuRegisterDspParameter *const *param,
						 GError **error)
{
	struct snd_firewire_motu_register_dsp_parameter *arg;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));
	g_return_if_fail(*param != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	arg = (struct snd_firewire_motu_register_dsp_parameter *)(*param);

	hinawa_snd_unit_ioctl(HINAWA_SND_UNIT(self),
			      SNDRV_FIREWIRE_IOCTL_MOTU_REGISTER_DSP_PARAMETER, arg, error);

}

/**
 * hinawa_snd_motu_read_register_dsp_meter:
 * @self: A [class@SndMotu]
 * @meter: (array fixed-size=48)(inout): The data of meter. Index 0 to 23 for inputs and index 24
 *	   to 47 for outputs.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; GLib.FileError and
 *	   Hinawa.SndUnitError.
 *
 * Read data of meter information for register DSP models.
 *
 * Since: 2.4
 * Deprecated: 2.5. Use implementation of [method@Hitaki.MotuRegisterDsp.read_byte_meter] in
 *	       [class@Hitaki.SndMotu] instead.
 */
void hinawa_snd_motu_read_register_dsp_meter(HinawaSndMotu *self, guint8 *const meter[48],
					     GError **error)
{
	struct snd_firewire_motu_register_dsp_meter *arg;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));
	g_return_if_fail(*meter != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	arg = (struct snd_firewire_motu_register_dsp_meter *)(*meter);

	hinawa_snd_unit_ioctl(HINAWA_SND_UNIT(self), SNDRV_FIREWIRE_IOCTL_MOTU_REGISTER_DSP_METER,
			      arg, error);
}

/**
 * hinawa_snd_motu_read_command_dsp_meter:
 * @self: A [class@SndMotu]
 * @meter: (array fixed-size=400)(inout): The data for meter.
 * @error: A [struct@GLib.Error]. Error can be generated with two domains; GLib.FileError and
 *	   Hinawa.SndUnitError.
 *
 * Read data of meter information for command DSP models.
 *
 * Since: 2.4
 * Deprecated: 2.5. Use implementation of [method@Hitaki.MotuCommandDsp.read_float_meter] in
 *	       [class@Hitaki.SndMotu] instead.
 */
void hinawa_snd_motu_read_command_dsp_meter(HinawaSndMotu *self, gfloat *const meter[400],
					    GError **error)
{
	struct snd_firewire_motu_command_dsp_meter *arg;

	g_return_if_fail(HINAWA_IS_SND_MOTU(self));
	g_return_if_fail(*meter != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	arg = (struct snd_firewire_motu_command_dsp_meter *)(*meter);

	hinawa_snd_unit_ioctl(HINAWA_SND_UNIT(self), SNDRV_FIREWIRE_IOCTL_MOTU_COMMAND_DSP_METER,
			      arg, error);
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
