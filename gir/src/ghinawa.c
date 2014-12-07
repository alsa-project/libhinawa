#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libhinawa/hinawa.h>
#include "ghinawa.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

struct _GHinawaSndUnitPrivate {
	HinawaSndUnit *unit;
};
G_DEFINE_TYPE_WITH_PRIVATE (GHinawaSndUnit, ghinawa_snd_unit, G_TYPE_OBJECT)

static void ghinawa_snd_unit_dispose (GObject *gobject)
{
	G_OBJECT_CLASS (ghinawa_snd_unit_parent_class)->dispose(gobject);
}

/*
gobject_new() -> g_clear_object()
*/

static void ghinawa_snd_unit_finalize (GObject *gobject)
{
	GHinawaSndUnit *self = GHINAWA_SND_UNIT(gobject);

	if (self->priv->unit)
		g_free(self->priv->unit);

	G_OBJECT_CLASS(ghinawa_snd_unit_parent_class)->finalize (gobject);
}

static void ghinawa_snd_unit_class_init(GHinawaSndUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = ghinawa_snd_unit_dispose;
	gobject_class->finalize = ghinawa_snd_unit_finalize;

	printf("class initialized\n");
}

static void
ghinawa_snd_unit_init(GHinawaSndUnit *self)
{
	self->priv = ghinawa_snd_unit_get_instance_private(self);
	printf("init\n");
}

gboolean ghinawa_snd_unit_new(GHinawaSndUnit *self, gchar *name)
{
	int err = 0;

	self->priv->unit = hinawa_snd_unit_create(name, &err);
	if (err > 0)
		return FALSE;

	return TRUE;
}

gint ghinawa_snd_unit_assign_hwdep(GHinawaSndUnit *self, gchar *name)
{
	gint err = 0;

	if (self->priv->unit)
		return EINVAL;

	self->priv->unit = hinawa_snd_unit_create(name, &err);
	return err;
}

const gchar *ghinawa_snd_unit_greet(GHinawaSndUnit *unit)
{
    return "Hello!";
}

gchar *ghinawa_snd_unit_do(GHinawaSndUnit *unit, gchar *str, const guint length)
{
	unsigned int i;

	printf("\n%s\n", str);

	for (i = 0; i < length; i++)
		str[i] = 65 + i;

	printf("%s\n\n", str);
}

void ghinawa_snd_unit_set_cb(GHinawaSndUnit *unit, GHinawaSndUnitCB cb,
			     void *private_data)
{
	cb(unit, private_data, 10);
}
