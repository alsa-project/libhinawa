#include "internal.h"

struct hinawa_fcp_transaction {
	uint8_t *cmd;
	uint8_t *resp;
	unsigned int length;

	LIST_ENTRY(hinawa_fcp_transaction) link;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};

enum fcp_type {
	FCP_TYPE_CONTROL		= 0x00,
	FCP_TYPE_STATUS			= 0x01,
	FCP_TYPE_SPECIFIC_INQUIRY	= 0x02,
	FCP_TYPE_NOTIFY			= 0x03,
	FCP_TYPE_GENERAL_INQUIRY	= 0x04,
	/* 0x05-0x07 are reserved. */
};
/* continue */
enum fcp_status {
	FCP_STATUS_NOT_IMPLEMENTED	= 0x08,
	FCP_STATUS_ACCEPTED		= 0x09,
	FCP_STATUS_REJECTED		= 0x0a,
	FCP_STATUS_IN_TRANSITION	= 0x0b,
	FCP_STATUS_IMPLEMENTED_STABLE	= 0x0c,
	FCP_STATUS_CHANGED		= 0x0d,
	/* reserved */
	FCP_STATUS_INTERIM		= 0x0f,
};

HinawaFcpTransaction *hinawa_fcp_transaction_create(int *err)
{
	HinawaFcpTransaction *trans;

	hinawa_malloc(trans, sizeof(HinawaFcpTransaction), err);
	if (*err)
		return NULL;

	return trans;
}

void hinawa_fcp_transaction_run(HinawaFcpTransaction *trans, HinawaFwUnit *unit,
				uint8_t *cmd, unsigned int cmd_len,
				uint8_t *resp, unsigned int *resp_len,
				int *err)
{
	HinawaFwRequest *req;
	struct timespec abstime;

	if (cmd == NULL || resp == NULL || cmd_len == 0 || *resp_len == 0) {
		*err = EINVAL;
		return;
	}

	req = hinawa_fw_request_create(err);
	if (*err)
		return;

	/* prevent from wrong matching in response handler */
	memset(resp, 0, *resp_len);
	trans->resp = resp;
	trans->length = *resp_len;

	pthread_mutex_lock(&unit->fcp_transactions_lock);
	LIST_INSERT_HEAD(&unit->fcp_transactions, trans, link);
	pthread_mutex_unlock(&unit->fcp_transactions_lock);

	trans->cmd = cmd;
	hinawa_fw_request_write(req, unit, FCP_REQUEST_ADDR, cmd, cmd_len, err);
	if (*err)
		goto end;
deferred:
	pthread_cond_init(&trans->cond, NULL);
	pthread_mutex_init(&trans->mutex, NULL);
	abstime.tv_sec = time(NULL) + 1;
	abstime.tv_nsec = 500 * 100 * 100;
	*err = pthread_cond_timedwait(&trans->cond, &trans->mutex, &abstime);
	if (*err)
		goto end;

	/* deferred transaction */
	if (resp[0] == FCP_STATUS_INTERIM)
		goto deferred;

	*resp_len = trans->length;
end:
	pthread_mutex_lock(&unit->fcp_transactions_lock);
	LIST_REMOVE(trans, link);
	pthread_mutex_unlock(&unit->fcp_transactions_lock);

	trans->resp = NULL;
	trans->length = 0;

	hinawa_fw_request_destroy(req);
}

void handle_fcp_response(void *buf, unsigned int *length, void *private_data,
			 int *err)
{
	HinawaFwUnit *unit = (HinawaFwUnit *)private_data;
	uint8_t *resp = (uint8_t *)buf;
	HinawaFcpTransaction *trans;

	pthread_mutex_lock(&unit->fcp_transactions_lock);

	LIST_FOREACH(trans, &unit->fcp_transactions, link) {
		/* match subunit type/id and opcode */
		if (trans->cmd[1] == resp[1] &&
		    trans->cmd[2] == resp[2])
			break;
	}

	if (trans == NULL) {
		*err = ENODATA;
		goto end;
	} else if (trans->length < *length) {
		*err = EAGAIN;
		goto end;
	} else {
		memcpy(trans->resp, resp, *length);
		pthread_cond_signal(&trans->cond);
	}
end:
	pthread_mutex_unlock(&unit->fcp_transactions_lock);


	*length = 0;
}

void hinawa_fcp_transaction_destroy(HinawaFcpTransaction *trans)
{
	hinawa_free(trans);
}
