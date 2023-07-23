// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINAWA_FW_REQ_H__
#define __ORG_KERNEL_HINAWA_FW_REQ_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_REQ	(hinawa_fw_req_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaFwReq, hinawa_fw_req, HINAWA, FW_REQ, GObject)

#define HINAWA_FW_REQ_ERROR	hinawa_fw_req_error_quark()

GQuark hinawa_fw_req_error_quark();

struct _HinawaFwReqClass {
	GObjectClass parent_class;

	/**
	 * HinawaFwReqClass::responded:
	 * @self: A [class@FwReq].
	 * @rcode: One of [enum@FwRcode].
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for transaction.
	 * @frame_size: The number of elements of the array.
	 *
	 * Class closure for the [signal@FwReq::responded] signal.
	 *
	 * Since: 2.1
	 * Deprecated: 2.6: Use [vfunc@FwReq.responded2], instead.
	 */
	void (*responded)(HinawaFwReq *self, HinawaFwRcode rcode,
			  const guint8 *frame, guint frame_size);

	/**
	 * HinawaFwReqClass::responded2:
	 * @self: A [class@FwReq].
	 * @rcode: One of [enum@FwRcode].
	 * @request_tstamp: The isochronous cycle at which the request was sent.
	 * @response_tstamp: The isochronous cycle at which the response arrived.
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for transaction.
	 * @frame_size: The number of elements of the array.
	 *
	 * Class closure for the [signal@FwReq::responded2] signal.
	 *
	 * Since: 2.6
	 */
	void (*responded2)(HinawaFwReq *self, HinawaFwRcode rcode, guint request_tstamp,
			   guint response_tstamp, const guint8 *frame, guint frame_size);
};

HinawaFwReq *hinawa_fw_req_new(void);

gboolean hinawa_fw_req_request(HinawaFwReq *self, HinawaFwNode *node, HinawaFwTcode tcode,
			       guint64 addr, gsize length, guint8 *const *frame, gsize *frame_size,
			       GError **error);

gboolean hinawa_fw_req_transaction_with_tstamp(HinawaFwReq *self, HinawaFwNode *node,
					       HinawaFwTcode tcode, guint64 addr, gsize length,
					       guint8 **frame, gsize *frame_size, guint tstamp[2],
					       guint timeout_ms, GError **error);

gboolean hinawa_fw_req_transaction(HinawaFwReq *self, HinawaFwNode *node,
				   HinawaFwTcode tcode, guint64 addr, gsize length,
				   guint8 **frame, gsize *frame_size, guint timeout_ms,
				   GError **error);

G_END_DECLS

#endif
