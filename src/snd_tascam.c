#include <linux/types.h>
#include <sound/firewire.h>
#include "snd_tascam.h"
#include "internal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/**
 * SECTION:snd_tascam
 * @Title: HinawaSndTscm
 * @Short_description: A notification listener for Dg00x models
 *
 * A #HinawaSndTscm listen to Dg00x notification and generates signal when
 * received. This inherits #HinawaSndUnit.
 */

G_DEFINE_TYPE(HinawaSndTscm, hinawa_snd_tascam, HINAWA_TYPE_SND_UNIT)

/* For error handling. */
G_DEFINE_QUARK("HinawaSndTscm", hinawa_snd_tascam)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_tascam_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

static void snd_tascam_dispose(GObject *obj)
{
	G_OBJECT_CLASS(hinawa_snd_tascam_parent_class)->dispose(obj);
}

static void snd_tascam_finalize(GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_snd_tascam_parent_class)->finalize(gobject);
}

static void hinawa_snd_tascam_class_init(HinawaSndTscmClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->dispose = snd_tascam_dispose;
	gobject_class->finalize = snd_tascam_finalize;
}

static void hinawa_snd_tascam_init(HinawaSndTscm *self)
{
	return;
}

/**
 * hinawa_snd_tascam_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Dg00x  devices.
 */
void hinawa_snd_tascam_open(HinawaSndTscm *self, gchar *path,
			    GError **exception)
{
	int type;

	g_return_if_fail(HINAWA_IS_SND_TSCM(self));

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if (type != SNDRV_FIREWIRE_TYPE_TASCAM) {
		raise(exception, EINVAL);
		return;
	}
}
#include <stdio.h>
void hinawa_snd_tascam_get_status(HinawaSndTscm *self, GError **exception)
{
	struct snd_firewire_event_tascam_status status;
	unsigned int i;

	g_return_if_fail(HINAWA_IS_SND_TSCM(self));

	hinawa_snd_unit_read(&self->parent_instance, &status,
			     sizeof(status), exception);
	if (*exception != NULL) {
		raise(exception, errno);
		return;
	}

	for (i = 0; i < 64; i++) {
		printf("[%02d] %08x\n", i, status.status[i]);
	}
}
