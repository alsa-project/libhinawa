/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_TOOLS_HINAWA_FW_RESP_H__
#define __ALSA_TOOLS_HINAWA_FW_RESP_H__

#include <glib.h>
#include <glib-object.h>
#include "hinawa_enums.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_RESP	(hinawa_fw_resp_get_type())

#define HINAWA_FW_RESP(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_FW_RESP,	\
				    HinawaFwResp))
#define HINAWA_IS_FW_RESP(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_FW_RESP))

#define HINAWA_FW_RESP_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_FW_RESP,		\
				 HinawaFwRespClass))
#define HINAWA_IS_FW_RESP_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_FW_RESP))
#define HINAWA_FW_RESP_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_FW_RESP,		\
				   HinawaFwRespClass))

typedef struct _HinawaFwResp		HinawaFwResp;
typedef struct _HinawaFwRespClass	HinawaFwRespClass;
typedef struct _HinawaFwRespPrivate	HinawaFwRespPrivate;

struct _HinawaFwResp {
	GObject parent_instance;

	HinawaFwRespPrivate *priv;
};

struct _HinawaFwRespClass {
	GObjectClass parent_class;

	/**
	 * HinawaFwRespClass::requested:
	 * @self: A #HinawaFwResp
	 * @tcode: One of #HinawaTcode enumerators
	 *
	 * When any units transfer requests to the range of address to which
	 * this object listening. The ::requested signal handler can get data
	 * frame by a call of ::get_req_frame and set data frame by a call of
	 * ::set_resp_frame, then returns rcode.
	 *
	 * Returns: One of #HinawaRcode enumerators corresponding to rcodes
	 * 	    defined in IEEE 1394 specification.
	 */
	HinawaFwRcode (*requested)(HinawaFwResp *self, HinawaFwTcode tcode);
};

GType hinawa_fw_resp_get_type(void) G_GNUC_CONST;

HinawaFwResp *hinawa_fw_resp_new(void);

void hinawa_fw_resp_reserve(HinawaFwResp *self, HinawaFwNode*node,
			    guint64 addr, guint width, GError **exception);
void hinawa_fw_resp_release(HinawaFwResp *self);

void hinawa_fw_resp_get_req_frame(HinawaFwResp *self, const guint8 **frame,
				  gsize *length);
void hinawa_fw_resp_set_resp_frame(HinawaFwResp *self, guint8 *frame,
				   gsize length);

G_END_DECLS

#endif
