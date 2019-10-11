/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "internal.h"
#include <errno.h>

enum th_type {
	TH_TYPE_DISPATCHER = 0,
	TH_TYPE_NOTIFIER,
	TH_TYPE_COUNT,
};

static gint counter[TH_TYPE_COUNT];

static GThread *th[TH_TYPE_COUNT];
static gboolean running[TH_TYPE_COUNT];
static G_LOCK_DEFINE(th_lock);

// For dispatcher thread.
static GMainContext *ctx;

// For notifier thread.
static GCond cond;
static GMutex mutex;
static GList *works;

struct notifier_work {
	void *target;
	NotifierWorkFunc func;
	unsigned int length;
	char data[0];
};

static gpointer run_dispacher(gpointer data)
{
	while (running[TH_TYPE_DISPATCHER])
		g_main_context_iteration(ctx, TRUE);

	g_thread_exit(NULL);

	return NULL;
}

static gpointer run_notifier(gpointer data)
{
	g_mutex_lock(&mutex);

	while (running[TH_TYPE_NOTIFIER]) {
		GList *entry;

		g_cond_wait(&cond, &mutex);

		entry = works;
		while (entry != NULL) {
			struct notifier_work *work =
					(struct notifier_work *)entry->data;

			works = g_list_delete_link(works, entry);

			g_mutex_unlock(&mutex);

			// NOTE: Allow to enqueue new works here.

			work->func(work->target, work->data, work->length);
			free(work);

			g_mutex_lock(&mutex);

			// NOTE: So this should not be assigned to .next.
			entry = works;
		}
	}

	g_mutex_unlock(&mutex);

	g_thread_exit(NULL);

	return NULL;
}

static void create_thread(enum th_type type, GError **exception)
{
	static const struct {
		const char *label;
		GThreadFunc func;
	} *entry, entries[] = {
		[TH_TYPE_DISPATCHER] = {
			"dispatcher",
			run_dispacher,
		},
		[TH_TYPE_NOTIFIER] = {
			"notifier",
			run_notifier,
		},
	};
	char name[16];

	if (th[type] != NULL)
		return;

	if (type == TH_TYPE_DISPATCHER) {
		ctx = g_main_context_new();
	} else {
		g_cond_init(&cond);
		g_mutex_init(&mutex);
	}

	entry = &entries[type];
	running[type] = TRUE;
	g_snprintf(name, sizeof(name), "hinawa:%s", entry->label);
	th[type] = g_thread_try_new(name, entry->func, NULL, exception);
}

static void stop_thread(enum th_type type)
{
	if (th[type] == NULL)
		return;

	running[type] = FALSE;
	if (type == TH_TYPE_DISPATCHER)
		g_main_context_wakeup(ctx);
	else
		g_cond_signal(&cond);

	g_thread_join(th[type]);
	g_thread_unref(th[type]);
	th[type] = NULL;

	if (type == TH_TYPE_DISPATCHER) {
		g_main_context_unref(ctx);
		ctx = NULL;
	} else {
		g_cond_clear(&cond);
		g_mutex_clear(&mutex);
	}
}

void hinawa_context_add_src(GSource *src, GError **exception)
{
	G_LOCK(th_lock);

	if (counter[TH_TYPE_DISPATCHER] == 0) {
		// For dispatcher thread.
		create_thread(TH_TYPE_DISPATCHER, exception);
		if (*exception != NULL) {
			g_main_context_unref(ctx);
			goto end;
		}

		++(counter[TH_TYPE_DISPATCHER]);
	}

	// NOTE: The returned ID is never used.
	g_source_attach(src, ctx);
end:
	G_UNLOCK(th_lock);
}

void hinawa_context_remove_src(GSource *src)
{
	G_LOCK(th_lock);

	if (--(counter[TH_TYPE_DISPATCHER]) == 0)
		stop_thread(TH_TYPE_DISPATCHER);

	G_UNLOCK(th_lock);

	g_source_destroy(src);
}

void hinawa_context_start_notifier(GError **exception)
{
	G_LOCK(th_lock);

	if (counter[TH_TYPE_NOTIFIER] == 0) {
		// For sub thread.
		create_thread(TH_TYPE_NOTIFIER, exception);
		if (*exception != NULL) {
			g_main_context_unref(ctx);
			stop_thread(TH_TYPE_DISPATCHER);
			goto end;
		}

		++(counter[TH_TYPE_NOTIFIER]);
	}
end:
	G_UNLOCK(th_lock);
}

void hinawa_context_schedule_notification(void *target, const void *data,
				unsigned int length, NotifierWorkFunc func,
				int *err)
{
	struct notifier_work *work;

	work = g_malloc0(sizeof(*work) + length);
	if (work == NULL) {
		*err = -ENOMEM;
		return;
	}

	work->target = target;
	work->func = func;

	if (length > 0) {
		work->length = length;
		memcpy(work->data, data, length);
	}

	g_mutex_lock(&mutex);
	works = g_list_append(works, work);
	g_cond_signal(&cond);
	g_mutex_unlock(&mutex);
}

void hinawa_context_stop_notifier(void)
{
	G_LOCK(th_lock);

	if (counter[TH_TYPE_NOTIFIER] == 0)
		stop_thread(TH_TYPE_NOTIFIER);

	G_UNLOCK(th_lock);
}
