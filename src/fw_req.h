/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_FW_REQ_H__
#define __ALSA_HINAWA_FW_REQ_H__

#include <glib.h>
#include <glib-object.h>

#include <fw_node.h>
#include <hinawa_enums.h>

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
				 HinawaFwReqClass))
#define HINAWA_IS_FW_REQ_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_FW_REQ))
#define HINAWA_FW_REQ_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_FW_REQ,		\
				   HinawaFwReqClass))

#define HINAWA_FW_REQ_ERROR	hinawa_fw_req_error_quark()

GQuark hinawa_fw_req_error_quark();

typedef struct _HinawaFwReq		HinawaFwReq;
typedef struct _HinawaFwReqClass	HinawaFwReqClass;
typedef struct _HinawaFwReqPrivate	HinawaFwReqPrivate;

struct _HinawaFwReq {
	GObject parent_instance;

	HinawaFwReqPrivate *priv;
};

struct _HinawaFwReqClass {
	GObjectClass parent_class;

	/**
	 * HinawaFwReqClass::responded:
	 * @self: A #HinawaFwReq.
	 * @rcode: One of #HinawaFwRcode.
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for transaction.
	 * @frame_size: The number of elements of the array.
	 */
	void (*responded)(HinawaFwReq *self, HinawaFwRcode rcode,
			  const guint8 *frame, guint frame_size);
};

GType hinawa_fw_req_get_type(void) G_GNUC_CONST;

HinawaFwReq *hinawa_fw_req_new(void);

void hinawa_fw_req_transaction(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 *const *frame, gsize *frame_size,
			       GError **exception);

G_END_DECLS

#endif
