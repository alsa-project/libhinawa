/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_SND_DG00X_H__
#define __ALSA_HINAWA_SND_DG00X_H__

#include <glib.h>
#include <glib-object.h>

#include <snd_unit.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_DG00X	(hinawa_snd_dg00x_get_type())

#define HINAWA_SND_DG00X(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_DG00X,	\
				    HinawaSndDg00x))
#define HINAWA_IS_SND_DG00X(obj)				\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_DG00X))

#define HINAWA_SND_DG00X_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_DG00X,		\
				 HinawaSndDg00xClass))
#define HINAWA_IS_SND_DG00X_CLASS(klass)			\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_DG00X))
#define HINAWA_SND_DG00X_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_DG00X,	\
				   HinawaSndDg00xClass))

typedef struct _HinawaSndDg00x		HinawaSndDg00x;
typedef struct _HinawaSndDg00xClass	HinawaSndDg00xClass;

struct _HinawaSndDg00x {
	HinawaSndUnit parent_instance;
};

struct _HinawaSndDg00xClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndDg00xClass::message:
	 * @self: A #HinawaSndDg00x
	 * @message: A message
	 *
	 * When Dg00x models transfer notification, the ::message handler is
	 * called.
	 */
	void (*message)(HinawaSndDg00x *self, guint32 message);
};

GType hinawa_snd_dg00x_get_type(void) G_GNUC_CONST;

HinawaSndDg00x *hinawa_snd_dg00x_new(void);

void hinawa_snd_dg00x_open(HinawaSndDg00x *self, gchar *path,
			   GError **exception);

G_END_DECLS

#endif
