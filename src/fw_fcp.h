/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_TOOLS_HINAWA_FW_FCP_H__
#define __ALSA_TOOLS_HINAWA_FW_FCP_H__

#include <glib.h>
#include <glib-object.h>
#include "fw_resp.h"

#include <hinawa_enums.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_FCP	(hinawa_fw_fcp_get_type())

#define HINAWA_FW_FCP(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_FW_FCP,		\
				    HinawaFwFcp))
#define HINAWA_IS_FW_FCP(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_FW_FCP))

#define HINAWA_FW_FCP_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_FW_FCP,		\
				 HinawaFwFcpClass))
#define HINAWA_IS_FW_FCP_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_FW_FCP))
#define HINAWA_FW_FCP_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_FW_FCP,		\
				   HinawaFwFcpClass))

typedef struct _HinawaFwFcp		HinawaFwFcp;
typedef struct _HinawaFwFcpClass	HinawaFwFcpClass;
typedef struct _HinawaFwFcpPrivate	HinawaFwFcpPrivate;

struct _HinawaFwFcp {
	HinawaFwResp parent_instance;

	HinawaFwFcpPrivate *priv;
};

struct _HinawaFwFcpClass {
	HinawaFwRespClass parent_class;
};

GType hinawa_fw_fcp_get_type(void) G_GNUC_CONST;

HinawaFwFcp *hinawa_fw_fcp_new(void);

void hinawa_fw_fcp_transaction(HinawaFwFcp *self,
			       const guint8 *req_frame, guint req_frame_size,
			       guint8 *const *resp_frame, guint *resp_frame_size,
			       GError **exception);

void hinawa_fw_fcp_bind(HinawaFwFcp *self, HinawaFwNode *node,
			GError **exception);
void hinawa_fw_fcp_unbind(HinawaFwFcp *self);

G_END_DECLS

#endif
