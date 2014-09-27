#include <stdio.h>
#include <endian.h>

#include <sys/ioctl.h>
#include <sys/epoll.h>

#include <linux/types.h>
#include <linux/limits.h>

#include "internal.h"

HinawaSndUnit *hinawa_snd_unit_create(char *hw_path, int *err)
{
	HinawaSndUnit *unit;
	snd_hwdep_t *hwdep;
	snd_hwdep_info_t *hwdep_info;
	struct snd_firewire_get_info info;
	char *name;

	*err = -snd_hwdep_open(&hwdep, hw_path, SND_HWDEP_OPEN_DUPLEX);
	if (*err)
		return NULL;

	*err = -snd_hwdep_ioctl(hwdep, SNDRV_FIREWIRE_IOCTL_GET_INFO, &info);
	if (*err) {
		snd_hwdep_close(hwdep);
		return NULL;
	}

	hinawa_malloc(unit, sizeof(HinawaSndUnit), err);
	if (*err) {
		snd_hwdep_close(hwdep);
		return NULL;
	}

	*err = -snd_hwdep_info_malloc(&hwdep_info);
	if (*err) {
		hinawa_free(unit);
		snd_hwdep_close(hwdep);
		return NULL;
	}

	*err = -snd_hwdep_info(hwdep, hwdep_info);
	if (*err == 0)
		*err = snd_card_get_name(snd_hwdep_info_get_card(hwdep_info),
					 &name);
	snd_hwdep_info_free(hwdep_info);
	if (*err) {
		hinawa_free(unit);
		snd_hwdep_close(hwdep);
		return NULL;
	}

	strncpy(unit->name, name, sizeof(unit->name));
	free(name);
	*err = 0;

	unit->hwdep = hwdep;
	unit->type = info.type;
	unit->card = info.card;
	memcpy(unit->guid, info.guid, sizeof(info.guid));
	memcpy(unit->device, info.device_name, strlen(info.device_name));

	return unit;
}

void hinawa_snd_unit_get_name(HinawaSndUnit *unit, char name[32])
{
	memcpy(name, unit->name, 32);
}

void hinawa_snd_unit_get_guid(HinawaSndUnit *unit, uint8_t guid[8])
{
	memcpy(guid, unit->guid, 8);
}

bool hinawa_snd_unit_is_streaming(HinawaSndUnit *unit)
{
	return unit->streaming;
}

unsigned int hinawa_snd_unit_get_type(HinawaSndUnit *unit)
{
	return unit->type;
}

void hinawa_snd_unit_reserve(HinawaSndUnit *unit, int *err)
{

	*err = -snd_hwdep_ioctl(unit->hwdep, SNDRV_FIREWIRE_IOCTL_LOCK, NULL);
}

void hinawa_snd_unit_release(HinawaSndUnit *unit)
{
	/* ignore return code */
	snd_hwdep_ioctl(unit->hwdep, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL);
}

static void handle_lock_event(HinawaSndUnit *unit, void *buf,
			      unsigned int length, int *err)
{
	struct snd_firewire_event_lock_status *event =
			(struct snd_firewire_event_lock_status *)buf;
	unit->streaming = event->status;
}

static void handle_dice_notification(HinawaSndUnit *unit, void *buf,
				     unsigned int length, int *err)
{
	struct snd_firewire_event_dice_notification *event =
		(struct snd_firewire_event_dice_notification *)buf;

	/* TODO */
	printf("Dice\n");
}

static void read_event(void *private_data, int *err)
{
	HinawaSndUnit *unit = (HinawaSndUnit *)private_data;
	struct snd_firewire_event_common *common;
	int length;

	length = snd_hwdep_read(unit->hwdep, unit->buf, unit->length);
	if (length < 0) {
		*err = -length;
		return;
	}

	if (length > unit->length) {
		*err = -EINVAL;
		return;
	}

	common = (struct snd_firewire_event_common *)unit->buf;

	if (common->type == SNDRV_FIREWIRE_EVENT_LOCK_STATUS)
		handle_lock_event(unit, unit->buf, length, err);
	else if (common->type == SNDRV_FIREWIRE_EVENT_DICE_NOTIFICATION &&
		 unit->type == SNDRV_FIREWIRE_TYPE_DICE)
		handle_dice_notification(unit, unit->buf, length, err);
	else if (common->type == SNDRV_FIREWIRE_EVENT_EFW_RESPONSE &&
		 unit->type == SNDRV_FIREWIRE_TYPE_FIREWORKS)
		handle_efw_response(unit, unit->buf, length, err);
}

static void handle_snd_events(void *private_data, HinawaReactantState state,
			      int *err)
{
	HinawaSndUnit *unit = (HinawaSndUnit *)private_data;

	if (state == HinawaReactantStateReadable)
		read_event(unit, err);
}

void hinawa_snd_unit_listen(HinawaSndUnit *unit, unsigned int priority,
			    int *err)
{
	struct pollfd pfds;

	/* MEMO: allocate one page because we cannot assume. */
	hinawa_malloc(unit->buf, 4096, err);
	if (*err)
		return;
	unit->length = 4096;

	if (snd_hwdep_poll_descriptors(unit->hwdep, &pfds, 1) != 1) {
		*err = EINVAL;
		return;
	}

	unit->reactant = hinawa_reactant_create(priority, pfds.fd,
						handle_snd_events, unit, err);
	if (*err)
		return;

	hinawa_reactant_add(unit->reactant, EPOLLIN, err);
	if (*err)
		hinawa_reactant_destroy(unit->reactant);

	hinawa_snd_unit_reserve(unit, err);
	if (*err == EBUSY)
		unit->streaming = true;
	else
		unit->streaming = false;
	hinawa_snd_unit_release(unit);
	*err = 0;
}

void hinawa_snd_unit_unlisten(HinawaSndUnit *unit)
{
	unit->listening = false;
	hinawa_reactant_remove(unit->reactant);
	hinawa_reactant_destroy(unit->reactant);
	hinawa_free(unit->buf);
	unit->length = 0;
}

void hinawa_snd_unit_destroy(HinawaSndUnit *unit)
{
	hinawa_snd_unit_release(unit);
	snd_hwdep_close(unit->hwdep);
	hinawa_free(unit);
}
