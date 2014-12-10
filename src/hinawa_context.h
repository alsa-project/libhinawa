#ifndef __ALSA_TOOLS_HINAWA_CONTEXT_H__
#define __ALSA_TOOLS_HINAWA_CONTEXT_H__

#include <glib.h>
#include <glib-object.h>

gpointer hinawa_context_add_src(GSource *src, gint fd, GIOCondition event,
				GError **exception);

#endif
