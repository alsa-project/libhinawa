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

/**
 * hinawa_snd_efw_error_quark:
 *
 * Return the GQuark for error domain of GError which has code in #HinawaSndEfwStatus.
 *
 * Returns: A #GQuark.
 */
G_DEFINE_QUARK(hinawa-snd-efw-error-quark, hinawa_snd_efw_error)

#define MINIMUM_SUPPORTED_VERSION	1
#define MAXIMUM_FRAME_BYTES		0x200U

static const char *const efw_status_names[] = {
	[HINAWA_SND_EFW_STATUS_OK]		= "The transaction finishes successfully",
	[HINAWA_SND_EFW_STATUS_BAD]		= "The request or response includes invalid header",
	[HINAWA_SND_EFW_STATUS_BAD_COMMAND]	= "The request includes invalid category or command",
	[HINAWA_SND_EFW_STATUS_COMM_ERR]	= "The transaction fails due to communication error",
	[HINAWA_SND_EFW_STATUS_BAD_QUAD_COUNT]	= "The number of quadlets in transaction is invalid",
	[HINAWA_SND_EFW_STATUS_UNSUPPORTED]	= "The request is not supported",
	[HINAWA_SND_EFW_STATUS_TIMEOUT]		= "The transaction is canceled due to response timeout",
	[HINAWA_SND_EFW_STATUS_DSP_TIMEOUT]	= "The operation for DSP did not finish within timeout",
	[HINAWA_SND_EFW_STATUS_BAD_RATE]	= "The request includes invalid value for sampling frequency",
	[HINAWA_SND_EFW_STATUS_BAD_CLOCK]	= "The request includes invalid value for source of clock",
	[HINAWA_SND_EFW_STATUS_BAD_CHANNEL]	= "The request includes invalid value for the number of channel",
	[HINAWA_SND_EFW_STATUS_BAD_PAN]		= "The request includes invalid value for panning",
	[HINAWA_SND_EFW_STATUS_FLASH_BUSY]	= "The on-board flash is busy and not operable",
	[HINAWA_SND_EFW_STATUS_BAD_MIRROR]	= "The request includes invalid value for mirroring channel",
	[HINAWA_SND_EFW_STATUS_BAD_LED]		= "The request includes invalid value for LED",
	[HINAWA_SND_EFW_STATUS_BAD_PARAMETER]	= "The request includes invalid value of parameter",
	[HINAWA_SND_EFW_STATUS_LARGE_RESP]	= "The size of response is larger than expected",
};

#define generate_local_error(exception, code)							\
	g_set_error_literal(exception, HINAWA_SND_EFW_ERROR, code, efw_status_names[code])

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
 * @exception: A #GError. Error can be generated with three domains; #g_file_error_quark(),
 *	       #hinawa_fw_node_error_quark(), and #hinawa_snd_unit_error_quark().
 *
 * Open ALSA hwdep character device and check it for Fireworks devices.
 */
void hinawa_snd_efw_open(HinawaSndEfw *self, gchar *path, GError **exception)
{
	HinawaSndEfwPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

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
 * @exception: A #GError. Error can be generated with three domains; #hinawa_snd_unit_error_quark(),
 *	       and #hinawa_snd_efw_error_quark().
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
	g_return_if_fail(params != NULL);
	g_return_if_fail(param_count != NULL && *param_count > 0);
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_efw_get_instance_private(self);

	trans.frame = g_malloc0(MAXIMUM_FRAME_BYTES);

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
		generate_local_error(exception, HINAWA_SND_EFW_STATUS_TIMEOUT);
		goto end;
	}

	// Check transaction status.
	status = GUINT32_FROM_BE(trans.frame->status);
	if (status != HINAWA_SND_EFW_STATUS_OK) {
		if (status > HINAWA_SND_EFW_STATUS_BAD_PARAMETER)
			status = HINAWA_SND_EFW_STATUS_BAD;
		generate_local_error(exception, status);
		goto end;
	}

	// Check transaction headers.
	if (GUINT32_FROM_BE(trans.frame->version) < MINIMUM_SUPPORTED_VERSION ||
	    GUINT32_FROM_BE(trans.frame->category) != category ||
	    GUINT32_FROM_BE(trans.frame->command) != command) {
		generate_local_error(exception, HINAWA_SND_EFW_STATUS_BAD);
		goto end;
	}

	// Check size.
	quads = GUINT32_FROM_BE(trans.frame->length) - sizeof(*trans.frame) / 4;
	if (quads > *param_count) {
		generate_local_error(exception, HINAWA_SND_EFW_STATUS_LARGE_RESP);
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
