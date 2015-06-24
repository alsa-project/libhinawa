/* backporting from 3.16 */

/* alsa-lib 1.0.28 is a lack of some Hwdep interfaces */
#ifndef SND_HWDEP_IFACE_FW_DICE
#define SND_HWDEP_IFACE_FW_DICE		(SND_HWDEP_IFACE_SB_RC + 3)
#define SND_HWDEP_IFACE_FW_FIREWORKS	(SND_HWDEP_IFACE_SB_RC + 4)
#define SND_HWDEP_IFACE_FW_BEBOB	(SND_HWDEP_IFACE_SB_RC + 5)
#define SND_HWDEP_IFACE_FW_OXFW		(SND_HWDEP_IFACE_SB_RC + 6)
#endif


#include <linux/types.h>


#ifndef SND_EFW_TRANSACTION_USER_SEQNUM_MAX

#define SND_EFW_TRANSACTION_USER_SEQNUM_MAX	((__u32)((__u16)~0) - 1)

/* each field should be in big endian */
struct snd_efw_transaction {
	__be32 length;
	__be32 version;
	__be32 seqnum;
	__be32 category;
	__be32 command;
	__be32 status;
	__be32 params[0];
};
struct snd_firewire_event_efw_response {
	unsigned int type;
	__be32 response[0];	/* some responses */
};

#define SNDRV_FIREWIRE_EVENT_EFW_RESPONSE       0x4e617475

#define SNDRV_FIREWIRE_TYPE_FIREWORKS	2
#define SNDRV_FIREWIRE_TYPE_BEBOB	3
#define SNDRV_FIREWIRE_TYPE_OXFW	4
#define SNDRV_FIREWIRE_TYPE_DIGI00X	5

#endif

#ifndef SND_HWDEP_IFACE_FW_DIGI00X
#define SND_HWDEP_IFACE_FW_DIGI00X	(SND_HWDEP_IFACE_SB_RC + 7)

#define SNDRV_FIREWIRE_EVENT_DIGI00x_MESSAGE    0x746e736c

struct snd_firewire_event_digi00x_message {
	unsigned int type;
	__u32 message;	/* Digi00x-specific message */
};

#define SNDRV_FIREWIRE_TYPE_DIGI00X	5

#endif

#ifndef SNDRV_FIREWIRE_TYPE_TASCAM

struct snd_firewire_tascam_status {
	__u32 status[64];
};

#define SNDRV_FIREWIRE_TYPE_TASCAM	6
#endif
