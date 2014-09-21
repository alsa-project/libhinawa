#ifndef __ALSA_TOOLS_HINAWA_EFW__
#define __ALSA_TOOLS_HINAWA_EFW__

#include "snd.h"

typedef struct hinawa_efw_transaction HinawaEfwTransaction;
HinawaEfwTransaction *hinawa_efw_transaction_create(int *err);
void hinawa_efw_transaction_run(HinawaEfwTransaction *trans, HinawaSndUnit *unit,
				unsigned int category, unsigned int command,
				uint32_t *args, unsigned int args_count,
				uint32_t *params, unsigned int *params_count,
				int *err);
void hinawa_efw_transaction_destroy(HinawaEfwTransaction *trans);

#endif
