/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_SND_EFW_H__
#define __ALSA_HINAWA_SND_EFW_H__

#include <glib.h>
#include <glib-object.h>

#include <snd_unit.h>

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

#define HINAWA_SND_EFW_ERROR	hinawa_snd_efw_error_quark()

GQuark hinawa_snd_efw_error_quark();

typedef struct _HinawaSndEfw		HinawaSndEfw;
typedef struct _HinawaSndEfwClass	HinawaSndEfwClass;
typedef struct _HinawaSndEfwPrivate	HinawaSndEfwPrivate;

struct _HinawaSndEfw {
	HinawaSndUnit parent_instance;

	HinawaSndEfwPrivate *priv;
};

struct _HinawaSndEfwClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndEfwClass::responded:
	 * @self: A #HinawaSndEfw.
	 * @status: One of #HinawaSndEfwStatus.
	 * @seqnum: The sequence number of response.
	 * @category: The value of category field in the response.
	 * @command: The value of command field in the response.
	 * @frame: (array length=frame_size)(element-type guint32): The array with elements for
	 *	   quadlet data of response for Echo Fireworks protocol.
	 * @frame_size: The number of elements of the array.
	 *
	 * When the unit transfers asynchronous packet as response for Echo Audio Fireworks
	 * protocol, and the process successfully reads the content of response from ALSA
	 * Fireworks driver, the #HinawaSndEfwClass::responded signal handler is called with parameters
	 * of the response.
	 *
	 * Since: 2.1
	 */
	void (*responded)(HinawaSndEfw *self, HinawaSndEfwStatus status, guint seqnum,
			  guint category, guint command, const guint32 *frame, guint frame_size);
};

GType hinawa_snd_efw_get_type(void) G_GNUC_CONST;

HinawaSndEfw *hinawa_snd_efw_new(void);

void hinawa_snd_efw_open(HinawaSndEfw *self, gchar *path, GError **exception);

void hinawa_snd_efw_transaction_async(HinawaSndEfw *self, guint category, guint command,
				      const guint32 *args, gsize arg_count, guint32 *resp_seqnum,
				      GError **exception);

void hinawa_snd_efw_transaction(HinawaSndEfw *self,
				guint category, guint command,
				const guint32 *args, gsize arg_count,
				guint32 *const *params, gsize *param_count,
				GError **exception);

void hinawa_snd_efw_transaction_sync(HinawaSndEfw *self, guint category, guint command,
				     const guint32 *args, gsize arg_count,
				     guint32 *const *params, gsize *param_count,
				     guint timeout_ms, GError **exception);

G_END_DECLS

#endif
