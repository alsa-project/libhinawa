#include <linux/types.h>
#include "internal.h"

#define MINIMUM_SUPPORTED_VERSION	1
#define MAXIMUM_FRAME_BYTES		0x200U

struct hinawa_efw_transaction {
	uint32_t seqnum;
	void *buf;
	unsigned int length;

	LIST_ENTRY(hinawa_efw_transaction) link;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};

struct _HinawaSndEftPrivate {
	guint32 seqnum;
	void *buf;
	unsigned int len;

	GCond cond;
};
G_DEFINE_TYPE_WITH_PRIVATE (HinawaSndEft, hinawa_snd_eft, G_TYPE_OBJECT)

enum efw_status {
	EFW_STATUS_OK			= 0,
	EFW_STATUS_BAD			= 1,
	EFW_STATUS_BAD_COMMAND		= 2,
	EFW_STATUS_COMM_ERR		= 3,
	EFW_STATUS_BAD_QUAD_COUNT	= 4,
	EFW_STATUS_UNSUPPORTED		= 5,
	EFW_STATUS_1394_TIMEOUT		= 6,
	EFW_STATUS_DSP_TIMEOUT		= 7,
	EFW_STATUS_BAD_RATE		= 8,
	EFW_STATUS_BAD_CLOCK		= 9,
	EFW_STATUS_BAD_CHANNEL		= 10,
	EFW_STATUS_BAD_PAN		= 11,
	EFW_STATUS_FLASH_BUSY		= 12,
	EFW_STATUS_BAD_MIRROR		= 13,
	EFW_STATUS_BAD_LED		= 14,
	EFW_STATUS_BAD_PARAMETER	= 15,
	EFW_STATUS_INCOMPLETE		= 0x80000000
};

static void hinawa_snd_eft_dispose(GObject *gobject)
{
	G_OBJECT_CLASS (hinawa_snd_eft_parent_class)->dispose(gobject);
}

static void hinawa_snd_eft_finalize (GObject *gobject)
{
	HinawaSndEft *self = HINAWA_FW_EFT(gobject);

	G_OBJECT_CLASS(hinawa_snd_eft_parent_class)->finalize(gobject);
}

static void hinawa_snd_eft_class_init(HinawaFwReqClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = NULL;
	gobject_class->set_property = NULL;
	gobject_class->dispose = hinawa_snd_eft_dispose;
	gobject_class->finalize = hinawa_snd_eft_finalize;
}

static void hinawa_snd_eft_init(HinawaFwReq *self)
{
	self->priv = hinawa_snd_eft_get_instance_private(self);
}

HinawaSndEft *hinawa_snd_eft_new(GError **exception)
{
	HinawaSndEft *self;
	void *buf;

	buf = g_malloc0(EFT_MAX_FRAME_BYTES);
	if (buf == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}

	self = g_object_new(HINAWA_TYPE_SND_EFT, NULL);
	if (self == NULL) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    ENOMEM, "%s", strerror(ENOMEM));
		return NULL;
	}
	self->priv->buf = buf;
	self->priv->len = EFT_MAX_FRAME_BYTES;

	return self;
}

