/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"

/**
 * SECTION:snd_efw
 * @Title: HinawaSndEfw
 * @Short_description: A transaction executor for Fireworks models
 * @include: snd_efw.h
 *
 * A #HinawaSndEfw is an application of Echo Fireworks Transaction (EFT).
 * This inherits #HinawaSndUnit.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaSndEfw", hinawa_snd_efw)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_efw_quark(), errno,		\
		    "%d: %s", __LINE__, strerror(errno))

#define MINIMUM_SUPPORTED_VERSION	1
#define MAXIMUM_FRAME_BYTES		0x200U

enum efw_status {
	EFT_STATUS_OK			= 0,
	EFT_STATUS_BAD			= 1,
	EFT_STATUS_BAD_COMMAND		= 2,
	EFT_STATUS_COMM_ERR		= 3,
	EFT_STATUS_BAD_QUAD_COUNT	= 4,
	EFT_STATUS_UNSUPPORTED		= 5,
	EFT_STATUS_1394_TIMEOUT		= 6,
	EFT_STATUS_DSP_TIMEOUT		= 7,
	EFT_STATUS_BAD_RATE		= 8,
	EFT_STATUS_BAD_CLOCK		= 9,
	EFT_STATUS_BAD_CHANNEL		= 10,
	EFT_STATUS_BAD_PAN		= 11,
	EFT_STATUS_FLASH_BUSY		= 12,
	EFT_STATUS_BAD_MIRROR		= 13,
	EFT_STATUS_BAD_LED		= 14,
	EFT_STATUS_BAD_PARAMETER	= 15,
};
static const char *const efw_status_names[] = {
	[EFT_STATUS_OK]			= "OK",
	[EFT_STATUS_BAD]		= "bad",
	[EFT_STATUS_BAD_COMMAND]	= "bad command",
	[EFT_STATUS_COMM_ERR]		= "comm err",
	[EFT_STATUS_BAD_QUAD_COUNT]	= "bad quad count",
	[EFT_STATUS_UNSUPPORTED]	= "unsupported",
	[EFT_STATUS_1394_TIMEOUT]	= "1394 timeout",
	[EFT_STATUS_DSP_TIMEOUT]	= "DSP timeout",
	[EFT_STATUS_BAD_RATE]		= "bad rate",
	[EFT_STATUS_BAD_CLOCK]		= "bad clock",
	[EFT_STATUS_BAD_CHANNEL]	= "bad channel",
	[EFT_STATUS_BAD_PAN]		= "bad pan",
	[EFT_STATUS_FLASH_BUSY]		= "flash busy",
	[EFT_STATUS_BAD_MIRROR]		= "bad mirror",
	[EFT_STATUS_BAD_LED]		= "bad LED",
	[EFT_STATUS_BAD_PARAMETER]	= "bad parameter",
};

struct efw_transaction {
	guint seqnum;

	struct snd_efw_transaction *frame;

	GCond cond;
};

struct _HinawaSndEfwPrivate {
	guint seqnum;

	GList *transactions;
	GMutex lock;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndEfw, hinawa_snd_efw, HINAWA_TYPE_SND_UNIT)

static void hinawa_snd_efw_class_init(HinawaSndEfwClass *klass)
{
	return;
}

static void hinawa_snd_efw_init(HinawaSndEfw *self)
{
	return;
}

/**
 * hinawa_snd_efw_new:
 *
 * Instantiate #HinawaSndEfw object and return the instance.
 *
 * Returns: an instance of #HinawaSndEfw.
 * Since: 1.3.
 */
HinawaSndEfw *hinawa_snd_efw_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_EFW, NULL);
}

/**
 * hinawa_snd_efw_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Fireworks devices.
 */
void hinawa_snd_efw_open(HinawaSndEfw *self, gchar *path, GError **exception)
{
	HinawaSndEfwPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	priv = hinawa_snd_efw_get_instance_private(self);

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	priv = hinawa_snd_efw_get_instance_private(self);
	priv->seqnum = 0;
	priv->transactions = NULL;
	g_mutex_init(&priv->lock);
}

/**
 * hinawa_snd_efw_transaction:
 * @self: A #HinawaSndEfw.
 * @category: one of category for the transaction.
 * @command: one of commands for the transaction.
 * @args: (array length=arg_count)(in)(nullable): An array with elements for
 *	  quadlet data as arguments for command.
 * @arg_count: The number of quadlets in the args array.
 * @params: (array length=param_count)(inout): An array with elements for
 *	    quadlet data to save parameters in response. Callers should give it
 *	    for buffer with enough space against the request since this library
 *	    performs no reallocation. Due to the reason, the value of this
 *	    argument should point to the pointer to the array and immutable.
 *	    The content of array is mutable for parameters in response.
 * @param_count: The number of quadlets in the params array.
 * @exception: A #GError.
 *
 * Execute transaction according to Echo Fireworks Transaction protocol.
 *
 * Since: 1.4.
 */
