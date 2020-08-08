/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "hinawa_error.h"

/**
 * SECTION:hinawa_error
 * @short_description: Represent domain for errors in libhinawa.
 * @include: hinawa_error.h
 *
 * GError with the domain has error code from errno.h for Linux system. No GLib
 * enumerations for the code is provided currently.
 */

/**
 * hinawa_error_quark:
 *
 * Retrieve GQuark as identifier for domain of error in libhinawa.
 *
 * Returns: A #GQuark;
 */
G_DEFINE_QUARK(hinawa-error-quark, hinawa_error)
