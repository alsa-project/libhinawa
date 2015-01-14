#include <linux/types.h>
#include <sound/firewire.h>
#include <alsa/asoundlib.h>
#include "snd_dice.h"

struct _HinawaSndDicePrivate {
	HinawaSndUnit *unit;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndDice, hinawa_snd_dice, HINAWA_TYPE_SND_UNIT)
#define SND_DICE_GET_PRIVATE(obj)				\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), 			\
				     HINAWA_TYPE_SND_DICE,	\
				     HinawaSndDicePrivate))

/* This object has one signal. */
enum dice_sig_type {
	DICE_SIG_TYPE_NOTIFIED,
	DICE_SIG_TYPE_COUNT,
};
static guint dice_sigs[DICE_SIG_TYPE_COUNT] = { 0 };

static void handle_notification(void *private_data,
				const void *buf, unsigned int len);

static void snd_dice_dispose(GObject *obj)
{
	HinawaSndDice *self = HINAWA_SND_DICE(obj);
	HinawaSndDicePrivate *priv = SND_DICE_GET_PRIVATE(self);

	if (priv->unit != NULL)
		hinawa_snd_unit_remove_handle(priv->unit, handle_notification);

	G_OBJECT_CLASS(hinawa_snd_dice_parent_class)->dispose(obj);
}

static void snd_dice_finalize(GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_snd_dice_parent_class)->finalize(gobject);
}

static void hinawa_snd_dice_class_init(HinawaSndDiceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->dispose = snd_dice_dispose;
	gobject_class->finalize = snd_dice_finalize;

	dice_sigs[DICE_SIG_TYPE_NOTIFIED] =
		g_signal_new("notified",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL, NULL,
			     g_cclosure_marshal_VOID__ULONG,
			     G_TYPE_NONE, 1, G_TYPE_ULONG);
}

static void hinawa_snd_dice_init(HinawaSndDice *self)
{
	self->priv = hinawa_snd_dice_get_instance_private(self);
}

HinawaSndDice *hinawa_snd_dice_new(HinawaSndUnit *unit, GError **exception)
{
	HinawaSndDice *self;
	HinawaSndDicePrivate *priv;

	self = g_object_new(HINAWA_TYPE_SND_DICE, NULL);
	if (self == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}

	hinawa_snd_unit_add_handle(unit, SNDRV_FIREWIRE_TYPE_DICE,
				   handle_notification, self, exception);
	if (*exception != NULL) {
		g_clear_object(&self);
		return NULL;
	}

	priv = SND_DICE_GET_PRIVATE(self);
	priv->unit = unit;

	return self;
}

static void handle_notification(void *private_data,
				const void *buf, unsigned int len)
{
	HinawaSndDice *self = (HinawaSndDice *)private_data;

	struct snd_firewire_event_dice_notification *event =
			(struct snd_firewire_event_dice_notification *)buf;

	g_signal_emit(self, dice_sigs[DICE_SIG_TYPE_NOTIFIED],
		      0, event->notification);
}