void hinawa_snd_eft_transact(HinawaSndEft *self, HinawaSndUnit *unit,
			     unsigned int category, unsigned int command,
			     guint32 *args, unsigned int args_count,
			     guint32 *params, unsigned int *params_count,
			     GError **exception)
{
	struct snd_efw_transaction *self =
				(struct snd_efw_transaction *)self->priv->buf;
	unsigned int type;
	unsigned int quads;
	unsigned int i;
	gint64 expiration;

	g_object_get(G_OBJECT(uint), "type", &type, NULL);

	if (type != SNDRV_FIREWIRE_TYPE_FIREWORKS) {
		g_set_error(exception, g_quark_from_static_string(__func__),
			    EINVAL, "%s", strerror(EINVAL));
		return;
	}

	if (args_count > 0 && args == NULL) {
		*err = EINVAL;
		return;
	}

	trans->seqnum = unit->seqnum;
	unit->seqnum += 2;
	if (unit->seqnum > SND_EFW_TRANSACTION_USER_SEQNUM_MAX)
		unit->seqnum = 0;

	/* copy a request */
	quadlets = sizeof(struct snd_efw_transaction) / sizeof(uint32_t) +
		   args_count;
	frame->length	= htobe32(quadlets);
	frame->version	= htobe32(1);
	frame->seqnum	= htobe32(trans->seqnum);
	frame->category	= htobe32(category);
	frame->command	= htobe32(command);
	for (i = 0; i < args_count; i++)
		frame->params[i] = htobe32(args[i]);

	pthread_mutex_lock(&unit->efw_transactions_lock);
	LIST_INSERT_HEAD(&unit->efw_transactions, trans, link);
	pthread_mutex_unlock(&unit->efw_transactions_lock);

	pthread_cond_init(&trans->cond, NULL);
	pthread_mutex_init(&trans->mutex, NULL);
	abstime.tv_sec = time(NULL) + 2;
	abstime.tv_nsec = 0;

	*err = snd_hwdep_write(unit->hwdep, trans->buf,
			       quadlets * sizeof(uint32_t));
	if (*err < 0) {
		*err *= -1;
		goto end;
	}

	*err = pthread_cond_timedwait(&trans->cond, &trans->mutex, &abstime);
	if (*err)
		goto end;

	/* transaction header includes wrong info. */
	if ((be32toh(frame->version)	<  MINIMUM_SUPPORTED_VERSION) ||
	    (be32toh(frame->category)	!= category) ||
	    (be32toh(frame->command)	!= command)) {
		*err = EPROTO;
		goto end;
	}

	quadlets = be32toh(frame->length) -
		   sizeof(struct snd_efw_transaction) / sizeof(uint32_t);
	if (quadlets > 0 &&
	    (params == NULL || *params_count < quadlets * sizeof(uint32_t))) {
		*err = EINVAL;
		goto end;
	}

	*params_count = quadlets;
	for (i = 0; i < *params_count; i++)
		params[i] = be32toh(frame->params[i]);
end:
	pthread_mutex_lock(&unit->efw_transactions_lock);
	LIST_REMOVE(trans, link);
	pthread_mutex_unlock(&unit->efw_transactions_lock);
}

static void dump_response(struct snd_efw_transaction *trans)
{
	unsigned int quadlets, i;

	printf("Quadlets:\t%d\n",	be32toh(trans->length));
	printf("Version:\t%d\n",	be32toh(trans->version));
	printf("Sequence:\t%d\n",	be32toh(trans->seqnum));
	printf("Category:\t%d\n",	be32toh(trans->category));
	printf("Command:\t%d\n",	be32toh(trans->command));
	printf("Status:\t%d\n",		be32toh(trans->status));

	quadlets = be32toh(trans->length) -
		   sizeof(struct snd_efw_transaction) / sizeof(uint32_t);

	for (i = 0; i < quadlets; i++)
		printf("param[%02d]: %08x\n", i, be32toh(trans->params[i]));
}

void hinawa_snd_eft_handle_response(HinawaSndUnit *unit, void *buf,
				    unsigned int len, int *err)
{
	struct snd_firewire_event_efw_response *event =
				(struct snd_firewire_event_efw_response *)buf;
	uint32_t *responses = event->response;
	struct snd_efw_transaction *resp;
	HinawaEfwTransaction *trans;
	unsigned int quadlets;

	while (length > 0) {
 		resp =  (struct snd_efw_transaction *)responses;
		quadlets = be32toh(resp->length);

		pthread_mutex_lock(&unit->efw_transactions_lock);

		LIST_FOREACH(trans, &unit->efw_transactions, link) {
			if (trans->seqnum + 1 == be32toh(resp->seqnum))
				break;
		}

		if (trans != NULL &&
		    trans->length >= quadlets * sizeof(uint32_t)) {
			memcpy(trans->buf, resp, quadlets * sizeof(uint32_t));
			pthread_cond_signal(&trans->cond);
		} else {
			dump_response(resp);
		}

		pthread_mutex_unlock(&unit->efw_transactions_lock);

		responses += quadlets;
		length -= quadlets * sizeof(uint32_t);
	}
}
