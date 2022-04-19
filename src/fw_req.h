// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_FW_REQ_H__
#define __ALSA_HINAWA_FW_REQ_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_REQ	(hinawa_fw_req_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaFwReq, hinawa_fw_req, HINAWA, FW_REQ, GObject);

#define HINAWA_FW_REQ_ERROR	hinawa_fw_req_error_quark()

GQuark hinawa_fw_req_error_quark();

struct _HinawaFwReqClass {
	GObjectClass parent_class;

	/**
	 * HinawaFwReqClass::responded:
	 * @self: A #HinawaFwReq.
	 * @rcode: One of #HinawaFwRcode.
	 * @frame: (array length=frame_size)(element-type guint8): The array with elements for
	 *	   byte data of response subaction for transaction.
	 * @frame_size: The number of elements of the array.
	 *
	 * When the unit transfers asynchronous packet as response subaction for the transaction,
	 * and the process successfully reads the content of packet from Linux firewire subsystem,
	 * the #HinawaFwReqClass::responded handler is called.
	 *
	 * Since: 2.1
	 */
	void (*responded)(HinawaFwReq *self, HinawaFwRcode rcode,
			  const guint8 *frame, guint frame_size);
};

HinawaFwReq *hinawa_fw_req_new(void);

void hinawa_fw_req_transaction_async(HinawaFwReq *self, HinawaFwNode *node,
				     HinawaFwTcode tcode, guint64 addr, gsize length,
				     guint8 *const *frame, gsize *frame_size,
				     GError **error);

void hinawa_fw_req_transaction_sync(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 *const *frame, gsize *frame_size, guint timeout_ms,
			       GError **error);

void hinawa_fw_req_transaction(HinawaFwReq *self, HinawaFwNode *node,
			       HinawaFwTcode tcode, guint64 addr, gsize length,
			       guint8 *const *frame, gsize *frame_size,
			       GError **error);

G_END_DECLS

#endif
