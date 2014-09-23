#ifndef __ALSA_TOOLS_HINAWA_INTERNAL__
#define __ALSA_TOOLS_HINAWA_INTERNAL__

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>
#include "../include/hinawa.h"

#include <sound/firewire.h>
#include <alsa/asoundlib.h>

#include "backport.h"

#define hinawa_malloc(ptr, length, errptr) \
	(ptr) = malloc((length)); \
	if (!(errptr)) \
		(*errptr) = ENOMEM; \
	else \
		memset((ptr), 0, (length));

#define hinawa_free(ptr) \
	if (ptr) \
		free(ptr); \
	ptr = NULL;

#define FCP_MAXIMUM_FRAME_BYTES	0x200U
#define FCP_REQUEST_ADDR	0xfffff0000b00
#define FCP_RESPOND_ADDR	0xfffff0000d00

struct hinawa_snd_unit {
	uint8_t guid[8];
	unsigned int type;
	unsigned int card;
	char device[16];
	unsigned char name[32];

	void *buf;
	unsigned int length;
	bool streaming;

	snd_hwdep_t *hwdep;
	HinawaReactant *reactant;
	bool listening;

	/* For Echo Audio Fireworks */
	uint32_t seqnum;
	LIST_HEAD(efw_transactions_list_head, hinawa_efw_transaction)
							efw_transactions;
	pthread_mutex_t efw_transactions_lock;
};

void handle_efw_response(HinawaSndUnit *unit, void *buf, unsigned int length,
			 int *err);
void handle_fcp_response(void *buf, unsigned int *length, void *private_data,
			 int *err);

struct hinawa_fw_unit {
	int fd;
	uint32_t generation;

	void *buf;
	unsigned int length;
	HinawaReactant *reactant;
	bool listening;

	HinawaFwRespond *fcp;
	LIST_HEAD(fcp_transactions_list_head, hinawa_fcp_transaction)
							fcp_transactions;
	pthread_mutex_t fcp_transactions_lock;
};

struct hinawa_reactant {
	unsigned int priority;
	int fd;
	uint32_t events;
	HinawaReactantCallback callback;
	void *private_data;
};

struct hinawa_reactor {
	unsigned int priority;
	bool active;

	int fd;
	unsigned int result_count;
	struct epoll_event *results;
	unsigned int period;

	HinawaReactorCallback callback;
	void *private_data;

	pthread_t thread;
	LIST_ENTRY(hinawa_reactor) link;
};

struct hinawa_fw_respond {
	int fd;
	unsigned int length;
	HinawaFwRespondCallback callback;
	void *private_data;
	uint64_t addr_handle;
};

struct hinawa_fw_request {
	int fd;
	void *buf;
	unsigned int length;
	HinawaFwRequestType type;
	void *private_data;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};

#endif
