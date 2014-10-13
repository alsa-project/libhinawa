#ifndef __ALSA_TOOLS_HINAWA_SND__
#define __ALSA_TOOLS_HINAWA_SND__

#include <stdbool.h>

typedef struct hinawa_snd_unit HinawaSndUnit;
typedef void (*HinawaSndUnitCallback)(void *buf, unsigned int length,
				      void *private_data);

HinawaSndUnit *hinawa_snd_unit_create(char *hwdep_path, int *err);
void hinawa_snd_unit_reserve(HinawaSndUnit *unit, int *err);
void hinawa_snd_unit_release(HinawaSndUnit *unit);
void hinawa_snd_unit_listen(HinawaSndUnit *unit, unsigned int priority,
			    int *err);
void hinawa_snd_unit_unlisten(HinawaSndUnit *unit);
void hinawa_snd_unit_destroy(HinawaSndUnit *unit);

void hinawa_snd_unit_get_name(HinawaSndUnit *unit, char name[32]);
void hinawa_snd_unit_get_guid(HinawaSndUnit *unit, uint8_t guid[8]);
bool hinawa_snd_unit_is_streaming(HinawaSndUnit *unit);
unsigned int hinawa_snd_unit_get_type(HinawaSndUnit *unit);
void hinawa_snd_unit_register_handler(HinawaSndUnit *unit,
				      HinawaSndUnitCallback callback,
				      void *private_data);

#endif
