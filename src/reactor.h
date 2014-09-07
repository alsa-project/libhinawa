#ifndef __ALSA_TOOLS_SNDFW_REACTOR
#define __ALSA_TOOLS_SNDFW_REACTOR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/queue.h>

struct sndfw_reactant;
typedef int (*sndfw_reactant_callback_t)(struct sndfw_reactant *, uint32_t);

struct sndfw_reactant {
	unsigned int priority;
	void *private_data;
	int fd;
	uint32_t events;
	sndfw_reactant_callback_t callback;
};

struct sndfw_reactor {
	unsigned int priority;
	bool active;

	int fd;
	unsigned int result_count;
	struct epoll_event *results;
	unsigned int timeout;

	/* private */
	pthread_t thread;
	LIST_ENTRY(sndfw_reactor) link;
};

int sndfw_reactant_init(struct sndfw_reactant *reactant, unsigned int priority,
			int fd, void *private_data,
			sndfw_reactant_callback_t callback);
int sndfw_reactant_add(struct sndfw_reactant *reactant, uint32_t events);
void sndfw_reactant_remove(struct sndfw_reactant *reactant);
void sndfw_reactant_destroy(struct sndfw_reactant *reactant);

int sndfw_reactor_init(struct sndfw_reactor *reactor, unsigned int priority);

int sndfw_reactor_start(struct sndfw_reactor *reactor,
			unsigned int result_count, unsigned int timeout);

void sndfw_reactor_stop(struct sndfw_reactor *reactor);

void sndfw_reactor_destroy(struct sndfw_reactor *reactor);

#endif
