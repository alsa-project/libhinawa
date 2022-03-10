// SPDX-License-Identifier: LGPL-2.1-or-later
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

void hinawa_snd_motu_register_dsp_parameter_get_mixer_output_paired_volume(
	const HinawaSndMotuRegisterDspParameter *self, const guint8 *volume[4]);

void hinawa_snd_motu_register_dsp_parameter_get_mixer_output_paired_flag(
	const HinawaSndMotuRegisterDspParameter *self, const guint8 *flag[4]);

void hinawa_snd_motu_register_dsp_parameter_get_main_output_paired_volume(
	const HinawaSndMotuRegisterDspParameter *self, guint8 *volume);

void hinawa_snd_motu_register_dsp_parameter_get_headphone_output_paired_volume(
	const HinawaSndMotuRegisterDspParameter *self, guint8 *volume);

void hinawa_snd_motu_register_dsp_parameter_get_headphone_output_paired_assignment(
	const HinawaSndMotuRegisterDspParameter *self, guint8 *assignment);

void hinawa_snd_motu_register_dsp_parameter_get_line_input_boost_flag(
	const HinawaSndMotuRegisterDspParameter *self, guint8 *boost_flag);

void hinawa_snd_motu_register_dsp_parameter_get_line_input_nominal_level_flag(
	const HinawaSndMotuRegisterDspParameter *self, guint8 *nominal_level_flag);

void hinawa_snd_motu_register_dsp_parameter_get_input_gain_and_invert(
	const HinawaSndMotuRegisterDspParameter *self, const guint8 *gain_and_invert[10]);

void hinawa_snd_motu_register_dsp_parameter_get_input_flag(
	const HinawaSndMotuRegisterDspParameter *self, const guint8 *flag[10]);

G_END_DECLS

#endif
