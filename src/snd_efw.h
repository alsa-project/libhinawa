// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_EFW_H__
#define __ALSA_HINAWA_SND_EFW_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_EFW	(hinawa_snd_efw_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaSndEfw, hinawa_snd_efw, HINAWA, SND_EFW, HinawaSndUnit);

#define HINAWA_SND_EFW_ERROR	hinawa_snd_efw_error_quark()

GQuark hinawa_snd_efw_error_quark();

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

HinawaSndEfw *hinawa_snd_efw_new(void);

void hinawa_snd_efw_open(HinawaSndEfw *self, gchar *path, GError **error);

void hinawa_snd_efw_transaction_async(HinawaSndEfw *self, guint category, guint command,
				      const guint32 *args, gsize arg_count, guint32 *resp_seqnum,
				      GError **error);

void hinawa_snd_efw_transaction(HinawaSndEfw *self,
				guint category, guint command,
				const guint32 *args, gsize arg_count,
				guint32 *const *params, gsize *param_count,
				GError **error);

void hinawa_snd_efw_transaction_sync(HinawaSndEfw *self, guint category, guint command,
				     const guint32 *args, gsize arg_count,
				     guint32 *const *params, gsize *param_count,
				     guint timeout_ms, GError **error);

G_END_DECLS

#endif
