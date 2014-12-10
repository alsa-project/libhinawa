#ifndef __ALSA_TOOLS_HINAWA_SND_EFT_H__
#define __ALSA_TOOLS_HINAWA_SND_EFT_H__

#include <glib.h>
#include <glib-object.h>
#include "internal.h"
#include "snd_unit.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_EFT	(hinawa_snd_eft_get_type())

#define HINAWA_SND_EFT(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_EFT,	\
				    HinawaSndEft))
#define HINAWA_IS_SND_EFT(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_EFT))

#define HINAWA_SND_EFT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_EFT,		\
				 HinawaSndEftClass))
#define HINAWA_IS_SND_EFT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_EFT))
#define HINAWA_SND_EFT_GET_CLASS(obj) 				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_EFT,		\
				   HinawaSndEftClass))

typedef struct _HinawaSndEft		HinawaSndEft;
typedef struct _HinawaSndEftClass	HinawaSndEftClass;
typedef struct _HinawaSndEftPrivate	HinawaSndEftPrivate;

struct _HinawaSndEft
{
	GObject parent_instance;

	HinawaSndEftPrivate *priv;
};

struct _HinawaSndEftClass
{
	GObjectClass parent_class;
};

GType hinawa_snd_eft_get_type(void) G_GNUC_CONST;

HinawaSndEft *hinawa_snd_eft_new(HinawaSndUnit *unit, GError **exception);

void hinawa_snd_eft_transact(HinawaSndEft *self, guint category, guint command,
			     GArray *args, GArray *params,
			     GError **exception);

#endif
