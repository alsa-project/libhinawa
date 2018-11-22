#ifndef __ALSA_TOOLS_HINAWA_SND_TSCM_H__
#define __ALSA_TOOLS_HINAWA_SND_TSCM_H__

#include <glib.h>
#include <glib-object.h>
#include "snd_unit.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_TSCM	(hinawa_snd_tscm_get_type())

#define HINAWA_SND_TSCM(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_TSCM,	\
				    HinawaSndTscm))
#define HINAWA_IS_SND_TSCM(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_TSCM))

#define HINAWA_SND_TSCM_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_TSCM,		\
				 HinawaSndTscmClass))
#define HINAWA_IS_SND_TSCM_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_TSCM))
#define HINAWA_SND_TSCM_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_TSCM,	\
				   HinawaSndTscmClass))

typedef struct _HinawaSndTscm		HinawaSndTscm;
typedef struct _HinawaSndTscmClass	HinawaSndTscmClass;

struct _HinawaSndTscm {
	HinawaSndUnit parent_instance;
};

struct _HinawaSndTscmClass {
	HinawaSndUnitClass parent_class;
};

GType hinawa_snd_tscm_get_type(void) G_GNUC_CONST;

void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path, GError **exception);

#endif
