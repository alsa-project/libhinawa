#include "fw-transaction.h"

static int send_response(int fd, uint32_t handle, uint32_t rcode)
{
	struct fw_cdev_send_response response;

	response.rcode = rcode;
	response.length = 0;
	response.data = 0;
	response.handle = handle;

	return ioctl(fd, FW_CDEV_IOC_SEND_RESPONSE, &response);
}

static int handle_bus_reset(struct sndfw_reactant *reactant)
{
	struct sndfw_fw_transaction *trans = reactant->private_data;
	struct fw_cdev_event_bus_reset *br =
			(struct fw_cdev_event_bus_reset *)trans->buffer;
	int err = 0;

	printf("\t\t%x %x %x %x %x\n",
	       br->node_id,
	       br->bm_node_id,
	       br->irm_node_id,
	       br->root_node_id,
	       br->generation);

	return err;
}

/* The closure is a closure of request. */
static int handle_response(struct sndfw_reactant *reactant)
{
	struct sndfw_fw_transaction *resp = reactant->private_data;
	struct fw_cdev_event_response *event =
			(struct fw_cdev_event_response *)resp->buffer;
	struct sndfw_fw_transaction *req =
			(struct sndfw_fw_transaction *)event->closure;
	int err = 0;

	printf("\t\trcode: %x, length: %x, %x\n",
	       event->rcode, event->length, req->length);

	if (event->length > 0 && req->length >= event->length)
		memcpy(req->buffer, event->data, event->length);

	pthread_cond_signal(&req->cond);

	return err;
}

static int handle_request(struct sndfw_reactant *reactant)
{
	struct sndfw_fw_transaction *trans = reactant->private_data;
	int err = 0;
	return err;
}

static int handle_request2(struct sndfw_reactant *reactant)
{
	struct sndfw_fw_transaction *trans = reactant->private_data;
	struct fw_cdev_event_request2 *req =
				(struct fw_cdev_event_request2 *)trans->buffer;
	int err;

	printf("\t\t%x %llx %x %x %x %x %x %x\n",
	       req->tcode,
	       req->offset,
	       req->source_node_id,
	       req->destination_node_id,
	       req->card,
	       req->generation,
	       req->handle,
	       req->length);
	err = send_response(reactant->fd, req->handle, RCODE_COMPLETE);

	return err;
}

static int reactant_handle(struct sndfw_reactant *reactant, uint32_t events)
{
	struct sndfw_fw_transaction *resp = reactant->private_data;
	struct fw_cdev_event_common *common;
	int err;

	int (*handler)(struct sndfw_reactant *reactant);
	static int (*event_handlers[])(struct sndfw_reactant *reactant) = {
		[FW_CDEV_EVENT_BUS_RESET]	= handle_bus_reset,
		[FW_CDEV_EVENT_RESPONSE]	= handle_response,
		[FW_CDEV_EVENT_REQUEST]		= handle_request,
		[FW_CDEV_EVENT_ISO_INTERRUPT]	= NULL,
		[FW_CDEV_EVENT_ISO_RESOURCE_ALLOCATED] = NULL,
		[FW_CDEV_EVENT_ISO_RESOURCE_DEALLOCATED] = NULL,
		[FW_CDEV_EVENT_REQUEST2]	= handle_request2,
		[FW_CDEV_EVENT_PHY_PACKET_SENT]	= NULL,
		[FW_CDEV_EVENT_PHY_PACKET_RECEIVED] = NULL,
		[FW_CDEV_EVENT_ISO_INTERRUPT_MULTICHANNEL] = NULL};

	/*
	 * Kernel driver remove an entry from list, then copy_to_user().
	 * As a result, an application in userspace MUST read at once.
	 */
	err = read(reactant->fd, resp->buffer, FW_TRANS_BUFFER_BYTES);
	if (err < 0) {
		err = -errno;
		goto end;
	}
	resp->length = err;
	common = (struct fw_cdev_event_common *)resp->buffer;

	printf ("\ttype: %x, closure: %llx\n",
	        common->type, common->closure);

	handler = event_handlers[common->type];
	if (handler != NULL) {
		err = handler(reactant);
		if (err < 0)
			goto end;
	}
end:
	return err;
}

int sndfw_fw_transaction_init(struct sndfw_fw_transaction *trans)
{
	trans->buffer = malloc(FW_TRANS_BUFFER_BYTES);
	if (trans->buffer == NULL)
		return -ENOMEM;
	trans->length = FW_TRANS_BUFFER_BYTES;

	pthread_cond_init(&trans->cond, NULL);
	pthread_mutex_init(&trans->mutex, NULL);

	return 0;
}

int sndfw_fw_transaction_write(struct sndfw_fw_transaction *trans,
			       int fd, uint64_t addr, uint64_t generation)
{
	struct fw_cdev_send_request request;
	struct sndfw_reactant reactant;
	unsigned int quadlets;
	struct timespec abstime;
	int err;

	quadlets = trans->length / 4;
	if (quadlets == 0)
		return -EINVAL;
	else if (quadlets == 1)
		request.tcode = TCODE_WRITE_QUADLET_REQUEST;
	else
		request.tcode = TCODE_WRITE_BLOCK_REQUEST;

	request.length = trans->length;
	request.offset = addr;
	request.closure = (uint64_t)trans;
	request.data = (uint64_t)trans->buffer;
	request.generation = generation;

	if (ioctl(fd, FW_CDEV_IOC_SEND_REQUEST, &request) < 0)
		return -errno;

	abstime.tv_sec = time(NULL) + 3;
	abstime.tv_nsec = 0;
	err = pthread_cond_timedwait(&trans->cond, &trans->mutex, &abstime);
	if (err == ETIMEDOUT || err == EINTR)
		return -ETIMEDOUT;
	else
		return 0;
}

int sndfw_fw_transaction_read(struct sndfw_fw_transaction *trans,
			      int fd, uint64_t addr, uint64_t generation)
{
	struct fw_cdev_send_request request;
	struct sndfw_reactant reactant;
	unsigned int quadlets;
	struct timespec abstime;
	int err;

	quadlets = trans->length / 4;
	if (quadlets == 0)
		return -EINVAL;
	else if (quadlets == 1)
		request.tcode = TCODE_READ_QUADLET_REQUEST;
	else
		request.tcode = TCODE_READ_BLOCK_REQUEST;

	request.length = trans->length;
	request.offset = addr;
	request.closure = (uint64_t)trans;
	request.data = (uint64_t)trans->buffer;
	request.generation = generation;

	if (ioctl(fd, FW_CDEV_IOC_SEND_REQUEST, &request) < 0)
		return -errno;

	abstime.tv_sec = time(NULL) + 3;
	abstime.tv_nsec = 0;
	err = pthread_cond_timedwait(&trans->cond, &trans->mutex, &abstime);
	if (err == ETIMEDOUT || err == EINTR)
		return -ETIMEDOUT;
	else
		return 0;
}

void sndfw_fw_transaction_destroy(struct sndfw_fw_transaction *trans)
{
	free(trans->buffer);
	pthread_cond_destroy(&trans->cond);
	pthread_mutex_destroy(&trans->mutex);
}

int sndfw_fw_transaction_listen(struct sndfw_fw_transaction *trans, int fd)
{
	sndfw_reactant_init(&trans->reactant, 0, fd, trans, reactant_handle);
	if (sndfw_reactant_add(&trans->reactant, EPOLLIN) < 0)
		printf("hepo\n");

	return 0;
}

void sndfw_fw_transaction_unlisten(struct sndfw_fw_transaction *trans)
{

	sndfw_reactant_remove(&trans->reactant);
}
