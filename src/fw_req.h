#ifndef __ALSA_TOOLS_HINAWA_FW_REQ_H__
#define __ALSA_TOOLS_HINAWA_FW_REQ_H__

#include <glib.h>
#include <glib-object.h>
#include "fw_unit.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_REQ	(hinawa_fw_req_get_type())

#define HINAWA_FW_REQ(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_FW_REQ,		\
				    HinawaFwReq))
#define HINAWA_IS_FW_REQ(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_FW_REQ))

#define HINAWA_FW_REQ_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_FW_REQ,		\
				 HinawaFwReq))
#define HINAWA_IS_FW_REQ_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_Seq))
#define HINAWA_FW_REQ_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_FW_REQ,		\
				   HinawaFwReq))

typedef struct _HinawaFwReq		HinawaFwReq;
typedef struct _HinawaFwReqClass	HinawaFwReqClass;
typedef struct _HinawaFwReqPrivate	HinawaFwReqPrivate;

struct _HinawaFwReq {
	GObject parent_instance;

	HinawaFwReqPrivate *priv;
};

struct _HinawaFwReqClass {
	GObjectClass parent_class;
};

GType hinawa_fw_req_get_type(void) G_GNUC_CONST;

void hinawa_fw_req_write(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			 GArray *frame, GError **exception);

void hinawa_fw_req_read(HinawaFwReq *self, HinawaFwUnit *unit, guint64 addr,
			GArray *frame, guint quads, GError **exception);

void hinawa_fw_req_lock(HinawaFwReq *self, HinawaFwUnit *unit,
			guint64 addr, GArray **frame, GError **exception);
#endif
