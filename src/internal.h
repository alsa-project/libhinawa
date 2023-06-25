// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_INTERNAL_H__
#define __ALSA_HINAWA_INTERNAL_H__

#include "hinawa.h"

int hinawa_fw_node_ioctl(HinawaFwNode *self, unsigned long req, void *args, GError **exception);
void hinawa_fw_node_invalidate_transaction(HinawaFwNode *self, HinawaFwReq *req);

void hinawa_fw_resp_handle_request(HinawaFwResp *self, const struct fw_cdev_event_request *event);
void hinawa_fw_resp_handle_request2(HinawaFwResp *self, const struct fw_cdev_event_request2 *event);
void hinawa_fw_resp_handle_request3(HinawaFwResp *self, const struct fw_cdev_event_request3 *event);
void hinawa_fw_req_handle_response(HinawaFwReq *self, const struct fw_cdev_event_response *event);

void hinawa_snd_unit_write(HinawaSndUnit *self, const void *buf, size_t length,
			   GError **exception);

void hinawa_snd_unit_ioctl(HinawaSndUnit *self, unsigned long request,
			   void *arg, GError **exception);

void hinawa_snd_dice_handle_notification(HinawaSndDice *self,
					 const void *buf, ssize_t len);
void hinawa_snd_efw_handle_response(HinawaSndEfw *self,
				    const void *buf, ssize_t len);
void hinawa_snd_dg00x_handle_msg(HinawaSndDg00x *self, const void *buf,
				 ssize_t len);
void hinawa_snd_motu_handle_notification(HinawaSndMotu *self,
					 const void *buf, ssize_t len);
void hinawa_snd_motu_handle_register_dsp_change(HinawaSndMotu *self, const void *buf, ssize_t len);
void hinawa_snd_tscm_handle_control(HinawaSndTscm *self, const void *buf,
				    ssize_t len);

#endif
