#ifndef __ALSA_TOOLS_HINAWA_INTERNAL_H__
#define __ALSA_TOOLS_HINAWA_INTERNAL_H__

#include <errno.h>
#include <string.h>

#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

#include "backport.h"
#include "fw_unit.h"
#include "fw_resp.h"
#include "fw_req.h"
#include "snd_unit.h"
#include "snd_dice.h"
#include "snd_efw.h"
#include "snd_dg00x.h"

void hinawa_fw_unit_ioctl(HinawaFwUnit *self, int req, void *args, int *err);
void hinawa_fw_resp_handle_request(HinawaFwResp *self,
				   struct fw_cdev_event_request2 *event);
void hinawa_fw_req_handle_response(HinawaFwReq *self,
				   struct fw_cdev_event_response *event);

void hinawa_snd_unit_read(HinawaSndUnit *unit,
			  void *buf, unsigned int length,
			  GError **exception);
void hinawa_snd_unit_write(HinawaSndUnit *unit,
			   const void *buf, unsigned int length,
			   GError **exception);
void hinawa_snd_dice_handle_notification(HinawaSndDice *self,
					 const void *buf, unsigned int len);
void hinawa_snd_efw_handle_response(HinawaSndEfw *self,
				    const void *buf, unsigned int len);
void hinawa_snd_dg00x_handle_msg(HinawaSndDg00x *self, const void *buf,
				 unsigned int len);
#endif
