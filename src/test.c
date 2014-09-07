#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include <sys/queue.h>
#include <pthread.h>

#include "reactor.h"
#include "fw-transaction.h"

static int send_read_request(int fd, unsigned int generation)
{
	struct sndfw_fw_transaction trans;
	uint8_t *cmd;
	int err;

	sndfw_fw_transaction_init(&trans);

	trans.length = 4;
	err = sndfw_fw_transaction_read(&trans, fd, 0xfffff0000904,
					generation);
	if (err < 0)
		printf("Fail to send request: %s\n", strerror(-err));

	sndfw_fw_transaction_destroy(&trans);

	return err;
}

static int send_write_request(int fd, unsigned int generation)
{
	struct sndfw_fw_transaction trans;
	uint8_t *cmd;
	int err;

	sndfw_fw_transaction_init(&trans);

	cmd = (uint8_t *)trans.buffer;
	cmd[0] = 0x01;
	cmd[1] = 0xff;
	cmd[2] = 0x18;
	cmd[3] = 0x00;
	cmd[4] = 0xff;
	cmd[5] = 0xff;
	cmd[6] = 0xff;
	cmd[7] = 0xff;
	trans.length = 8;

	err = sndfw_fw_transaction_write(&trans, fd, FCP_COMMAND_ADDR,
					 generation);
	if (err < 0)
		printf("Fail to send request: %s\n", strerror(-err));

	sndfw_fw_transaction_destroy(&trans);

	return err;
}

int main(int argc, void *argv[])
{
	char *path;
	int fd;
	struct fw_cdev_get_info info;
	struct fw_cdev_event_bus_reset reset = {0};
	struct fw_cdev_allocate allocate;

	int epfd;
	struct epoll_event ev = {0};

	bool running;
	unsigned int count;
	unsigned int i;
	struct epoll_event events[10];

	struct sndfw_reactor reactor;
	struct sndfw_reactant reactant;
	struct sndfw_fw_transaction resp;


	if (argc != 2) {
		printf("Full path for firewire character device is requred.\n");
		exit(EXIT_FAILURE);
	}
	path = argv[1];

	fd = open(path, O_RDWR);
	if (fd < 0) {
		printf("Fail to open %s.\n", path);
		exit(EXIT_FAILURE);
	}
	info.version = 4;
	info.rom_length = 0;
	info.rom = 0;
	info.bus_reset = (uint64_t)&reset;
	info.bus_reset_closure = 0;
	if (ioctl(fd, FW_CDEV_IOC_GET_INFO, &info) < 0) {
		perror("Fail to execute ioctl(FW_CDEV_IOC_GET_INFO):");
		exit(EXIT_FAILURE);
	}

	printf("%d %d %d %llx\n",
	       info.card, info.version, info.rom_length, info.bus_reset);
	printf("%x %x %x %x %x %x\n",
	       reset.node_id, reset.local_node_id, reset.bm_node_id,
	       reset.irm_node_id, reset.root_node_id, reset.generation);

	allocate.offset = FCP_RESPONSE_ADDR;
	allocate.closure = 0;
	allocate.length = 0x200;
	allocate.region_end = allocate.offset + allocate.length;

	if (ioctl(fd, FW_CDEV_IOC_ALLOCATE, &allocate) < 0) {
		perror("Fail to allocate for FCP: ");
		exit(EXIT_FAILURE);
	}


	if (sndfw_reactor_init(&reactor, 0) < 0)
		printf("hoge\n");
	if (sndfw_reactor_start(&reactor, 10, 100) < 0)
		printf("fuga\n");

	sndfw_fw_transaction_init(&resp);
	sndfw_fw_transaction_listen(&resp, fd);

	if (send_write_request(fd, reset.generation) < 0)
		exit(EXIT_FAILURE);

	if (send_read_request(fd, reset.generation) < 0)
		exit(EXIT_FAILURE);

	sleep(15);

	sndfw_reactant_remove(&reactant);
	sndfw_reactant_destroy(&reactant);

	sndfw_fw_transaction_destroy(&resp);

	sndfw_reactor_stop(&reactor);
	sndfw_reactor_destroy(&reactor);

	close(epfd);
	close(fd);

	return 0;
}
