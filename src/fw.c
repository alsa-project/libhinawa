#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>

#include "internal.h"
#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

typedef void (*HinawaFwListenHandle)(int fd, union fw_cdev_event *ev);

HinawaFwRespond *hinawa_fw_respond_create(HinawaFwRespondCallback callback,
					  void *private_data, int *err)
{
	HinawaFwRespond *resp;

	hinawa_malloc(resp, sizeof(HinawaFwRespond), err);
	if (*err)
		return NULL;

	resp->callback = callback;
	resp->private_data = private_data;

	return resp;
}

void hinawa_fw_respond_register(HinawaFwRespond *resp, HinawaFwUnit *unit,
				uint64_t addr, unsigned int length, int *err)
{
	struct fw_cdev_allocate allocate = {0};

	allocate.offset = addr;
	allocate.closure = (uint64_t)resp;
	allocate.length = length;
	allocate.region_end = addr + length;

	if (ioctl(unit->fd, FW_CDEV_IOC_ALLOCATE, &allocate) < 0) {
		*err = errno;
		return;
	}

	resp->fd = unit->fd;
	resp->length = length;
	resp->addr_handle = allocate.handle;
}

/*
 * IEEE1394: Table 10-3 — Response code encoding
 * Code	Comment
 * COMPLETE:		The node has successfully completed the command.
 * CONFLICT_ERROR:	A resource conflict was detected. The request may be
 *			retried.
 * DATA_ERROR:		Hardware error, data is unavailable.
 * TYPE_ERROR:		A field in the request packet was set to an unsupported
 *			or incorrect value, or an invalid reqaction was
 *			attempted (e.g., a write to a read-only address).
 * ADDRESS_ERROR:	The destination offset in the request was set to an
 *			address not accessible in the destination node.
 */
static void handle_request(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_request2 *event = &ev->request2;
	HinawaFwRespond *resp = (HinawaFwRespond *)event->closure;
	struct fw_cdev_send_response send = {0};
	unsigned int length;
	int err;
	uint32_t rcode;

	/* decide rcode */
	if (!resp || !resp->callback) {
		rcode = RCODE_ADDRESS_ERROR;
		goto respond;
	}

	if ((resp->length < event->length)) {
		rcode = RCODE_CONFLICT_ERROR;
		goto respond;
	}

	length = event->length;

	resp->callback(event->data, &length, resp->private_data, &err);
	if (err)
		rcode = RCODE_DATA_ERROR;
	else
		rcode = RCODE_COMPLETE;
respond:
	send.rcode	= rcode;
	send.length	= length;
	send.data	= (uint64_t)event->data;
	send.handle	= event->handle;

	ioctl(fd, FW_CDEV_IOC_SEND_RESPONSE, &send);
}

void hinawa_fw_respond_unregister(HinawaFwRespond *resp)
{
	struct fw_cdev_deallocate deallocate = {0};
	deallocate.handle = resp->addr_handle;
	ioctl(resp->fd, FW_CDEV_IOC_DEALLOCATE, &deallocate);
}

void hinawa_fw_respond_destroy(HinawaFwRespond *resp)
{
	hinawa_free(resp);
}

HinawaFwRequest *hinawa_fw_request_create(int *err)
{
	HinawaFwRequest *req;

	hinawa_malloc(req, sizeof(HinawaFwRequest), err);
	if (!req)
		return NULL;

	return req;
}

void hinawa_fw_request_run(HinawaFwRequest *req, HinawaFwUnit *unit,
			   HinawaFwRequestType type, uint64_t addr, void *buf,
			   unsigned int length, int *err)
{
	struct fw_cdev_send_request send = {0};
	unsigned int quadlets;
	struct timespec abstime;

	quadlets = length / 4;
	if (quadlets == 0) {
		*err = EINVAL;
		return;
	} else if (type == HinawaFwRequestTypeRead) {
		req->buf = buf;
		req->length = length;
		send.data = (uint64_t)NULL;
		if (quadlets == 1)
			send.tcode = TCODE_READ_QUADLET_REQUEST;
		else
			send.tcode = TCODE_READ_BLOCK_REQUEST;
	} else if (type == HinawaFwRequestTypeWrite) {
		req->buf = NULL;
		req->length = 0;
		send.data = (uint64_t)buf;
		if (quadlets == 1)
			send.tcode = TCODE_WRITE_QUADLET_REQUEST;
		else
			send.tcode = TCODE_WRITE_BLOCK_REQUEST;
	/* Not implemented yet. */
	} else if ((type == HinawaFwRequestTypeCompareSwap) &&
		   ((quadlets == 2) || (quadlets == 4))) {
			req->buf = NULL;
			req->length = 0;
			send.data = (uint64_t)buf;
			send.tcode = TCODE_LOCK_COMPARE_SWAP;
	} else {
		*err = EINVAL;
		return;
	}

	send.length = length;
	send.offset = addr;
	send.closure = (uint64_t)req;
	send.generation = unit->generation;

	pthread_cond_init(&req->cond, NULL);
	pthread_mutex_init(&req->mutex, NULL);
	/* TODO: consider timeout of epoll */
	abstime.tv_sec = time(NULL) + 1;
	abstime.tv_nsec = 0;
	if (ioctl(unit->fd, FW_CDEV_IOC_SEND_REQUEST, &send) < 0) {
		*err = errno;
		return;
	}

	*err = pthread_cond_timedwait(&req->cond, &req->mutex, &abstime);
	req->buf = NULL;
	req->length = 0;

	if (*err)
		return;
}

