/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"

/**
 * SECTION:snd_dice
 * @Title: HinawaSndDice
 * @Short_description: A notification listener for Dice models
 * @include: snd_dice.h
 *
 * A #HinawaSndDice listen to Dice notification and generates signal when
 * received. This inherits #HinawaSndUnit.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaSndDice", hinawa_snd_dice)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_dice_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

struct notification_waiter {
	guint32 bit_flag;
	GCond cond;
	gboolean awakened;
};

struct _HinawaSndDicePrivate {
	HinawaFwReq *req;
	GList *waiters;
	GMutex mutex;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndDice, hinawa_snd_dice, HINAWA_TYPE_SND_UNIT)

/* This object has one signal. */
enum dice_sig_type {
	DICE_SIG_TYPE_NOTIFIED,
	DICE_SIG_TYPE_COUNT,
};
static guint dice_sigs[DICE_SIG_TYPE_COUNT] = { 0 };

static void snd_dice_finalize(GObject *obj)
{
	HinawaSndDice *self = HINAWA_SND_DICE(obj);
	HinawaSndDicePrivate *priv = hinawa_snd_dice_get_instance_private(self);

	g_clear_object(&priv->req);
	g_mutex_clear(&priv->mutex);

	G_OBJECT_CLASS(hinawa_snd_dice_parent_class)->finalize(obj);
}

static void hinawa_snd_dice_class_init(HinawaSndDiceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

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
			     G_STRUCT_OFFSET(HinawaSndDiceClass, notified),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__UINT,
			     G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void hinawa_snd_dice_init(HinawaSndDice *self)
{
	return;
}

/**
 * hinawa_snd_dice_new:
 *
 * Instantiate #HinawaSndDice object and return the instance.
 *
 * Returns: an instance of #HinawaSndDice.
 * Since: 1.3.
 */
HinawaSndDice *hinawa_snd_dice_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_DICE, NULL);
}

/**
 * hinawa_snd_dice_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Dice  devices.
 */
void hinawa_snd_dice_open(HinawaSndDice *self, gchar *path, GError **exception)
{
	HinawaSndDicePrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_DICE(self));
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_dice_get_instance_private(self);

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	priv->req = g_object_new(HINAWA_TYPE_FW_REQ, NULL);

	priv->waiters = NULL;
	g_mutex_init(&priv->mutex);
}

/**
 * hinawa_snd_dice_transaction:
 * @self: A #HinawaSndDice
 * @addr: A destination address of target device
 * @frame: (array length=frame_count)(in): An array with elements for quadlet
 *	   data to transmit.
 * @frame_count: The number of quadlets in the frame.
 * @bit_flag: bit flag to wait
 * @exception: A #GError
 *
 * Execute write transactions to the given address, then wait and check
 * notification.
 *
 * Since: 1.4.
 */
void hinawa_snd_dice_transaction(HinawaSndDice *self, guint64 addr,
			         const guint32 *frame, gsize frame_count,
				 guint32 bit_flag, GError **exception)
{
	HinawaSndDicePrivate *priv;
	HinawaFwTcode tcode;
	gsize length;
	guint8 *req_frame;
	HinawaFwNode *node;
	gint i;

	struct notification_waiter waiter = {0};
	gint64 expiration;

	g_return_if_fail(HINAWA_IS_SND_DICE(self));
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_dice_get_instance_private(self);

	if (frame_count == 1)
		tcode = HINAWA_FW_TCODE_WRITE_QUADLET_REQUEST;
	else
		tcode = HINAWA_FW_TCODE_WRITE_BLOCK_REQUEST;

	// Alignment data on given buffer to local buffer for transaction.
	length = frame_count * sizeof(*frame);
	req_frame = g_malloc0(length);
	for (i = 0; i < frame_count; ++i) {
		__be32 quad = GUINT32_TO_BE(frame[i]);
		memcpy(req_frame + i * sizeof(quad), &quad, sizeof(quad));
	}

	// This predicates against suprious wakeup.
	waiter.awakened = FALSE;
	g_cond_init(&waiter.cond);
	g_mutex_lock(&priv->mutex);

	// Insert this entry to list and enter critical section.
	waiter.bit_flag = bit_flag;
	priv->waiters = g_list_append(priv->waiters, &waiter);

	expiration = g_get_monotonic_time() + 200 * G_TIME_SPAN_MILLISECOND;

	// NOTE: I believe that a pair of this action/subaction is done within
	// default timeout of HinawaFwReq.
	hinawa_snd_unit_get_node(&self->parent_instance, &node);
	hinawa_fw_req_transaction(priv->req, node, tcode, addr, length,
				  &req_frame, &length, exception);
	g_free(req_frame);
	if (*exception)
		goto end;

	while (!waiter.awakened) {
		if (!g_cond_wait_until(&waiter.cond, &priv->mutex, expiration))
			break;
	}
	if (!waiter.awakened)
		raise(exception, ETIMEDOUT);
end:
	priv->waiters = g_list_remove(priv->waiters, (gpointer *)&waiter);

	g_mutex_unlock(&priv->mutex);
	g_cond_clear(&waiter.cond);
}

void hinawa_snd_dice_handle_notification(HinawaSndDice *self,
					 const void *buf, ssize_t len)
{
	const struct snd_firewire_event_dice_notification *event =
			(struct snd_firewire_event_dice_notification *)buf;
	HinawaSndDicePrivate *priv;
	GList *entry;
	struct notification_waiter *waiter;

	g_return_if_fail(HINAWA_IS_SND_DICE(self));
	priv = hinawa_snd_dice_get_instance_private(self);

	g_mutex_lock(&priv->mutex);

	for (entry = priv->waiters; entry != NULL; entry = entry->next) {
		waiter = (struct notification_waiter *)entry->data;
		if (waiter->bit_flag & event->notification) {
			waiter->awakened = TRUE;
			g_cond_signal(&waiter->cond);
		}
	}

	g_mutex_unlock(&priv->mutex);

	g_signal_emit(self, dice_sigs[DICE_SIG_TYPE_NOTIFIED],
		      0, event->notification);
}
