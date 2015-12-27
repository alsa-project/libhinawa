#include <string.h>
#include <errno.h>

#include "internal.h"

/**
 * SECTION:snd_dg00x
 * @Title: HinawaSndDg00x
 * @Short_description: A notification listener for Dg00x models
 *
 * A #HinawaSndDg00x listen to Dg00x notification and generates signal when
 * received. This inherits #HinawaSndUnit.
 */

G_DEFINE_TYPE(HinawaSndDg00x, hinawa_snd_dg00x, HINAWA_TYPE_SND_UNIT)

/* For error handling. */
G_DEFINE_QUARK("HinawaSndDg00x", hinawa_snd_dg00x)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_dg00x_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

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
	 * @self: A #HinawaSndDg00x
	 * @message: A message
	 *
	 * When Dg00x models transfer notification, the ::message signal is
	 * generated.
	 */
	dg00x_sigs[DG00X_SIG_TYPE_MESSAGE] =
		g_signal_new("message",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL, NULL,
			     g_cclosure_marshal_VOID__ULONG,
			     G_TYPE_NONE, 1, G_TYPE_ULONG);
}

static void hinawa_snd_dg00x_init(HinawaSndDg00x *self)
{
	return;
}

/**
 * hinawa_snd_dg00x_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Dg00x  devices.
 */
void hinawa_snd_dg00x_open(HinawaSndDg00x *self, gchar *path, GError **exception)
{
	int type;

	g_return_if_fail(HINAWA_IS_SND_DG00X(self));

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if (type != SNDRV_FIREWIRE_TYPE_DIGI00X) {
		raise(exception, EINVAL);
		return;
	}
}

void hinawa_snd_dg00x_handle_msg(HinawaSndDg00x *self, const void *buf,
				 unsigned int len)
{
	struct snd_firewire_event_digi00x_message *event =
			(struct snd_firewire_event_digi00x_message *)buf;

	g_return_if_fail(HINAWA_IS_SND_DG00X(self));

	g_signal_emit(self, dg00x_sigs[DG00X_SIG_TYPE_MESSAGE],
		      0, event->message);
}
