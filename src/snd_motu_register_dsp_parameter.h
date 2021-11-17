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

void hinawa_snd_motu_register_dsp_parameter_get_mixer_source_gain(
	const HinawaSndMotuRegisterDspParameter *self, gsize mixer, const guint8 *gain[20]);

void hinawa_snd_motu_register_dsp_parameter_get_mixer_source_pan(
	const HinawaSndMotuRegisterDspParameter *self, gsize mixer, const guint8 *pan[20]);

void hinawa_snd_motu_register_dsp_parameter_get_mixer_source_flag(
	const HinawaSndMotuRegisterDspParameter *self, gsize mixer, const guint8 *flag[20]);

void hinawa_snd_motu_register_dsp_parameter_get_mixer_source_paired_balance(
	const HinawaSndMotuRegisterDspParameter *self, gsize mixer, const guint8 *balance[20]);

void hinawa_snd_motu_register_dsp_parameter_get_mixer_source_paired_width(
	const HinawaSndMotuRegisterDspParameter *self, gsize mixer, const guint8 *width[20]);

G_END_DECLS

#endif
