/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "hinawa_context.h"

static struct {
	GMainContext *ctx;
	GThread *thread;
	gboolean running;
	gint counter;
	GMutex mutex;
} ctx_data[HINAWA_CONTEXT_TYPE_COUNT];

static G_LOCK_DEFINE(ctx_data_lock);

static gpointer run_main_loop(gpointer data)
{
	enum hinawa_context_type type = (enum hinawa_context_type)data;

	while (ctx_data[type].running)
		g_main_context_iteration(ctx_data[type].ctx, TRUE);

	g_thread_exit(NULL);

	return NULL;
}

static GMainContext *get_my_context(enum hinawa_context_type type,
				    GError **exception)
{
	const char *labels[] = {
		[HINAWA_CONTEXT_TYPE_FW]	= "fw",
		[HINAWA_CONTEXT_TYPE_SND]	= "snd",
	};

	if (!ctx_data[type].ctx)
		ctx_data[type].ctx = g_main_context_new();

	if (!ctx_data[type].thread) {
		char name[16];

		g_snprintf(name, sizeof(name), "libhinawa:%s", labels[type]);

		ctx_data[type].thread = g_thread_try_new(name, run_main_loop,
						(gpointer)type, exception);
		if (*exception) {
			g_main_context_unref(ctx_data[type].ctx);
			return NULL;
		} else {
			ctx_data[type].running = TRUE;
		}
	}

	g_atomic_int_inc(&ctx_data[type].counter);

	return ctx_data[type].ctx;
}

gpointer hinawa_context_add_src(enum hinawa_context_type type, GSource *src,
				gint fd, GIOCondition event, GError **exception)
{
	GMainContext *ctx;
	gpointer tag;

	G_LOCK(ctx_data_lock);

	ctx = get_my_context(type, exception);
	if (*exception) {
		G_UNLOCK(ctx_data_lock);
		return NULL;
	}

	/* NOTE: The returned ID is never used. */
	g_source_attach(src, ctx);

	tag = g_source_add_unix_fd(src, fd, event);

	G_UNLOCK(ctx_data_lock);

	return tag;
}

void hinawa_context_remove_src(enum hinawa_context_type type, GSource *src)
{
	G_LOCK(ctx_data_lock);

	if (g_atomic_int_dec_and_test(&ctx_data[type].counter)) {
		ctx_data[type].running = FALSE;
		g_thread_join(ctx_data[type].thread);
		g_thread_unref(ctx_data[type].thread);
		ctx_data[type].thread = NULL;

		// When the ctx has no source, it becomes to execute poll(2)
		// with -1 timeout and the thread doesn't exit without any UNIX
		// signal. To prevent this situation, desctuction of source is
		// done here.
		g_source_destroy(src);

		g_main_context_unref(ctx_data[type].ctx);
		ctx_data[type].ctx = NULL;
	} else {
		g_source_destroy(src);
	}

	G_UNLOCK(ctx_data_lock);
}
