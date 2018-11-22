#include <string.h>
#include <errno.h>

#include "internal.h"
#include "snd_tscm.h"
#include "hinawa_sigs_marshal.h"

/**
 * SECTION:snd_tascam
 * @Title: HinawaSndTscm
 * @Short_description: A class for TASCAM FireWire models
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaSndTscm", hinawa_snd_tscm)
#define raise(exception, errno)					\
	g_set_error(exception, hinawa_snd_tscm_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

G_DEFINE_TYPE(HinawaSndTscm, hinawa_snd_tscm, HINAWA_TYPE_SND_UNIT)

static void hinawa_snd_tscm_class_init(HinawaSndTscmClass *klass)
{
	return;
}

static void hinawa_snd_tscm_init(HinawaSndTscm *self)
{
	return;
}

/**
 * hinawa_snd_tscm_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Dg00x  devices.
 */
void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path, GError **exception)
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
