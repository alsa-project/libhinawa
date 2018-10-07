/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_TOOLS_HINAWA_CONTEXT_H__
#define __ALSA_TOOLS_HINAWA_CONTEXT_H__

#include <glib.h>
#include <glib-object.h>

enum hinawa_context_type {
	HINAWA_CONTEXT_TYPE_FW = 0,
	HINAWA_CONTEXT_TYPE_SND,
	HINAWA_CONTEXT_TYPE_COUNT,
};

gpointer hinawa_context_add_src(enum hinawa_context_type type, GSource *src,
				gint fd, GIOCondition event, GError **exception);
void hinawa_context_remove_src(enum hinawa_context_type type, GSource *src);
#endif
