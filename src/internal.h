#ifndef __ALSA_TOOLS_HINAWA_INTERNAL_H__
#define __ALSA_TOOLS_HINAWA_INTERNAL_H__

#include <errno.h>
#include <string.h>

#include <linux/firewire-cdev.h>
#include <linux/firewire-constants.h>

#include "backport.h"

typedef void (*HinawaFwUnitHandle)(int fd, union fw_cdev_event *ev);
void hinawa_fw_resp_handle_request(int fd, union fw_cdev_event *ev);
void hinawa_fw_req_handle_response(int fd, union fw_cdev_event *ev);

typedef void (*HinawaSndUnitHandle)(void *buf, unsigned int len);

#define FCP_MAXIMUM_FRAME_BYTES	0x200U
#define FCP_REQUEST_ADDR	0xfffff0000b00
#define FCP_RESPOND_ADDR	0xfffff0000d00

#endif
