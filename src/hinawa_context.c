#include "hinawa_context.h"

static GMainContext *ctx;
static GThread *thread;

static gboolean running;
static gint counter;

static gpointer run_main_loop(gpointer data)
{
	while (running)
		g_main_context_iteration(ctx, TRUE);

	g_thread_exit(NULL);

	return NULL;
}

static GMainContext *get_my_context(GError **exception)
{
	if (ctx == NULL)
		ctx = g_main_context_new();

	if (thread == NULL) {
		thread = g_thread_try_new("gmain", run_main_loop, NULL,
					  exception);
		if (*exception != NULL) {
			g_main_context_unref(ctx);
			ctx = NULL;
		}
	}

	return ctx;
}

gpointer hinawa_context_add_src(GSource *src, gint fd, GIOCondition event,
				GError **exception)
{
	GMainContext *ctx;

	ctx = get_my_context(exception);
	if (*exception != NULL)
		return NULL;
	running = TRUE;

	/* NOTE: The returned ID is never used. */
	g_source_attach(src, ctx);

	return g_source_add_unix_fd(src, fd, event);
}

void hinawa_context_remove_src(GSource *src)
{
	g_source_destroy(src);
	if (g_atomic_int_dec_and_test(&counter)) {
		running = FALSE;
		g_thread_join(thread);
		thread = NULL;
	}
}
