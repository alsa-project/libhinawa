/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_SND_MOTU_H__
#define __ALSA_HINAWA_SND_MOTU_H__

#include <glib.h>
#include <glib-object.h>
#include <snd_unit.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_MOTU	(hinawa_snd_motu_get_type())

#define HINAWA_SND_MOTU(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_MOTU,	\
				    HinawaSndMotu))
#define HINAWA_IS_SND_MOTU(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_MOTU))

#define HINAWA_SND_MOTU_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_MOTU,		\
				 HinawaSndMotuClass))
#define HINAWA_IS_SND_MOTU_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_MOTU))
#define HINAWA_SND_MOTU_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_MOTU,	\
				   HinawaSndMotuClass))

typedef struct _HinawaSndMotu		HinawaSndMotu;
typedef struct _HinawaSndMotuClass	HinawaSndMotuClass;
typedef struct _HinawaSndMotuPrivate	HinawaSndMotuPrivate;

struct _HinawaSndMotu {
	HinawaSndUnit parent_instance;

	HinawaSndMotuPrivate *priv;
};

struct _HinawaSndMotuClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndMotuClass::notified:
	 * @self: A #HinawaSndMotu
	 * @message: A notification message
	 *
	 * When Motu models transfer notification, the ::notified handler is
	 * called.
	 */
	void (*notified)(HinawaSndMotu *self, guint message);
};

GType hinawa_snd_motu_get_type(void) G_GNUC_CONST;

HinawaSndMotu *hinawa_snd_motu_new(void);

void hinawa_snd_motu_open(HinawaSndMotu *self, gchar *path, GError **exception);

G_END_DECLS

#endif
