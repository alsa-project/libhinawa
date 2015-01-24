#ifndef __ALSA_TOOLS_HINAWA_SND_EFW_H__
#define __ALSA_TOOLS_HINAWA_SND_EFW_H__

#include <glib.h>
#include <glib-object.h>
#include "snd_unit.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_EFW	(hinawa_snd_efw_get_type())

#define HINAWA_SND_EFW(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_EFW,	\
				    HinawaSndEfw))
#define HINAWA_IS_SND_EFW(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_EFW))

#define HINAWA_SND_EFW_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_EFW,		\
				 HinawaSndEfwClass))
#define HINAWA_IS_SND_EFW_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_EFW))
#define HINAWA_SND_EFW_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_EFW,		\
				   HinawaSndEfwClass))

typedef struct _HinawaSndEfw		HinawaSndEfw;
typedef struct _HinawaSndEfwClass	HinawaSndEfwClass;
typedef struct _HinawaSndEfwPrivate	HinawaSndEfwPrivate;

struct _HinawaSndEfw {
	HinawaSndUnit parent_instance;

	HinawaSndEfwPrivate *priv;
};

struct _HinawaSndEfwClass {
	HinawaSndUnitClass parent_class;
};

GType hinawa_snd_efw_get_type(void) G_GNUC_CONST;

void hinawa_snd_efw_open(HinawaSndEfw *self, gchar *path, GError **exception);

void hinawa_snd_efw_transact(HinawaSndEfw *self, guint category, guint command,
			     GArray *args, GArray *params,
			     GError **exception);

#endif
