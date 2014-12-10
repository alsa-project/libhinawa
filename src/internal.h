#ifndef __ALSA_TOOLS_HINAWA_INTERNAL_H__
#define __ALSA_TOOLS_HINAWA_INTERNAL_H__

#include <errno.h>
#include <string.h>

#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

#include "backport.h"
#include "snd_unit.h"

typedef void (*HinawaFwUnitHandle)(int fd, union fw_cdev_event *ev);
void hinawa_fw_resp_handle_request(int fd, union fw_cdev_event *ev);
void hinawa_fw_req_handle_response(int fd, union fw_cdev_event *ev);

typedef void (HinawaSndUnitHandle)(void *private_data,
				   const void *buf, unsigned int len);
void hinawa_snd_unit_add_handle(HinawaSndUnit *self, unsigned int type,
				HinawaSndUnitHandle *handle,
				void *private_data, GError **exception);
void hinawa_snd_unit_remove_handle(HinawaSndUnit *self,
				   HinawaSndUnitHandle *handle);
#endif
