#ifndef __ALSA_TOOLS_SNDFW_FW_TRANSACTION__
#define __ALSA_TOOLS_SNDFW_FW_TRANSACTION__

#include "reactor.h"

#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

#define FCP_COMMAND_ADDR	0xfffff0000b00uLL
#define FCP_RESPONSE_ADDR	0xfffff0000d00uLL

#define FCP_FRAME_MAX_BYTES	0x200

#define FW_TRANS_BUFFER_BYTES sizeof(union fw_cdev_event) + FCP_FRAME_MAX_BYTES

struct sndfw_fw_transaction {
	void *buffer;
	unsigned int length;
	uint64_t addr;
	int (*callback)(void *buffer, void *private_data);
	void *private_data;

	pthread_cond_t cond;
	pthread_mutex_t mutex;
	struct sndfw_reactant reactant;
};

int sndfw_fw_transaction_init(struct sndfw_fw_transaction *trans);
void sndfw_fw_transaction_destroy(struct sndfw_fw_transaction *trans);

int sndfw_fw_transaction_write(struct sndfw_fw_transaction *trans,
			       int fd, uint64_t addr, uint64_t generation);
int sndfw_fw_transaction_read(struct sndfw_fw_transaction *trans,
			      int fd, uint64_t addr, uint64_t generation);
int sndfw_fw_transaction_listen(struct sndfw_fw_transaction *trans, int fd);

#endif
