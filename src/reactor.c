#include <sys/epoll.h>
#include <sys/queue.h>
#include <pthread.h>

struct sndfw_reactant {
	LIST_ENTRY(struct sndfw_reactant) link;

	int (*handle)(struct sndfw_reactant, uint32_t events);
};

struct sndfw_reactor {
	bool active;

	int fd;
	unsigned int result_count;
	struct epoll_event *results;
	unsigned int timeout;

	LIST_ENTRY(struct sndfw_reactor) link;
	LIST_HEAD(, struct sndfw_reactant) reactants;

	pthread_t thread;
};

LIST_HEAD(, sndfw_reactor) reactors = LIST_HEAD_INITIALIZER(reactors);

static void killing_reactor(void *arg)
{
	struct sndfw_reactor *reactor = arg;

	LIST_REMOVE(reactor, link);
	reactor->active = false;

	reactor->thread = 0;
	close(reactor->fd);
	free(reactor->results);
}

static void running_reactor(struct sndfw_reactor *reactor)
{
	struct sndfw_reactant *reactant;
	uint32_t events;
	unsigned int i;
	int count, err;

	pthread_cleanup_push(killing_reactor, (void *)reactor);

	reactor->active = true;
	LIST_INSERT_HEAD(reactors, newcomer, link);

	while (reactor->active) {
		count = epoll_wait(reactor->fd, reactor->results,
				   reactor->result_count, reactor->timeout);
		if (count < 0) {
			printf("%s\n", strerror(errno));
			reactor->active = false;
			continue;
		}

		for (i = 0; i < count; i++) {
			reactant = reactor->events[i]->data;
			events = reactor->events[i]->events;
			err = reactant->handle(reactant, events);
			if (err < 0) {
				/* TODO */
				printf("%s\n", strerror(err));
				reactor->active = false;
			}
		}
	}

	pthread_exit(NULL);
	pthread_cleanup_pop(true);
}

int sndfw_reactor_new(struct sndfw_reactor **reactor)
{
	struct sndfw_reactor *newcomer;
	int err = 0;

	newcomer = malloc(sizeof(struct sndfw_reactor));
	if (newcomer == NULL) {
		err = -ENOMEM;
		goto end;
	}
	memset(newcomer, 0, sizeof(struct sndfw_reactor));

	LIST_INIT(newcomer->reactant);

	*reactor = newcomer;
end:
	return err;
}

int sndfw_reactor_start(struct sndfw_reactor *reactor,
			unsigned int result_count, unsigned int timeout)
{
	pthread_attr_t attr;
	int err;

	reactor->fd = epoll_create1(EPOLL_CLOEXEC);
	if (reactor->fd < 0) {
		err = errno;
		goto end;
	}

	reactor->results = malloc(sizeof(struct epoll_event) * result_count);
	if (reactor->results == NULL) {
		err = -ENOMEM;
		goto end;
	}
	memset(reactor->results, 0, sizeof(struct epoll_event) * result_count);
	reactor->result_count = result_count;
	reactor->timeout = timeout;

	err = -pthread_attr_init(&attr);
	if (err < 0)
		goto end;
	
	err = -pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
	if (err < 0)
		goto end;

	err = -pthread_attr_setschedpolicy(&attr, policy);
	if (err < 0)
		goto end;

	err = -pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (err < 0)
		goto end;

	err = -pthread_create(&reactor->thread, &attr,
			      (void *)running_reactor, (void *)reactor);
end:
	return err;
}

void sndfw_reactor_stop(struct sndfw_reactor *reactor)
{
	reactor->active = false;

	if (reactor->thread)
		pthread_join(&reactor->thread);
}

void sndfw_reactor_destroy(struct sndfw_reactor *reactor)
{
	sndfw_reactor_stop(reactor);
	free(reactor);
}
