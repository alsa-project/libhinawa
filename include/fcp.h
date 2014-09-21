#ifndef __ALSA_TOOLS_HINAWA_FCP__
#define __ALSA_TOOLS_HINAWA_FCP__

#include "fw.h"

typedef struct hinawa_fcp_transaction HinawaFcpTransaction;

HinawaFcpTransaction *hinawa_fcp_transaction_create(int *err);

void hinawa_fcp_transaction_run(HinawaFcpTransaction *trans, HinawaFwUnit *unit,
				uint8_t *cmd, unsigned int cmd_len,
				uint8_t *resp, unsigned int *resp_len,
				int *err);
void hinawa_fcp_transaction_destroy(HinawaFcpTransaction *trans);
#endif
