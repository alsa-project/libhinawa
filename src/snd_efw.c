/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "hinawa_sigs_marshal.h"

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

struct _HinawaSndEfwPrivate {
	guint seqnum;
	GMutex lock;
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndEfw, hinawa_snd_efw, HINAWA_TYPE_SND_UNIT)

enum efw_sig_type {
	EFW_SIG_TYPE_RESPONDED = 1,
	EFW_SIG_TYPE_COUNT,
};
static guint efw_sigs[EFW_SIG_TYPE_COUNT] = { 0 };

static void hinawa_snd_efw_class_init(HinawaSndEfwClass *klass)
{
	/**
	 * HinawaSndEfw::responded:
	 * @self: A #HinawaSndEfw.
	 * @status: One of #HinawaSndEfwStatus.
	 * @seqnum: The sequence number of response.
	 * @category: The value of category field in the response.
	 * @command: The value of command field in the response.
	 * @frame: (array length=frame_size)(element-type guint32): The array with elements for
	 *	   quadlet data of response for Echo Fireworks protocol.
	 * @frame_size: The number of elements of the array.
	 *
	 * When the unit transfers asynchronous packet as response for Echo Audio Fireworks
	 * protocol, and the process successfully reads the content of response from ALSA
	 * Fireworks driver, the #HinawaSndEfw::responded signal handler is called with parameters
	 * of the response.
	 */
	efw_sigs[EFW_SIG_TYPE_RESPONDED] =
		g_signal_new("responded",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaSndEfwClass, responded),
			     NULL, NULL,
			     hinawa_sigs_marshal_VOID__ENUM_UINT_UINT_UINT_POINTER_UINT,
			     G_TYPE_NONE,
			     6, HINAWA_TYPE_SND_EFW_STATUS, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_UINT);
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
	g_mutex_init(&priv->lock);
}

/**
 * hinawa_snd_efw_transaction_async:
 * @self: A #HinawaSndEfw.
 * @category: One of category for the transaction.
 * @command: One of commands for the transaction.
 * @args: (array length=arg_count)(in)(nullable): An array with elements for quadlet data as
 *	  arguments for command.
 * @arg_count: The number of quadlets in the args array.
 * @resp_seqnum: (out): The sequence number for response transaction;
 * @exception: A #GError. Error can be generated with domain of #hinawa_snd_unit_error_quark().
 *
 * Transfer asynchronous transaction for command frame of Echo Fireworks protocol. When receiving
 * asynchronous transaction for response frame, #HinawaSndEfw::responded GObject signal is emitted.
 *
 * Since: 2.1.
 */
void hinawa_snd_efw_transaction_async(HinawaSndEfw *self, guint category, guint command,
				      const guint32 *args, gsize arg_count, guint32 *resp_seqnum,
				      GError **exception)
{
	HinawaSndEfwPrivate *priv;
	struct snd_efw_transaction *frame;
	unsigned int quads;
	int i;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	g_return_if_fail(sizeof(*args) * arg_count + sizeof(*frame) < MAXIMUM_FRAME_BYTES);
	g_return_if_fail(resp_seqnum != NULL);
	g_return_if_fail(exception == NULL || *exception == NULL);

	priv = hinawa_snd_efw_get_instance_private(self);

	quads = sizeof(*frame) / 4;
	if (args != NULL)
		quads += arg_count;

	frame = g_malloc0(sizeof(guint32) * quads);

	// Fill request frame for transaction.
	frame->length = GUINT32_TO_BE(quads);
	frame->version = GUINT32_TO_BE(MINIMUM_SUPPORTED_VERSION);
	frame->category = GUINT32_TO_BE(category);
	frame->command = GUINT32_TO_BE(command);
	if (args != NULL) {
		for (i = 0; i < arg_count; ++i)
			frame->params[i] = GUINT32_TO_BE(args[i]);
	}

	// Increment the sequence number for next transaction.
	g_mutex_lock(&priv->lock);
	frame->seqnum = GUINT32_TO_BE(priv->seqnum);
	*resp_seqnum = priv->seqnum + 1;
	priv->seqnum += 2;
	if (priv->seqnum > SND_EFW_TRANSACTION_USER_SEQNUM_MAX)
		priv->seqnum = 0;
	g_mutex_unlock(&priv->lock);

	// Send this request frame.
	hinawa_snd_unit_write(&self->parent_instance, frame, quads * sizeof(__be32), exception);

	g_free(frame);
}

struct waiter {
	guint32 seqnum;

	guint32 category;
	guint32 command;
	HinawaSndEfwStatus status;
	guint32 *params;
	gsize param_count;

	GCond cond;
	GMutex mutex;
};

static void handle_responded_signal(HinawaSndEfw *self, HinawaSndEfwStatus status, guint32 seqnum,
				    guint category, guint command,
				    const guint32 *params, guint32 param_count, gpointer user_data)
{
	struct waiter *w = (struct waiter *)user_data;

	if (seqnum == w->seqnum) {
		g_mutex_lock(&w->mutex);

		if (category != w->category || command != w->command)
			status = HINAWA_SND_EFW_STATUS_BAD;
		w->status = status;

		if (param_count > 0 && param_count <= w->param_count)
			memcpy(w->params, params, param_count * sizeof(*params));
		w->param_count = param_count;

		g_cond_signal(&w->cond);

		g_mutex_unlock(&w->mutex);
	}
}

