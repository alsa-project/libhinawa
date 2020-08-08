/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_ERROR_H__
#define __ALSA_HINAWA_ERROR_H__

#include <glib.h>

GQuark hinawa_error_quark();

#define HINAWA_ERROR	hinawa_error_quark()

#endif
