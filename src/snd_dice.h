/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_TOOLS_HINAWA_SND_DICE_H__
#define __ALSA_TOOLS_HINAWA_SND_DICE_H__

#include <glib.h>
#include <glib-object.h>
#include "snd_unit.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_DICE	(hinawa_snd_dice_get_type())

#define HINAWA_SND_DICE(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_DICE,	\
				    HinawaSndDice))
#define HINAWA_IS_SND_DICE(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_DICE))

#define HINAWA_SND_DICE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_DICE,		\
				 HinawaSndDiceClass))
#define HINAWA_IS_SND_DICE_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_DICE))
#define HINAWA_SND_DICE_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_DICE,	\
				   HinawaSndDiceClass))

typedef struct _HinawaSndDice		HinawaSndDice;
typedef struct _HinawaSndDiceClass	HinawaSndDiceClass;
typedef struct _HinawaSndDicePrivate	HinawaSndDicePrivate;

struct _HinawaSndDice {
	HinawaSndUnit parent_instance;

	HinawaSndDicePrivate *priv;
};

struct _HinawaSndDiceClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndDiceClass::notified:
	 * @self: A #HinawaSndDice
	 * @message: A notification message
	 *
	 * When Dice models transfer notification, the ::notified handler is
	 * called.
	 */
	void (*notified)(HinawaSndDice *self, gulong message);
};

GType hinawa_snd_dice_get_type(void) G_GNUC_CONST;

HinawaSndDice *hinawa_snd_dice_new(void);

void hinawa_snd_dice_open(HinawaSndDice *self, gchar *path, GError **exception);

void hinawa_snd_dice_transact(HinawaSndDice *self, guint64 addr, GArray *frame,
			      guint32 bit_flag, GError **exception);

G_END_DECLS

#endif
