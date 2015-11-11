#include <linux/types.h>
#include <sound/firewire.h>
#include "snd_efw.h"
#include "internal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/**
 * SECTION:snd_efw
 * @Title: HinawaSndEfw
 * @Short_description: A transaction executor for Fireworks models
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
	int type;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	priv = hinawa_snd_efw_get_instance_private(self);

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if (type != SNDRV_FIREWIRE_TYPE_FIREWORKS) {
		raise(exception, EINVAL);
		return;
	}

	priv = hinawa_snd_efw_get_instance_private(self);
	priv->seqnum = 0;
	priv->transactions = NULL;
	g_mutex_init(&priv->lock);
}

/**
 * hinawa_snd_efw_transact:
 * @self: A #HinawaSndEfw
 * @category: one of category for the transact
 * @command: one of commands for the transact
 * @args: (nullable) (element-type guint32) (array) (in): arguments for the
 *        transaction
 * @params: (element-type guint32) (array) (out caller-allocates): return params
 * @exception: A #GError
 */
void hinawa_snd_efw_transact(HinawaSndEfw *self, guint category, guint command,
			     GArray *args, GArray *params, GError **exception)
{
	HinawaSndEfwPrivate *priv;
	int type;

	struct efw_transaction trans;
	__le32 *items;

	unsigned int quads;
	unsigned int count;
	unsigned int i;

	gint64 expiration;

	g_return_if_fail(HINAWA_IS_SND_EFW(self));
	priv = hinawa_snd_efw_get_instance_private(self);

	/* Check unit type and function arguments . */
	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if ((type != SNDRV_FIREWIRE_TYPE_FIREWORKS) ||
	    (args && g_array_get_element_size(args) != sizeof(guint32)) ||
	    (params && g_array_get_element_size(params) != sizeof(guint32))) {
		raise(exception, EINVAL);
		return;
	}

	trans.frame = g_malloc0(MAXIMUM_FRAME_BYTES);
	if (trans.frame == NULL) {
		raise(exception, ENOMEM);
		return;
	}

	/* Increment the sequence number for next transaction. */
	trans.frame->seqnum = priv->seqnum;
	priv->seqnum += 2;
	if (priv->seqnum > SND_EFW_TRANSACTION_USER_SEQNUM_MAX)
		priv->seqnum = 0;

	/* Fill transaction frame. */
	quads = sizeof(struct snd_efw_transaction) / 4;
	if (args)
		quads += args->len;
	trans.frame->length	= quads;
	trans.frame->version	= MINIMUM_SUPPORTED_VERSION;
	trans.frame->category	= category;
	trans.frame->command	= command;
	trans.frame->status	= 0xff;
	if (args)
		memcpy(trans.frame->params,
		       args->data, args->len * sizeof(guint32));

	/* The transactions are aligned to big-endian. */
	items = (__le32 *)trans.frame;
	for (i = 0; i < quads; i++)
		items[i] = htobe32(items[i]);

	/* Insert this entry to list and enter critical section. */
	g_mutex_lock(&priv->lock);
	priv->transactions = g_list_append(priv->transactions, &trans);

	/* NOTE: Timeout is 200 milli-seconds. */
	expiration = g_get_monotonic_time() + 200 * G_TIME_SPAN_MILLISECOND;
	g_cond_init(&trans.cond);

	/* Send this request frame. */
	hinawa_snd_unit_write(&self->parent_instance, trans.frame, quads * 4,
			      exception);
	if (*exception != NULL)
		goto end;

	/*
	 * Wait corresponding response till timeout and temporarily leave the
	 * critical section.
	 */
	if (!g_cond_wait_until(&trans.cond, &priv->lock, expiration)) {
		raise(exception, ETIMEDOUT);
		goto end;
	}

	quads = be32toh(trans.frame->length);

	/* The transactions are aligned to big-endian. */
	items = (__le32 *)trans.frame;
	for (i = 0; i < quads; i++)
		items[i] = be32toh(items[i]);

	/* Check transaction status. */
	if (trans.frame->status != EFT_STATUS_OK) {
		g_set_error(exception, hinawa_snd_efw_quark(),
			    EPROTO, "%s",
			    efw_status_names[trans.frame->status]);
		goto end;
	}

	/* Check transaction headers. */
	if ((trans.frame->version  <  MINIMUM_SUPPORTED_VERSION) ||
	    (trans.frame->category != category) ||
	    (trans.frame->command  != command)) {
		raise(exception, EIO);
		goto end;
	}

	/* Check returned parameters. */
	count = quads - sizeof(struct snd_efw_transaction) / 4;
	if (count > 0 && params == NULL) {
		raise(exception, EINVAL);
		goto end;
	}

	/* Copy parameters. */
	g_array_insert_vals(params, 0, trans.frame->params, count);
end:
	/* Remove thie entry from list and leave the critical section. */
	priv->transactions =
			g_list_remove(priv->transactions, (gpointer *)&trans);
	g_mutex_unlock(&priv->lock);

	g_free(trans.frame);
}

void hinawa_snd_efw_handle_response(HinawaSndEfw *self,
				    const void *buf, unsigned int len)
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

			if (be32toh(resp_frame->seqnum) == trans->seqnum)
				break;
		}

		g_mutex_unlock(&priv->lock);

		quadlets = be32toh(resp_frame->length);
		if (trans != NULL) {
			memcpy(trans->frame, resp_frame, quadlets * 4);
			g_cond_signal(&trans->cond);
		}

		responses += quadlets;
		len -= quadlets * sizeof(guint);
	}
}
