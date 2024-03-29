// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ORG_KERNEL_HINAWA_INTERNAL_H__
#define __ORG_KERNEL_HINAWA_INTERNAL_H__

#include "hinawa.h"

int hinawa_fw_node_ioctl(HinawaFwNode *self, unsigned long req, void *args, GError **exception);
void hinawa_fw_node_invalidate_transaction(HinawaFwNode *self, HinawaFwReq *req);

void hinawa_fw_resp_handle_request(HinawaFwResp *self, const struct fw_cdev_event_request *event);
void hinawa_fw_resp_handle_request2(HinawaFwResp *self, const struct fw_cdev_event_request2 *event);
void hinawa_fw_resp_handle_request3(HinawaFwResp *self, const struct fw_cdev_event_request3 *event);
void hinawa_fw_req_handle_response(HinawaFwReq *self, const struct fw_cdev_event_response *event);
void hinawa_fw_req_handle_response2(HinawaFwReq *self, const struct fw_cdev_event_response2 *event);

#endif
