#include <unistd.h>
#include <alsa/asoundlib.h>
#include <sound/firewire.h>

#include "unit_query.h"

G_DEFINE_TYPE(HinawaUnitQuery, hinawa_unit_query, G_TYPE_OBJECT)

static void unit_query_dispose(GObject *obj)
{
	G_OBJECT_CLASS(hinawa_unit_query_parent_class)->dispose(obj);
}

static void unit_query_finalize(GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_unit_query_parent_class)->finalize(gobject);
}

static void hinawa_unit_query_class_init(HinawaUnitQueryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->dispose = unit_query_dispose;
	gobject_class->finalize = unit_query_finalize;
}

static void hinawa_unit_query_init(HinawaUnitQuery *self)
{
	return;
}

gint hinawa_unit_query_get_sibling(gint id, GError **exception)
{
	snd_ctl_t *handle = NULL;
	int err;

	/* NOTE: at least one sound device is expected to be detected. */
	err = snd_ctl_open(&handle, "hw:0", 0);
	if (err < 0)
		goto end;

	err = snd_ctl_hwdep_next_device(handle, &id);
	if (err < 0)
		goto end;
	if (id < 0) {
		err = -ENODEV;
		goto end;
	}

	hinawa_unit_query_get_unit_type(id, exception);
end:
	if (err < 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    -err, "%s", snd_strerror(err));
	if (handle != NULL)
		snd_ctl_close(handle);

	return id;
}

gint hinawa_unit_query_get_unit_type(gint id, GError **exception)
{
	snd_hwdep_t *handle = NULL;
	char path[10] = {0};
	struct snd_firewire_get_info info;
	gint type = -1;
	int err;

	/* Check id and make device string */
	if (id < 0) {
		err = EINVAL;
		goto end;
	}
	snprintf(path, sizeof(path), "hw:%d", id);

	/* Open hwdep handle. */
	err = snd_hwdep_open(&handle, path, 0);
	if (err < 0)
		goto end;

	/* Get FireWire sound device information. */
	err = snd_hwdep_ioctl(handle, SNDRV_FIREWIRE_IOCTL_GET_INFO, &info);
	if (err < 0)
		goto end;
	type = info.type;
end:
	if (err < 0)
		g_set_error(exception, g_quark_from_static_string(__func__),
			    -err, "%s", snd_strerror(err));
	if (handle != NULL)
		snd_hwdep_close(handle);

	return type;
}