void hinawa_snd_efw_transaction(HinawaSndEfw *self,
				guint category, guint command,
				const guint32 *args, gsize arg_count,
				guint32 *const *params, gsize *param_count,
				GError **exception)
{
	HinawaSndEfwPrivate *priv;
	struct efw_transaction trans;
	unsigned int quads;
	gint64 expiration;
	unsigned int i;
	guint32 status;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	priv = hinawa_snd_efw_get_instance_private(self);

	if (*params == NULL || *param_count == 0) {
		raise(exception, EINVAL);
		return;
	}

	trans.frame = g_malloc0(MAXIMUM_FRAME_BYTES);
	if (trans.frame == NULL) {
		raise(exception, ENOMEM);
		return;
	}

	quads = sizeof(*trans.frame) / 4;
	if (args)
		quads += arg_count;

	// Fill transaction frame.
	trans.frame->seqnum = GUINT32_TO_BE(priv->seqnum);
	trans.frame->length = GUINT32_TO_BE(quads);
	trans.frame->version = GUINT32_TO_BE(MINIMUM_SUPPORTED_VERSION);
	trans.frame->category = GUINT32_TO_BE(category);
	trans.frame->command = GUINT32_TO_BE(command);
	for (i = 0; i < arg_count; ++i)
		trans.frame->params[i] = GUINT32_TO_BE(args[i]);

	// Increment the sequence number for next transaction.
	priv->seqnum += 2;
	if (priv->seqnum > SND_EFW_TRANSACTION_USER_SEQNUM_MAX)
		priv->seqnum = 0;

	// This predicates against suprious wakeup.
	trans.frame->status = 0xffffffff;
	g_mutex_lock(&priv->lock);
	g_cond_init(&trans.cond);

	// Insert this entry to list and enter critical section.
	priv->transactions = g_list_append(priv->transactions, &trans);

	// Send this request frame.
	expiration = g_get_monotonic_time() + 200 * G_TIME_SPAN_MILLISECOND;
	hinawa_snd_unit_write(&self->parent_instance, trans.frame,
			      quads * sizeof(__be32), exception);
	if (*exception != NULL)
		goto end;

	// Wait corresponding response till timeout and temporarily leave the
	// critical section.
	while (trans.frame->status == 0xffffffff) {
		if (!g_cond_wait_until(&trans.cond, &priv->lock, expiration))
			break;
	}
	if (trans.frame->status == 0xffffffff) {
		raise(exception, ETIMEDOUT);
		goto end;
	}

	// Check transaction status.
	status = GUINT32_FROM_BE(trans.frame->status);
	if (status != EFT_STATUS_OK) {
		g_set_error(exception, hinawa_snd_efw_quark(),
			    EPROTO, "%s",
			    efw_status_names[status]);
		goto end;
	}

	// Check transaction headers.
	if (GUINT32_FROM_BE(trans.frame->version) < MINIMUM_SUPPORTED_VERSION ||
	    GUINT32_FROM_BE(trans.frame->category) != category ||
	    GUINT32_FROM_BE(trans.frame->command) != command) {
		raise(exception, EIO);
		goto end;
	}

	// Check size.
	quads = GUINT32_FROM_BE(trans.frame->length) - sizeof(*trans.frame) / 4;
	if (quads > *param_count) {
		raise(exception, ENOBUFS);
		goto end;

	}

	// Copy parameters.
	for (i = 0; i < quads; ++i)
		(*params)[i] = GUINT32_FROM_BE(trans.frame->params[i]);
	*param_count = quads;
end:
	// Remove thie entry from list and leave the critical section.
	priv->transactions =
			g_list_remove(priv->transactions, (gpointer *)&trans);
	g_mutex_unlock(&priv->lock);
	g_cond_clear(&trans.cond);

	g_free(trans.frame);
}

void hinawa_snd_efw_handle_response(HinawaSndEfw *self,
				    const void *buf, ssize_t len)
{
	HinawaSndEfwPrivate *priv;
	struct snd_firewire_event_efw_response *event =
				(struct snd_firewire_event_efw_response *)buf;
	guint *responses = event->response;

	struct snd_efw_transaction *resp_frame;
	struct efw_transaction *trans;

	unsigned int quadlets;
	GList *entry;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	priv = hinawa_snd_efw_get_instance_private(self);

	while (len > 0) {
		resp_frame =  (struct snd_efw_transaction *)responses;

		g_mutex_lock(&priv->lock);

		trans = NULL;
		for (entry = priv->transactions;
		     entry != NULL; entry = entry->next) {
			trans = (struct efw_transaction *)entry->data;

			if (GUINT32_FROM_BE(resp_frame->seqnum) ==
								trans->seqnum)
				break;
		}

		quadlets = GUINT32_FROM_BE(resp_frame->length);
		if (trans != NULL) {
			memcpy(trans->frame, resp_frame, quadlets * 4);
			g_cond_signal(&trans->cond);
		}

		g_mutex_unlock(&priv->lock);

		responses += quadlets;
		len -= quadlets * sizeof(guint);
	}
}
