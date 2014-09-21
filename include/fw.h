#ifndef __ALSA_TOOLS_HINAWA_FW__
#define __ALSA_TOOLS_HINAWA_FW__

#include <stdint.h>
#include <pthread.h>
#include "snd.h"

typedef struct hinawa_fw_unit HinawaFwUnit;
HinawaFwUnit *hinawa_fw_unit_create(char *fw_path, int *err);
HinawaFwUnit *hinawa_fw_unit_from_snd_unit(HinawaSndUnit *snd_unit, int *err);
void hinawa_fw_unit_listen(HinawaFwUnit *unit, int priority, int *err);
void hinawa_fw_unit_unlisten(HinawaFwUnit *unit);
void hinawa_fw_unit_destroy(HinawaFwUnit *unit);

typedef struct hinawa_fw_respond HinawaFwRespond;
typedef void (*HinawaFwRespondCallback)(void *buf, unsigned int *length,
					void *private_data, int *err);
HinawaFwRespond *hinawa_fw_respond_create(HinawaFwRespondCallback callback,
					  void *private_data, int *err);
void hinawa_fw_respond_register(HinawaFwRespond *resp, HinawaFwUnit *unit,
				uint64_t addr, unsigned int length, int *err);
void hinawa_fw_respond_unregister(HinawaFwRespond *resp);
void hinawa_fw_respond_destroy(HinawaFwRespond *resp);

typedef struct hinawa_fw_request HinawaFwRequest;
typedef enum {
	HinawaFwRequestTypeRead = 0,
	HinawaFwRequestTypeWrite,
	HinawaFwRequestTypeCompareSwap,
} HinawaFwRequestType;
HinawaFwRequest *hinawa_fw_request_create(int *err);
void hinawa_fw_request_run(HinawaFwRequest *req, HinawaFwUnit *unit,
			   HinawaFwRequestType type, uint64_t addr,
			   void *buf, unsigned int length, int *err);
static inline void hinawa_fw_request_write(HinawaFwRequest *req,
					   HinawaFwUnit *unit, uint64_t addr,
					   void *buf, int length, int *err)
{
	hinawa_fw_request_run(req, unit, HinawaFwRequestTypeWrite, addr, buf,
			      length, err);
}
static inline void hinawa_fw_request_read(HinawaFwRequest *req,
					  HinawaFwUnit *unit, uint64_t addr,
					  void *buf, int length, int *err)
{
	hinawa_fw_request_run(req, unit, HinawaFwRequestTypeRead, addr, buf,
			      length, err);
}
void hinawa_fw_request_destroy(HinawaFwRequest *trans);

#endif
