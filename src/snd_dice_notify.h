#ifndef __ALSA_TOOLS_HINAWA_SND_DICE_NOTIFY_H__
#define __ALSA_TOOLS_HINAWA_SND_DICE_NOTIFY_H__

#include <glib.h>
#include <glib-object.h>
#include "internal.h"
#include "snd_unit.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_DICE_NOTIFY	(hinawa_snd_dice_notify_get_type())

#define HINAWA_SND_DICE_NOTIFY(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),				\
				    HINAWA_TYPE_SND_DICE_NOTIFY,	\
				    HinawaSndDiceNotify))
#define HINAWA_IS_SND_DICE_NOTIFY(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),				\
				    HINAWA_TYPE_SND_DICE_NOTIFY))

#define HINAWA_SND_DICE_NOTIFY_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),				\
				 HINAWA_TYPE_SND_DICE_NOTIFY,		\
				 HinawaSndDiceNotifyClass))
#define HINAWA_IS_SND_DICE_NOTIFY_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),				\
				 HINAWA_TYPE_SND_DICE_NOTIFY))
#define HINAWA_SND_DICE_NOTIFY_GET_CLASS(obj) 				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),				\
				   HINAWA_TYPE_SND_DICE_NOTIFY,		\
				   HinawaSndDiceNotifyClass))

typedef struct _HinawaSndDiceNotify		HinawaSndDiceNotify;
typedef struct _HinawaSndDiceNotifyClass	HinawaSndDiceNotifyClass;
typedef struct _HinawaSndDiceNotifyPrivate	HinawaSndDiceNotifyPrivate;

struct _HinawaSndDiceNotify
{
	GObject parent_instance;

	HinawaSndDiceNotifyPrivate *priv;
};

struct _HinawaSndDiceNotifyClass
{
	GObjectClass parent_class;
};

GType hinawa_snd_dice_notify_get_type(void) G_GNUC_CONST;

HinawaSndDiceNotify *hinawa_snd_dice_notify_new(HinawaSndUnit *unit,
						GError **exception);
#endif
