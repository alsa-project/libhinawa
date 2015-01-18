#ifndef __ALSA_TOOLS_HINAWA_INTERNAL_H__
#define __ALSA_TOOLS_HINAWA_INTERNAL_H__

#include <errno.h>
#include <string.h>

#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

#include "backport.h"
#include "fw_unit.h"
#include "snd_unit.h"

typedef void (*HinawaFwUnitHandle)(int fd, union fw_cdev_event *ev);
void hinawa_fw_resp_handle_request(int fd, union fw_cdev_event *ev);
void hinawa_fw_req_handle_response(int fd, union fw_cdev_event *ev);
void hinawa_fw_unit_ioctl(HinawaFwUnit *self, int req, void *args, int *err);

typedef void (HinawaSndUnitHandle)(void *private_data,
				   const void *buf, unsigned int len);
void hinawa_snd_unit_new_with_instance(HinawaSndUnit *self, gchar *path,
				       HinawaSndUnitHandle *handle,
				       void *private_data, GError **exception);
void hinawa_snd_unit_write(HinawaSndUnit *unit,
			   const void *buf, unsigned int length,
			   GError **exception);
#endif
