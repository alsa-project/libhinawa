/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_SND_MOTU_REGISTER_DSP_PARAMETER_H__
#define __ALSA_HINAWA_SND_MOTU_REGISTER_DSP_PARAMETER_H__

#include <glib.h>
#include <glib-object.h>

#include <sound/firewire.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_MOTU_REGISTER_DSP_PARAMETER	(hinawa_snd_motu_register_dsp_parameter_get_type())

typedef struct {
	/*< private >*/
	guint8 parameter[512];
	// TODO: when UAPI header of ALSA firewire stack is enough disseminated, the above 512 byte
	// data can be replaced with actual structure.
} HinawaSndMotuRegisterDspParameter;

GType hinawa_snd_motu_register_dsp_parameter_get_type() G_GNUC_CONST;

HinawaSndMotuRegisterDspParameter *hinawa_snd_motu_register_dsp_parameter_new(void);

G_END_DECLS

#endif
