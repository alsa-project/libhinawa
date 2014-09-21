#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "internal.h"

static LIST_HEAD(reactor_list_head, hinawa_reactor) reactors =
				LIST_HEAD_INITIALIZER(hinawa_reactor_head);
static pthread_mutex_t reactors_mutex = PTHREAD_MUTEX_INITIALIZER;

static void killing_reactor(void *arg)
{
	struct hinawa_reactor *reactor = arg;

	reactor->active = false;

	reactor->thread = 0;
	close(reactor->fd);
	hinawa_free(reactor->results);
}

static void hinawa_reactant_prepared(struct hinawa_reactant *reactant,
				     uint32_t event, int *err)
{
	HinawaReactantState state;

	if (!reactant->callback)
		return;

	if (event & EPOLLIN)
		state = HinawaReactantStateReadable;
	else if (event & EPOLLOUT)
		state = HinawaReactantStateWritable;
	else
		state = HinawaReactantStateError;

	reactant->callback(reactant->private_data, state, err);
}

static void running_reactor(struct hinawa_reactor *reactor)
{
	struct hinawa_reactant *reactant;
	struct epoll_event *ev;
	uint32_t event;
	unsigned int i;
	int count, err;

	pthread_cleanup_push(killing_reactor, (void *)reactor);

	reactor->active = true;

	while (reactor->active) {
		count = epoll_wait(reactor->fd, reactor->results,
				   reactor->result_count, reactor->period);
		if (count < 0) {
			printf("%s\n", strerror(errno));
			reactor->active = false;
			continue;
		}

		for (i = 0; i < count; i++) {
			ev = &reactor->results[i];
			event = ev->events;
			reactant = (struct hinawa_reactant *)ev->data.ptr;
			hinawa_reactant_prepared(reactant, event, &err);
		}
	}

	pthread_exit(NULL);
	pthread_cleanup_pop(true);
}

HinawaReactor *hinawa_reactor_create(unsigned int priority,
				     HinawaReactorCallback callback,
				     void *private_data, int *err)
{
	HinawaReactor *reactor;

	hinawa_malloc(reactor, sizeof(HinawaReactor), err);
	if (*err)
		return NULL;

	if (callback) {
		callback(private_data, HinawaReactorStateCreated, err);
		if (*err) {
			hinawa_free(reactor);
			return NULL;
		}
	}

	reactor->priority = priority;
	reactor->callback = callback;
	reactor->private_data = private_data;

	pthread_mutex_lock(&reactors_mutex);
	LIST_INSERT_HEAD(&reactors, reactor, link);
	pthread_mutex_unlock(&reactors_mutex);

	return reactor;
}

void hinawa_reactor_start(HinawaReactor *reactor, unsigned int result_count,
			  unsigned int period, int *err)
{
	pthread_attr_t attr;

	reactor->fd = epoll_create1(EPOLL_CLOEXEC);
	if (reactor->fd < 0) {
		*err = errno;
		return;
	}

	hinawa_malloc(reactor->results,
		      sizeof(struct epoll_event) * result_count, err);
	if (*err) {
		close(reactor->fd);
		return;
	}
	reactor->result_count = result_count;
	reactor->period = period;

	*err = -pthread_attr_init(&attr);
	if (*err)
		goto end;
	
	*err = -pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
	if (*err)
		goto end;

	*err = -pthread_create(&reactor->thread, &attr,
			      (void *)running_reactor, (void *)reactor);
	if (*err)
		goto end;

	if (reactor->callback)
		reactor->callback(reactor->private_data,
				  HinawaReactorStateStarted, err);
end:
	if (*err) {
		close(reactor->fd);
		hinawa_free(reactor->results);
	}
}

void hinawa_reactor_stop(HinawaReactor *reactor)
{
	int err;

	reactor->active = false;

	if (reactor->thread)
		pthread_join(reactor->thread, NULL);

	if (reactor->callback)
		reactor->callback(reactor->private_data,
				  HinawaReactorStateStopped, &err);
}

void hinawa_reactor_destroy(HinawaReactor *reactor)
{
	HinawaReactorCallback callback = reactor->callback;
	void *private_data = reactor->private_data;
	int err;

	pthread_mutex_lock(&reactors_mutex);
	LIST_REMOVE(reactor, link);
	pthread_mutex_unlock(&reactors_mutex);

	hinawa_free(reactor);

	if (callback)
		callback(private_data, HinawaReactorStateDestroyed, &err);
}

HinawaReactant *hinawa_reactant_create(unsigned int priority, int fd,
				       HinawaReactantCallback callback,
				       void *private_data, int *err)
{
	HinawaReactant *reactant;

	hinawa_malloc(reactant, sizeof(HinawaReactant), err);
	if (*err)
		return NULL;

	if (reactant->callback) {
		callback(reactant->private_data, HinawaReactantStateCreated,
			 err);
		if (*err) {
			hinawa_free(reactant);
			return NULL;
		}
	}

	reactant->priority = priority;
	reactant->fd = fd;
	reactant->callback = callback;
	reactant->private_data = private_data;

	return reactant;
}

void hinawa_reactant_add(HinawaReactant *reactant, uint32_t events, int *err)
{
	struct hinawa_reactor *reactor;
	struct epoll_event ev;

	reactant->events = events;

	pthread_mutex_lock(&reactors_mutex);

	LIST_FOREACH(reactor, &reactors, link) {
		if (reactor->priority == reactant->priority)
			break;
	}

	if (reactor == NULL) {
		*err = -EINVAL;
		goto end;
	}

	ev.events = reactant->events;
	ev.data.ptr = (void *)reactant;

	if (epoll_ctl(reactor->fd, EPOLL_CTL_ADD, reactant->fd, &ev) < 0) {
		*err = errno;
		goto end;
	}

	if (reactant->callback) {
		reactant->callback(reactant->private_data,
				   HinawaReactantStateAdded, err);
		if (*err) {
			epoll_ctl(reactor->fd, EPOLL_CTL_DEL,
				  reactant->fd, &ev);
			goto end;
		}
	}
end:
	pthread_mutex_unlock(&reactors_mutex);
}

void hinawa_reactant_remove(HinawaReactant *reactant)
{
	struct hinawa_reactor *reactor;
	int err;

	pthread_mutex_lock(&reactors_mutex);

	LIST_FOREACH(reactor, &reactors, link)
		epoll_ctl(reactor->fd, EPOLL_CTL_DEL, reactant->fd, NULL);

	pthread_mutex_unlock(&reactors_mutex);

	if (reactant->callback)
		reactant->callback(reactant->private_data,
				   HinawaReactantStateRemoved, &err);
}

void hinawa_reactant_destroy(HinawaReactant *reactant)
{
	HinawaReactantCallback callback = reactant->callback;
	void *private_data = reactant->private_data;
	int err;

	hinawa_free(reactant);

	callback(private_data, HinawaReactantStateDestroyed, &err);
}
