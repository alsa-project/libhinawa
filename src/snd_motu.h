// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_MOTU_H__
#define __ALSA_HINAWA_SND_MOTU_H__

#include <hinawa.h>

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
	 * When Motu models transfer notification, the #HinawaSndMotuClass::notified handler is
	 * called.
	 *
	 * Since: 1.2
	 */
	void (*notified)(HinawaSndMotu *self, guint message);

	/**
	 * HinawaSndMotuClass::register_dsp_changed:
	 * @self: A #HinawaSndMotu
	 * @events: (element-type guint32)(array length=length): The array with element for
	 *	    unsigned 32 bit encoded data.
	 * @length: The length of events.
	 *
	 * When MOTU register DSP models transfer events by messages in the sequence of isochronous
	 * packet, the #HinawaSndMotuClass::register_dsp_changed handle is called.
	 * The event consists of encoded data. The most significant byte is the type of message. The
	 * next two bytes are identifier 0 and 1. The least significant byte is value. The meaning
	 * of identifier 0, 1 and value is decided depending on the type. For detail, see
	 * `sound/firewire/motu/motu-register-dsp-message-parser.c` in Linux kernel.
	 *
	 * Since: 2.4
	 */
	void (*register_dsp_changed)(HinawaSndMotu *self, const guint32 *events, guint length);
};

GType hinawa_snd_motu_get_type(void) G_GNUC_CONST;

HinawaSndMotu *hinawa_snd_motu_new(void);

void hinawa_snd_motu_open(HinawaSndMotu *self, gchar *path, GError **exception);

void hinawa_snd_motu_read_register_dsp_parameter(HinawaSndMotu *self,
						 HinawaSndMotuRegisterDspParameter *const *param,
						 GError **exception);

void hinawa_snd_motu_read_register_dsp_meter(HinawaSndMotu *self, guint8 *const meter[48],
					     GError **exception);

void hinawa_snd_motu_read_command_dsp_meter(HinawaSndMotu *self, gfloat *const meter[400],
					    GError **exception);

G_END_DECLS

#endif
