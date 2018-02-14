/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_TOOLS_HINAWA_CONTEXT_H__
#define __ALSA_TOOLS_HINAWA_CONTEXT_H__

#include <glib.h>
#include <glib-object.h>

gpointer hinawa_context_add_src(GSource *src, gint fd, GIOCondition event,
				GError **exception);

#endif
