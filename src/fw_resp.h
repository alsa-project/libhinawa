// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINAWA_FW_RESP_H__
#define __ORG_KERNEL_HINAWA_FW_RESP_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_RESP	(hinawa_fw_resp_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaFwResp, hinawa_fw_resp, HINAWA, FW_RESP, GObject)

#define HINAWA_FW_RESP_ERROR	hinawa_fw_resp_error_quark()

GQuark hinawa_fw_resp_error_quark();

struct _HinawaFwRespClass {
	GObjectClass parent_class;

	/**
	 * HinawaFwRespClass::requested:
	 * @self: A [class@FwResp]
	 * @tcode: One of [enum@FwTcode] enumerations
	 * @offset: The address offset at which the transaction arrives.
	 * @src_node_id: The node ID of source for the transaction.
	 * @dst_node_id: The node ID of destination for the transaction.
	 * @card_id: The index of card specific to 1394 OHCI hardware at which the request
	 *	     subaction arrived.
	 * @generation: The generation of bus when the transaction is transferred.
	 * @tstamp: The time stamp at which the request arrived.
	 * @frame: (element-type guint8)(array length=length): The array with elements for byte
	 *	   data.
	 * @length: The length of bytes for the frame.
	 *
	 * Class closure for the [signal@FwResp::requested] signal.
	 *
	 * Returns: One of [enum@FwRcode enumerations corresponding to rcodes defined in IEEE 1394
	 *	    specification.
	 *
	 * Since: 3.0
	 */
	HinawaFwRcode (*requested)(HinawaFwResp *self, HinawaFwTcode tcode, guint64 offset,
				   guint src_node_id, guint dst_node_id, guint card_id,
				   guint generation, guint tstamp,
				   const guint8 *frame, guint length);
};

HinawaFwResp *hinawa_fw_resp_new(void);

gboolean hinawa_fw_resp_reserve_within_region(HinawaFwResp *self, HinawaFwNode *node,
					      guint64 region_start, guint64 region_end,
					      guint width, GError **error);
gboolean hinawa_fw_resp_reserve(HinawaFwResp *self, HinawaFwNode*node, guint64 addr, guint width,
				GError **error);
void hinawa_fw_resp_release(HinawaFwResp *self);

void hinawa_fw_resp_set_resp_frame(HinawaFwResp *self, guint8 *frame,
				   gsize length);

G_END_DECLS

#endif