static void handle_response(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_response *event = &ev->response;
	HinawaFwRequest *req = (HinawaFwRequest *)event->closure;

	if (!req)
		return;

	if (req->buf != NULL && event->length > 0 &&
	    req->length == event->length)
		memcpy(req->buf, event->data, event->length);

	pthread_cond_signal(&req->cond);

	return;
}

void hinawa_fw_request_destroy(HinawaFwRequest *req)
{
	hinawa_free(req);
}

HinawaFwUnit *hinawa_fw_unit_create(char *fw_path, int *err)
{
	HinawaFwUnit *unit;

	hinawa_malloc(unit, sizeof(HinawaFwUnit), err);
	if (*err)
		return NULL;

	unit->fd = open(fw_path, O_RDWR);
	if (unit->fd < 0) {
		*err = errno;
		hinawa_free(unit);
		return NULL;
	}

	return unit;
}

HinawaFwUnit *hinawa_fw_unit_from_snd_unit(HinawaSndUnit *snd_unit, int *err)
{
	char fw_path[36];
	HinawaFwUnit *fw_unit;

	snprintf(fw_path, 36, "/dev/%s", snd_unit->device);

	fw_unit = hinawa_fw_unit_create(fw_path, err);
	if (*err)
		return NULL;

	return fw_unit;
}

static void handle_update(int fd, union fw_cdev_event *ev)
{
	struct fw_cdev_event_bus_reset *event = &ev->bus_reset;
	HinawaFwUnit *unit = (HinawaFwUnit *)event->closure;

	if (unit)
		unit->generation = event->generation;
}

static void handle_events(void *private_data, HinawaReactantState state,
			  int *err)
{
	HinawaFwUnit *unit = (HinawaFwUnit *)private_data;
	struct fw_cdev_event_common *common;

	HinawaFwListenHandle handle;
	HinawaFwListenHandle handles[] = {
		[FW_CDEV_EVENT_BUS_RESET]	= handle_update,
		[FW_CDEV_EVENT_RESPONSE]	= handle_response,
		[FW_CDEV_EVENT_REQUEST2]	= handle_request
	};

	if (!unit || (state != HinawaReactantStateReadable))
		return;

	/*
	 * Kernel driver remove an entry from list, then copy_to_user().
	 * As a result, an application in userspace MUST read at once.
	 */
	if (read(unit->fd, unit->buf, unit->length) < 0) {
		printf("Fail to read(2) from %d\n", unit->fd);
		return;
	}
	/* TODO: handle I/O error */

	common = (struct fw_cdev_event_common *)unit->buf;

	if (common->type >= sizeof(handles)/sizeof(handles[0]))
		return;

	handle = handles[common->type];
	if (!handle)
		return;

	handle(unit->fd, (union fw_cdev_event *)unit->buf);
}

static void listen_fcp(HinawaFwUnit *unit, int *err)
{
	HinawaFwRespond *fcp;

	fcp = hinawa_fw_respond_create(handle_fcp_response, unit, err);
	if (*err)
		return;

	hinawa_fw_respond_register(fcp, unit, FCP_RESPOND_ADDR,
				   FCP_MAXIMUM_FRAME_BYTES, err);
	if (*err) {
		hinawa_fw_respond_destroy(fcp);
		return;
	}

	unit->fcp = fcp;
	LIST_INIT(&unit->fcp_transactions);
}

static void unlisten_fcp(HinawaFwUnit *unit)
{
	hinawa_fw_respond_unregister(unit->fcp);
	hinawa_fw_respond_destroy(unit->fcp);
}

void hinawa_fw_unit_listen(HinawaFwUnit *unit, int priority, int *err)
{
	struct fw_cdev_get_info info = {0};
	struct fw_cdev_event_bus_reset br = {0};

	if (unit->listening)
		return;

	/* NOTE: seems to be enough */
	hinawa_malloc(unit->buf, 2048, err);
	if (*err)
		return;
	unit->length = 2048;

	unit->reactant = hinawa_reactant_create(priority, unit->fd,
						handle_events, (void *)unit,
						err);
	if (*err) {
		hinawa_free(unit->buf);
		return;
	}

	hinawa_reactant_add(unit->reactant, EPOLLIN, err);
	if (*err) {
		hinawa_reactant_destroy(unit->reactant);
		unit->reactant = NULL;
		return;
	}

	listen_fcp(unit, err);
	if (*err) {
		hinawa_reactant_remove(unit->reactant);
		hinawa_reactant_destroy(unit->reactant);
		return;
	}

	info.version = 4;
	info.bus_reset = (uint64_t)&br;
	info.bus_reset_closure = (uint64_t)unit;
	if (ioctl(unit->fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		unlisten_fcp(unit);
		hinawa_reactant_remove(unit->reactant);
		hinawa_reactant_destroy(unit->reactant);
		*err = errno;
		return;
	}

	unit->listening = true;
	unit->generation = br.generation;
}

void hinawa_fw_unit_unlisten(HinawaFwUnit *unit)
{
	struct fw_cdev_get_info info = {0};

	unit->listening = false;
	info.version = 4;
	info.bus_reset_closure = 0;
	ioctl(unit->fd, FW_CDEV_IOC_GET_INFO, &info);

	hinawa_reactant_remove(unit->reactant);
	hinawa_reactant_destroy(unit->reactant);

	hinawa_free(unit->buf);
}

void hinawa_fw_unit_destroy(HinawaFwUnit *unit)
{
	hinawa_free(unit);
}
