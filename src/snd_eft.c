#include <linux/types.h>
#include <sound/firewire.h>
#include <alsa/asoundlib.h>
#include "snd_eft.h"

#define MINIMUM_SUPPORTED_VERSION	1
#define MAXIMUM_FRAME_BYTES		0x200U

enum eft_status {
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
static const char *const eft_status_names[] = {
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

struct _HinawaSndEftPrivate {
	HinawaSndUnit *unit;
	guint seqnum;

	GList *transactions;
	GMutex lock;
};
G_DEFINE_TYPE_WITH_PRIVATE (HinawaSndEft, hinawa_snd_eft, G_TYPE_OBJECT)
#define SND_EFT_GET_PRIVATE(obj)					\
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), 				\
				     HINAWA_TYPE_SND_EFT, HinawaSndEftPrivate))

static void handle_response(void *private_data,
			    const void *buf, unsigned int len);

static void snd_eft_dispose(GObject *obj)
{
	HinawaSndEft *self = HINAWA_SND_EFT(obj);
	HinawaSndEftPrivate *priv = SND_EFT_GET_PRIVATE(self);

	if (priv->unit != NULL)
		hinawa_snd_unit_remove_handle(priv->unit, handle_response);

	G_OBJECT_CLASS (hinawa_snd_eft_parent_class)->dispose(obj);
}

static void snd_eft_finalize (GObject *gobject)
{
	G_OBJECT_CLASS(hinawa_snd_eft_parent_class)->finalize(gobject);
}

static void hinawa_snd_eft_class_init(HinawaSndEftClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = NULL;
	gobject_class->set_property = NULL;
	gobject_class->dispose = snd_eft_dispose;
	gobject_class->finalize = snd_eft_finalize;
}

static void hinawa_snd_eft_init(HinawaSndEft *self)
{
	self->priv = hinawa_snd_eft_get_instance_private(self);
}

HinawaSndEft *hinawa_snd_eft_new(HinawaSndUnit *unit, GError **exception)
{
	HinawaSndEft *self;
	HinawaSndEftPrivate *priv;

	self = g_object_new(HINAWA_TYPE_SND_EFT, NULL);
	if (self == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}

	hinawa_snd_unit_add_handle(unit, SNDRV_FIREWIRE_TYPE_FIREWORKS,
				   handle_response, self, exception);
	if (*exception != NULL) {
		g_clear_object(&self);
		return NULL;
	}

	priv = SND_EFT_GET_PRIVATE(self);
	priv->unit = unit;
	priv->seqnum = 0;
	priv->transactions = NULL;
	g_mutex_init(&priv->lock);

	return self;
}

/**
 * hinawa_snd_eft_transact:
 * @self: A #HinawaSndEft
 * @category: one of category for the transact
 * @command: one of commands for the transact
 * @args: (nullable) (element-type guint32) (array) (in): arguments for the
 *        transaction
 * @params: (element-type guint32) (array) (out caller-allocates): return params
 * @exception: A #GError
 */
void hinawa_snd_eft_transact(HinawaSndEft *self, guint category, guint command,
			     GArray *args, GArray *params, GError **exception)
{
	HinawaSndEftPrivate *priv = SND_EFT_GET_PRIVATE(self);
	unsigned int type;

	struct efw_transaction trans;
	__le32 *items;

	unsigned int quads;
	unsigned int count;
	unsigned int i;

	gint64 expiration;

	/* Check unit type and function arguments . */
	g_object_get(G_OBJECT(priv->unit), "iface", &type, NULL);
	if ((type != SNDRV_FIREWIRE_TYPE_FIREWORKS) ||
	    (args && g_array_get_element_size(args) != sizeof(guint32)) ||
	    (params && g_array_get_element_size(params) != sizeof(guint32))) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EINVAL, "%s", strerror(EINVAL));
		return;
	}

	trans.frame = g_malloc0(MAXIMUM_FRAME_BYTES);
	if (trans.frame == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
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
	trans.frame->status   	= 0xff;
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
	hinawa_snd_unit_write(priv->unit, trans.frame, quads * 4, exception);
	if (*exception != NULL)
		goto end;

	/*
	 * Wait corresponding response till timeout and temporarily leave the
	 * critical section.
	 */
	if (!g_cond_wait_until(&trans.cond, &priv->lock, expiration)) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ETIMEDOUT, "%s", strerror(ETIMEDOUT));
		goto end;
	}

	quads = be32toh(trans.frame->length);

	/* The transactions are aligned to big-endian. */
	items = (__le32 *)trans.frame;
	for (i = 0; i < quads; i++)
		items[i] = be32toh(items[i]);

	/* Check transaction status. */
	if (trans.frame->status != EFT_STATUS_OK) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EPROTO, "%s",
			    eft_status_names[trans.frame->status]);
		goto end;
	}

	/* Check transaction headers. */
	if ((trans.frame->version  <  MINIMUM_SUPPORTED_VERSION) ||
	    (trans.frame->category != category) ||
	    (trans.frame->command  != command)) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EIO, "%s", strerror(EIO));
		goto end;
	}

	/* Check returned parameters. */
	count = quads - sizeof(struct snd_efw_transaction) / 4;
	if (count > 0 && params == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EINVAL, "%s", strerror(EINVAL));
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

static void handle_response(void *private_data,
			    const void *buf, unsigned int len)
{
	HinawaSndEftPrivate *priv = SND_EFT_GET_PRIVATE(private_data);
	struct snd_firewire_event_efw_response *event =
				(struct snd_firewire_event_efw_response *)buf;
	guint *responses = event->response;

	struct snd_efw_transaction *resp_frame;
	struct efw_transaction *trans;

	unsigned int quadlets;
	GList *entry;

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

		if (trans == NULL)
			continue;

		quadlets = be32toh(resp_frame->length);
		memcpy(trans->frame, resp_frame, quadlets * 4);
		g_cond_signal(&trans->cond);

		responses += quadlets;
		len -= quadlets * sizeof(guint);
	}
}
