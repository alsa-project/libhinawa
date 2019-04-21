/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "internal.h"

enum th_type {
	TH_TYPE_DISPATCHER = 0,
	TH_TYPE_COUNT,
};

static gint counter;

static GThread *th[TH_TYPE_COUNT];
static gboolean running[TH_TYPE_COUNT];
static G_LOCK_DEFINE(th_lock);

// For dispatcher thread.
static GMainContext *ctx;

static gpointer run_dispacher(gpointer data)
{
	while (running[TH_TYPE_DISPATCHER])
		g_main_context_iteration(ctx, TRUE);

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
	};
	char name[16];

	if (th[type] != NULL)
		return;

	if (type == TH_TYPE_DISPATCHER) {
		ctx = g_main_context_new();
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

	g_thread_join(th[type]);
	g_thread_unref(th[type]);
	th[type] = NULL;

	if (type == TH_TYPE_DISPATCHER) {
		g_main_context_unref(ctx);
		ctx = NULL;
	}
}

void hinawa_context_add_src(GSource *src, GError **exception)
{
	G_LOCK(th_lock);

	if (counter == 0) {
		// For dispatcher thread.
		create_thread(TH_TYPE_DISPATCHER, exception);
		if (*exception != NULL) {
			g_main_context_unref(ctx);
			goto end;
		}

		++counter;
	}

	// NOTE: The returned ID is never used.
	g_source_attach(src, ctx);
end:
	G_UNLOCK(th_lock);
}

void hinawa_context_remove_src(GSource *src)
{
	G_LOCK(th_lock);

	if (--counter == 0) {
		stop_thread(TH_TYPE_DISPATCHER);
	}

	G_UNLOCK(th_lock);

	g_source_destroy(src);
}