/**
 * hinawa_snd_efw_transaction_sync:
 * @self: A #HinawaSndEfw.
 * @category: one of category for the transaction.
 * @command: one of commands for the transaction.
 * @args: (array length=arg_count)(in)(nullable): An array with elements for
 *	  quadlet data as arguments for command.
 * @arg_count: The number of quadlets in the args array.
 * @params: (array length=param_count)(inout)(nullable): An array with elements for
 *	    quadlet data to save parameters in response. Callers should give it
 *	    for buffer with enough space against the request since this library
 *	    performs no reallocation. Due to the reason, the value of this
 *	    argument should point to the pointer to the array and immutable.
 *	    The content of array is mutable for parameters in response.
 * @param_count: The number of quadlets in the params array.
 * @timeout_ms: The timeout to wait for response of the transaction since request is transferred in
 *		milliseconds.
 * @exception: A #GError. Error can be generated with three domains; #hinawa_snd_unit_error_quark(),
 *	       and #hinawa_snd_efw_error_quark().
 *
 * Transfer asynchronous transaction for command frame of Echo Fireworks protocol, then wait
 * asynchronous transaction for response frame within the given timeout.
 *
 * Since: 2.1.
 */
void hinawa_snd_efw_transaction_sync(HinawaSndEfw *self, guint category, guint command,
				     const guint32 *args, gsize arg_count,
				     guint32 *const *params, gsize *param_count,
				     guint timeout_ms, GError **exception)
{
	gulong handler_id;
	struct waiter w;
	guint64 expiration;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	g_return_if_fail(param_count != NULL);
	g_return_if_fail(exception == NULL || *exception == NULL);

	// This predicates against suprious wakeup.
	w.status = 0xffffffff;
	w.category = category;
	w.command = command;
	if (*param_count > 0)
		w.params = *params;
	else
		w.params = NULL;
	w.param_count = *param_count;
	g_cond_init(&w.cond);
	g_mutex_init(&w.mutex);

	handler_id = g_signal_connect(self, "responded", (GCallback)handle_responded_signal, &w);

	// Timeout is set in advance as a parameter of this object.
	expiration = g_get_monotonic_time() + timeout_ms * G_TIME_SPAN_MILLISECOND;

	hinawa_snd_efw_transaction_async(self, category, command, args, arg_count, &w.seqnum,
					 exception);
	if (*exception != NULL) {
		g_signal_handler_disconnect(self, handler_id);
		goto end;
	}

	g_mutex_lock(&w.mutex);
	while (w.status == 0xffffffff) {
		// Wait for a response with timeout, waken by the response handler.
		if (!g_cond_wait_until(&w.cond, &w.mutex, expiration))
			break;
	}
	g_signal_handler_disconnect(self, handler_id);
	g_mutex_unlock(&w.mutex);

	if (w.status == 0xffffffff)
		generate_local_error(exception, HINAWA_SND_EFW_STATUS_TIMEOUT);
	else if (w.status != HINAWA_SND_EFW_STATUS_OK)
		generate_local_error(exception, w.status);
	else if (w.param_count > *param_count)
		generate_local_error(exception, HINAWA_SND_EFW_STATUS_LARGE_RESP);
	else
		*param_count = w.param_count;
end:
	g_cond_clear(&w.cond);
	g_mutex_clear(&w.mutex);
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
 * Transfer request of transaction according to Echo Fireworks Transaction protocol, then wait for
 * the response of transaction within 200 millisecond timeout.
 *
 * Since: 1.4.
 */
void hinawa_snd_efw_transaction(HinawaSndEfw *self,
				guint category, guint command,
				const guint32 *args, gsize arg_count,
				guint32 *const *params, gsize *param_count,
				GError **exception)
{
	hinawa_snd_efw_transaction_sync(self, category, command, args, arg_count,
					params, param_count, 200, exception);
}

void hinawa_snd_efw_handle_response(HinawaSndEfw *self,
				    const void *buf, ssize_t len)
{
	struct snd_firewire_event_efw_response *event =
				(struct snd_firewire_event_efw_response *)buf;
	guint32 *responses = event->response;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));

	while (len > 0) {
		struct snd_efw_transaction *frame =  (struct snd_efw_transaction *)responses;
		guint32 quadlets;
		guint32 seqnum;
		HinawaSndEfwStatus status;
		guint category;
		guint command;
		guint param_count;
		int i;

		quadlets = GUINT32_FROM_BE(frame->length);
		seqnum = GUINT32_FROM_BE(frame->seqnum);

		status = GUINT32_FROM_BE(frame->status);
		if (status > HINAWA_SND_EFW_STATUS_BAD_PARAMETER)
			status = HINAWA_SND_EFW_STATUS_BAD;

		category = GUINT32_FROM_BE(frame->category);
		command = GUINT32_FROM_BE(frame->command);

		param_count = quadlets - sizeof(*frame) / sizeof(guint32);
		for (i = 0; i < param_count; ++i)
			frame->params[i] = GUINT32_FROM_BE(frame->params[i]);

		g_signal_emit(self, efw_sigs[EFW_SIG_TYPE_RESPONDED], 0,
			      status, seqnum, category, command, frame->params, param_count);

		responses += quadlets;
		len -= quadlets * sizeof(guint);
	}
}
