#ifndef __ALSA_TOOLS_HINAWA_SND__
#define __ALSA_TOOLS_HINAWA_SND__

typedef struct hinawa_snd_unit HinawaSndUnit;
typedef void (*HinawaSndUnitCallback)(void *);
HinawaSndUnit *hinawa_snd_unit_create(char *hwdep_path, int *err);
void hinawa_snd_unit_reserve(HinawaSndUnit *unit, int *err);
void hinawa_snd_unit_release(HinawaSndUnit *unit);
void hinawa_snd_unit_listen(HinawaSndUnit *unit, unsigned int priority,
			    int *err);
void hinawa_snd_unit_unlisten(HinawaSndUnit *unit);
void hinawa_snd_unit_destroy(HinawaSndUnit *unit);
typedef struct hinawa_fw_unit HinawaFwUnit;

#endif
