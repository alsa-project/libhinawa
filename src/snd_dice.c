#include <linux/types.h>
#include <sound/firewire.h>
#include <alsa/asoundlib.h>
#include "snd_dice.h"
#include "internal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/**
 * SECTION:snd_dice
 * @Title: HinawaSndDice
 * @Short_description: A notification listener for Dice models
 *
 * A #HinawaSndDice listen to Dice notification and generates signal when
 * received. This inherits #HinawaSndUnit.
 */

struct notification_waiter {
	guint32 bit_flag;
	GCond cond;
};

struct _HinawaSndDicePrivate {
	GList *waiters;
	GMutex lock;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndDice, hinawa_snd_dice, HINAWA_TYPE_SND_UNIT)
#define SND_DICE_GET_PRIVATE(obj)					\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj),				\
				     HINAWA_TYPE_SND_DICE,		\
				     HinawaSndDicePrivate))

/* This object has one signal. */
enum dice_sig_type {
	DICE_SIG_TYPE_NOTIFIED,
	DICE_SIG_TYPE_COUNT,
};
static guint dice_sigs[DICE_SIG_TYPE_COUNT] = { 0 };

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
void hinawa_snd_dice_open(HinawaSndDice *self, gchar *path, GError **exception)
{
	HinawaSndDicePrivate *priv;
	int type;

	g_return_if_fail(HINAWA_IS_SND_DICE(self));
	priv = SND_DICE_GET_PRIVATE(self);

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if (type != SNDRV_FIREWIRE_TYPE_DICE) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EINVAL, "%s", strerror(EINVAL));
		return;
	}

	priv->waiters = NULL;
	g_mutex_init(&priv->lock);
}

/**
 * hinawa_snd_dice_transact:
 * @self: A #HinawaSndDice
 * @addr: A destination address of target device
 * @frame: (element-type guint32) (array) (in): a 32bit array
 * @bit_flag: bit flag to wait
 * @exception: A #GError
 *
 * Execute write transactions to the given address, then wait and check
 * notification.
 */
void hinawa_snd_dice_transact(HinawaSndDice *self, guint64 addr,
			      GArray *frame, guint32 bit_flag,
			      GError **exception)
{
	HinawaSndDicePrivate *priv;
	struct notification_waiter waiter = {0};
	gint64 expiration;

	g_return_if_fail(HINAWA_IS_SND_DICE(self));
	priv = SND_DICE_GET_PRIVATE(self);

	/* Insert this entry to list and enter critical section. */
	g_mutex_lock(&priv->lock);
	priv->waiters = g_list_append(priv->waiters, &waiter);

	/* NOTE: Timeout is 200 milli-seconds. */
	expiration = g_get_monotonic_time() + 200 * G_TIME_SPAN_MILLISECOND;
	g_cond_init(&waiter.cond);

	waiter.bit_flag = bit_flag;
	hinawa_snd_unit_write_transact(&self->parent_instance, addr, frame,
				       exception);
	if (*exception != NULL)
		goto end;

	/*
	 * Wait notification till timeout and temporarily leave the critical
	 * section.
	 */
	if (!g_cond_wait_until(&waiter.cond, &priv->lock, expiration))
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ETIMEDOUT, "%s", strerror(ETIMEDOUT));
end:
	priv->waiters = g_list_remove(priv->waiters, (gpointer *)&waiter);

	g_mutex_unlock(&priv->lock);
}

void hinawa_snd_dice_handle_notification(HinawaSndDice *self,
					 const void *buf, unsigned int len)
{
	HinawaSndDicePrivate *priv;

	GList *entry;
	struct notification_waiter *waiter;

	struct snd_firewire_event_dice_notification *event =
			(struct snd_firewire_event_dice_notification *)buf;

	g_return_if_fail(HINAWA_IS_SND_DICE(self));
	priv = SND_DICE_GET_PRIVATE(self);

	g_signal_emit(self, dice_sigs[DICE_SIG_TYPE_NOTIFIED],
		      0, event->notification);

	g_mutex_lock(&priv->lock);

	for (entry = priv->waiters; entry != NULL; entry = entry->next) {
		waiter = (struct notification_waiter *)entry->data;

		if (waiter->bit_flag & event->notification)
			break;
	}

	g_mutex_unlock(&priv->lock);

	if (entry == NULL)
		return;

	g_cond_signal(&waiter->cond);
}
