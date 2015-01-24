#ifndef __ALSA_TOOLS_HINAWA_FW_RESP_H__
#define __ALSA_TOOLS_HINAWA_FW_RESP_H__

#include <glib.h>
#include <glib-object.h>
#include "fw_unit.h"

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
				 HinawaFwResp))
#define HINAWA_IS_FW_RESP_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_Seq))
#define HINAWA_FW_RESP_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_FW_RESP,		\
				   HinawaFwResp))

typedef struct _HinawaFwResp		HinawaFwResp;
typedef struct _HinawaFwRespClass	HinawaFwRespClass;
typedef struct _HinawaFwRespPrivate	HinawaFwRespPrivate;

struct _HinawaFwResp {
	GObject parent_instance;

	HinawaFwRespPrivate *priv;
};

struct _HinawaFwRespClass {
	GObjectClass parent_class;
};

GType hinawa_fw_resp_get_type(void) G_GNUC_CONST;

void hinawa_fw_resp_register(HinawaFwResp *self, HinawaFwUnit *unit,
			     guint64 addr, guint width, GError **exception);
void hinawa_fw_resp_unregister(HinawaFwResp *self);
#endif
