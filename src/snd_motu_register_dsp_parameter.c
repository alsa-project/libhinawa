/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "snd_motu_register_dsp_parameter.h"

/**
 * SECTION:snd_motu_register_dsp_parameter
 * @Title: HinawaSndMotuRegisterDspParameter
 * @Short_description: A boxed object for container of parameter in register DSP model
 * @include: snd_motu_register_dsp_parameter.h
 *
 * A #HinawaSndMotuRegisterDspParameter is a boxed object for container of parameter in register
 * DSP model.
 */
HinawaSndMotuRegisterDspParameter *register_dsp_parameter_copy(const HinawaSndMotuRegisterDspParameter *self)
{
#ifdef g_memdup2
    return g_memdup2(self, sizeof(*self));
#else
    // GLib v2.68 deprecated g_memdup() with concern about overflow by narrow conversion from size_t to
    // unsigned int however it's safe in the local case.
    gpointer ptr = g_malloc(sizeof(*self));
    memcpy(ptr, self, sizeof(*self));
    return ptr;
#endif
}

G_DEFINE_BOXED_TYPE(HinawaSndMotuRegisterDspParameter, hinawa_snd_motu_register_dsp_parameter, register_dsp_parameter_copy, g_free)

/**
 * hinawa_snd_motu_register_dsp_parameter_new:
 *
 * Instantiate #HinawaSndMotuRegisterDspParameter object and return the instance.
 *
 * Returns: an instance of #HinawaSndMotuRegisterDspParameter.
 * Since: 2.4
 */
HinawaSndMotuRegisterDspParameter *hinawa_snd_motu_register_dsp_parameter_new(void)
{
    return g_malloc0(sizeof(HinawaSndMotuRegisterDspParameter));
}
