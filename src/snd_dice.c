// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>

/**
 * HinawaSndDice:
 * A notification listener for Dice models.
 *
 * A [class@SndDice] listen to Dice notification and generates signal when received.
 *
 * Deprecated: 2.5. libhitaki library provides [class@Hitaki.SndDice] as the alternative.
 */

/**
 * hinawa_snd_dice_error_quark:
 *
 * Return the [alias@GLib.Quark] for [struct@GLib.Error] which has code in Hinawa.SndDiceError.
 *
 * Since: 2.1
 * Deprecated: 2.5.
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinawa-snd-dice-error-quark, hinawa_snd_dice_error)

const char *const err_msgs[] = {
	[HINAWA_SND_DICE_ERROR_TIMEOUT] = "The transaction is canceled due to response timeout",
};

#define generate_local_error(error, code)					\
	g_set_error_literal(error, HINAWA_SND_DICE_ERROR, code, err_msgs[code])

struct notification_waiter {
	guint32 bit_flag;
	GCond cond;
	gboolean awakened;
};

typedef struct {
	HinawaFwReq *req;
	GList *waiters;
	GMutex mutex;
} HinawaSndDicePrivate;
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
	 * @self: A [class@SndDice]
	 * @message: A notification message
	 *
	 * Emitted when Dice unit transfers notification.
	 *
	 * Since: 0.3
	 * Deprecated: 2.5. Use implementation of [class@Hitaki.SndDice] for
	 *	       [signal@Hitaki.QuadletNotification::notified] instead.
	 */
	dice_sigs[DICE_SIG_TYPE_NOTIFIED] =
		g_signal_new("notified",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
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
 * Instantiate [class@SndDice] object and return the instance.
 *
 * Returns: an instance of [class@SndDice].
 *
 * Since: 1.3.
 * Deprecated: 2.5. Use [method@Hitaki.SndDice.new] instead.
 */
HinawaSndDice *hinawa_snd_dice_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_DICE, NULL);
}

/**
 * hinawa_snd_dice_open:
 * @self: A [class@SndDice]
 * @path: A full path of a special file for ALSA hwdep character device
 * @error: A [struct@GLib.Error]. Error can be generated with three domains; GLib.FileError,
 *	   Hinawa.FwNodeError, and Hinawa.SndUnitError.
 *
 * Open ALSA hwdep character device and check it for Dice devices.
 *
 * Since: 0.4
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.open] in
 *	       [class@Hitaki.SndDice] instead.
 */
void hinawa_snd_dice_open(HinawaSndDice *self, gchar *path, GError **error)
{
	HinawaSndDicePrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_DICE(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_snd_dice_get_instance_private(self);

	hinawa_snd_unit_open(&self->parent_instance, path, error);
	if (*error != NULL)
		return;

	priv->req = g_object_new(HINAWA_TYPE_FW_REQ, NULL);

	priv->waiters = NULL;
	g_mutex_init(&priv->mutex);
}

/**
 * hinawa_snd_dice_transaction:
 * @self: A [class@SndDice]
 * @addr: A destination address of target device
 * @frame: (array length=frame_count)(in): An array with elements for quadlet data to transmit.
 * @frame_count: The number of quadlets in the frame.
 * @bit_flag: bit flag to wait
 * @error: A [struct@GLib.Error]. Error can be generated with three domains; Hinawa.FwNodeError,
 *	   Hinawa.FwReqError, and Hinawa.SndDiceError.
 *
 * Execute write transactions to the given address, then wait and check notification.
 *
 * Since: 1.4.
 * Deprecated: 2.5. Use [class@FwReq] to send write request transaction to the unit, then use
 *		    implementaion of [signal@Hitaki.QuadletNotification::notified] in
 *		    [class@Hitaki.SndDice] to wait for notification.
 */
void hinawa_snd_dice_transaction(HinawaSndDice *self, guint64 addr,
			         const guint32 *frame, gsize frame_count,
				 guint32 bit_flag, GError **error)
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
	g_return_if_fail(frame != NULL);
	g_return_if_fail(frame_count > 0);
	g_return_if_fail(error == NULL || *error == NULL);

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
				  &req_frame, &length, error);
	g_free(req_frame);
	if (*error)
		goto end;

	while (!waiter.awakened) {
		if (!g_cond_wait_until(&waiter.cond, &priv->mutex, expiration))
			break;
	}
	if (!waiter.awakened)
		generate_local_error(error, HINAWA_SND_DICE_ERROR_TIMEOUT);
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
