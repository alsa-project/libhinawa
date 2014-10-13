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

HinawaEfwTransaction *hinawa_efw_transaction_create(int *err)
{
	HinawaEfwTransaction *trans;

	hinawa_malloc(trans, sizeof(HinawaEfwTransaction), err);
	if (*err)
		return NULL;

	hinawa_malloc(trans->buf, MAXIMUM_FRAME_BYTES, err);
	if (*err) {
		hinawa_free(trans);
		return NULL;
	}
	trans->length = MAXIMUM_FRAME_BYTES;

	return trans;
}

void hinawa_efw_transaction_run(HinawaEfwTransaction *trans,
				HinawaSndUnit *unit, unsigned int category,
				unsigned int command,
				uint32_t *args, unsigned int args_count,
				uint32_t *params, unsigned int *params_count,
				int *err)
{
	struct snd_efw_transaction *frame =
					(struct snd_efw_transaction *)trans->buf;
	unsigned int quadlets, i;
	struct timespec abstime;

	if (unit->type != SNDRV_FIREWIRE_TYPE_FIREWORKS) {
		*err = EINVAL;
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

void handle_efw_response(HinawaSndUnit *unit, void *buf, unsigned int length,
			 int *err)
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

void hinawa_efw_transaction_destroy(HinawaEfwTransaction *trans)
{
	hinawa_free(trans->buf);
	hinawa_free(trans);
}
