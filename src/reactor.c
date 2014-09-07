#include "reactor.h"

static LIST_HEAD(reactor_list_head, sndfw_reactor) reactors =
				LIST_HEAD_INITIALIZER(sndfw_reactor_head);
static pthread_mutex_t reactors_mutex = PTHREAD_MUTEX_INITIALIZER;

static void killing_reactor(void *arg)
{
	struct sndfw_reactor *reactor = arg;

	reactor->active = false;

	reactor->thread = 0;
	close(reactor->fd);
	free(reactor->results);
}

static void running_reactor(struct sndfw_reactor *reactor)
{
	struct sndfw_reactant *reactant;
	struct epoll_event *ev;
	uint32_t event;
	unsigned int i;
	int count, err;

	pthread_cleanup_push(killing_reactor, (void *)reactor);

	reactor->active = true;

	while (reactor->active) {
		count = epoll_wait(reactor->fd, reactor->results,
				   reactor->result_count, reactor->timeout);
		if (count < 0) {
			printf("%s\n", strerror(errno));
			reactor->active = false;
			continue;
		}

		for (i = 0; i < count; i++) {
			ev = &reactor->results[i];
			event = ev->events;
			reactant = (struct sndfw_reactant *)ev->data.ptr;
			if (reactant->callback != NULL) {
				err = reactant->callback(reactant, event);
				if (err < 0) {
					/* TODO */
					perror("reactant callback: ");
					reactor->active = false;
				}
			}
		}
	}

	pthread_exit(NULL);
	pthread_cleanup_pop(true);
}

int sndfw_reactor_init(struct sndfw_reactor *reactor, unsigned int priority)
{
	reactor->priority = priority;

	pthread_mutex_lock(&reactors_mutex);
	LIST_INSERT_HEAD(&reactors, reactor, link);
	pthread_mutex_unlock(&reactors_mutex);

	return 0;
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

	err = -pthread_create(&reactor->thread, &attr,
			      (void *)running_reactor, (void *)reactor);
end:
	return err;
}

void sndfw_reactor_stop(struct sndfw_reactor *reactor)
{
	reactor->active = false;

	if (reactor->thread)
		pthread_join(reactor->thread, NULL);
}

void sndfw_reactor_destroy(struct sndfw_reactor *reactor)
{
	sndfw_reactor_stop(reactor);

	pthread_mutex_lock(&reactors_mutex);
	LIST_REMOVE(reactor, link);
	pthread_mutex_unlock(&reactors_mutex);
}

int sndfw_reactant_init(struct sndfw_reactant *reactant, unsigned int priority,
			int fd, void *private_data,
			sndfw_reactant_callback_t callback)
{
	reactant->priority = priority;
	reactant->fd = fd;
	reactant->callback = callback;
	reactant->private_data = private_data;

	return 0;
}

int sndfw_reactant_add(struct sndfw_reactant *reactant, uint32_t events)
{
	struct sndfw_reactor *reactor;
	struct epoll_event ev;
	int err = 0;

	reactant->events = events;

	pthread_mutex_lock(&reactors_mutex);

	LIST_FOREACH(reactor, &reactors, link) {
		if (reactor->priority == reactant->priority)
			break;
	}

	if (reactor == NULL) {
		err = -EINVAL;
		goto end;
	}

	ev.events = reactant->events;
	ev.data.ptr = (void *)reactant;

	if (epoll_ctl(reactor->fd, EPOLL_CTL_ADD, reactant->fd, &ev) < 0)
		err = errno;
end:
	pthread_mutex_unlock(&reactors_mutex);
	return err;
}

void sndfw_reactant_remove(struct sndfw_reactant *reactant)
{
	struct sndfw_reactor *reactor;

	pthread_mutex_lock(&reactors_mutex);

	LIST_FOREACH(reactor, &reactors, link) {
		if (reactor->priority != reactant->priority)
			continue;

		epoll_ctl(reactor->fd, EPOLL_CTL_DEL, reactant->fd, NULL);
	}

	pthread_mutex_unlock(&reactors_mutex);
}

void sndfw_reactant_destroy(struct sndfw_reactant *reactant)
{
	/* nothing? */
}
