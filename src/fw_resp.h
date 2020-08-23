/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __ALSA_HINAWA_FW_RESP_H__
#define __ALSA_HINAWA_FW_RESP_H__

#include <glib.h>
#include <glib-object.h>

#include <fw_node.h>
#include <hinawa_enums.h>

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

#define HINAWA_FW_RESP_ERROR	hinawa_fw_resp_error_quark()

GQuark hinawa_fw_resp_error_quark();

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
	 * When any node transfers requests to the range of address to which this object listening,
	 * the #HinawaFwRespClass::requested signal handler is called with #HinawaFwTcode, without the
	 * case that #HinawaFwRespClass::requested2 signal handler is already assigned.
	 *
	 * The handler can get data frame by a call of #hinawa_fw_resp_get_req_frame() and set data
	 * frame by a call of #hinawa_fw_resp_set_resp_frame(), then returns rcode.
	 *
	 * Returns: One of #HinawaRcode enumerators corresponding to rcodes defined in IEEE 1394
	 * specification.
	 *
	 * Deprecated: 2.2: Use #HinawaFwRespClass::requested2, instead.
	 */
	HinawaFwRcode (*requested)(HinawaFwResp *self, HinawaFwTcode tcode);

	/**
	 * HinawaFwRespClass::requested2:
	 * @self: A #HinawaFwResp
	 * @tcode: One of #HinawaTcode enumerations
	 * @offset: The address offset at which the transaction arrives.
	 * @src: The node ID of source for the transaction.
	 * @dst: The node ID of destination for the transaction.
	 * @card: The index of card corresponding to 1394 OHCI controller.
	 * @generation: The generation of bus when the transaction is transferred.
	 * @frame: (element-type guint8)(array length=length): The array with elements for byte
	 *	   data.
	 * @length: The length of bytes for the frame.
	 *
	 * When any node transfers request subaction to the range of address to which this object
	 * listening, the #HinawaFwResp::requested signal handler is called with arrived frame for
	 * the subaction. The handler is expected to call #hinawa_fw_resp_set_resp_frame() with
	 * frame and return rcode for response subaction.
	 *
	 * Returns: One of #HinawaRcode enumerators corresponding to rcodes defined in IEEE 1394
	 *	    specification.
	 */
	HinawaFwRcode (*requested2)(HinawaFwResp *self, HinawaFwTcode tcode, guint64 offset,
				    guint32 src, guint32 dst, guint32 card, guint32 generation,
				    const guint8 *frame, guint length);
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
