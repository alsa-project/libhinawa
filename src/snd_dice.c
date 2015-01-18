#include <linux/types.h>
#include <sound/firewire.h>
#include <alsa/asoundlib.h>
#include "snd_dice.h"

/**
 * SECTION:snd_dice
 * @Title: HinawaSndDice
 * @Short_description: A notification listener for Dice models
 *
 * A #HinawaSndDice listen to Dice notification and generates signal when
 * received. This inherits #HinawaSndUnit.
 */

G_DEFINE_TYPE(HinawaSndDice, hinawa_snd_dice, HINAWA_TYPE_SND_UNIT)

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

	/**
	 * HinawaSndDice::notified:
	 * @self: A #HinawaSndDice
	 * @message: A notification message
	 * 
	 * When Dice models transfer notification, the ::notified signal is
	 * generated.
	 */
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

/**
 * hinawa_snd_dice_new:
 * @path: A path to ALSA hwdep device for Dice models (i.e. hw:0)
 * @exception: A #GError
 *
 * Returns: An instance of #HinawaSndDice
 */
HinawaSndDice *hinawa_snd_dice_new(gchar *path, GError **exception)
{
	HinawaSndDice *self;
	int type;

	self = g_object_new(HINAWA_TYPE_SND_DICE, NULL);
	if (self == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}

	hinawa_snd_unit_new_with_instance(&self->parent_instance, path,
					  handle_notification, self, exception);
	if (*exception != NULL) {
		g_clear_object(&self);
		return NULL;
	}

	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if (type != SNDRV_FIREWIRE_TYPE_DICE) {
		g_clear_object(&self);
		return NULL;
	}

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
