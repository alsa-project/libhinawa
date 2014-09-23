#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <libhinawa/hinawa.h>

static void read_transaction(HinawaFwUnit *unit, int *err)
{
	HinawaFwRequest *req;
	uint32_t buf = 10;
	uint64_t addr = 0xfffff0000904;

	req = hinawa_fw_request_create(err);
	if (!err) {
		printf("Fail to create request for read: %s.\n",
		       strerror(*err));
		return;
	}

	hinawa_fw_request_read(req, unit, addr, &buf, sizeof(buf), err);
	if (*err)
		printf("Fail to send request for read: %s\n",
		       strerror(*err));
	else
		printf("Sucess to send request for read: 0x%08X\n", buf);

	hinawa_fw_request_destroy(req);
}

static void write_transaction(HinawaFwUnit *unit, int *err)
{
	HinawaFwRequest *req;
	uint8_t buf[8];
	uint64_t addr = 0xfffff0000b00;

	req = hinawa_fw_request_create(err);
	if (*err) {
		printf("Fail to create request for write: %s.\n",
		       strerror(*err));
		return;
	}

	buf[0] = 0x01;
	buf[1] = 0xff;
	buf[2] = 0x19;
	buf[3] = 0x00;
	buf[4] = 0xff;
	buf[5] = 0xff;
	buf[6] = 0xff;
	buf[7] = 0xff;

	hinawa_fw_request_write(req, unit, addr, buf, sizeof(buf), err);
	if (*err)
		printf("Fail to send request for write: %s\n",
		       strerror(*err));
	else
		printf("Success to send request for write.\n");

	hinawa_fw_request_destroy(req);
}

static void lock_transaction(HinawaFwUnit *unit, int *err)
{
	HinawaFwRequest *req;
	uint32_t old, new;
	uint32_t buf[2];
	uint64_t addr = 0xfffff0000904;

	req = hinawa_fw_request_create(err);
	if (*err) {
		printf("Fail to create request for lock: %s.\n",
		       strerror(*err));
		return;
	}

	hinawa_fw_request_read(req, unit, addr, &buf, 4, err);
	if (*err)
		printf("Fail to send request for read: %s\n",
		       strerror(*err));

	old = buf[0];
	new = old | 0x00000001;

	buf[0] = old;
	buf[1] = new;
	hinawa_fw_request_run(req, unit, HinawaFwRequestTypeCompareSwap,
			      addr, &buf, 8, err);
	if (*err) {
		printf("Fail to send request for lock: %s\n",
		       strerror(*err));
		return;
	} else if (old != buf[0]) {
		printf("Fail to swap: %08x %08x %08x", old, new, buf[0]);
		return;
	}

	buf[0] = new;
	buf[1] = old;
	hinawa_fw_request_run(req, unit, HinawaFwRequestTypeCompareSwap,
			      addr, &buf, 8, err);
	if (*err)
		printf("Fail to send request for lock: %s\n",
		       strerror(*err));
	else if (new != buf[0])
		printf("Fail to swap: %08x %08x %08x", new, old, buf[0]);
	else
		printf("Success to swap.\n");

	hinawa_fw_request_destroy(req);
}

static void fcp_transaction(HinawaFwUnit *unit, int *err)
{
	HinawaFcpTransaction *trans;
	uint8_t cmd[8];
	uint8_t resp[8];
	unsigned int i, length;
	static unsigned int count = 0;

	trans = hinawa_fcp_transaction_create(err);
	if (*err) {
		printf("Fail to create FCP transaction: %s\n",
		       strerror(*err));
		return;
	}

	cmd[0] = 0x01;
	cmd[1] = 0xff;
	cmd[2] = count % 2 ? 0x19 : 0x18; count++;
	cmd[3] = 0x00;
	cmd[4] = 0xff;
	cmd[5] = 0xff;
	cmd[6] = 0xff;
	cmd[7] = 0xff;

	length = 8;
	hinawa_fcp_transaction_run(trans, unit, cmd, 8, resp, &length, err);
	if (*err) {
		printf("Fail to run FCP transaction: %s\n", strerror(*err));
	} else {
		printf("Success to run FCP transaction.\n");
		for (i = 0; i < sizeof(cmd); i++)
			printf("\tresp[%02d]: %02x\n", i, resp[i]);
	}

	hinawa_fcp_transaction_destroy(trans);
}

static void efw_transaction(HinawaSndUnit *unit, int *err)
{
	HinawaEfwTransaction *trans;
	uint32_t params[26] = {0};
	unsigned int i, length = sizeof(params);

	trans = hinawa_efw_transaction_create(err);
	if (*err) {
		printf("Fail to create EFW transaction: %s\n", strerror(*err));
		return;
	}

	hinawa_efw_transaction_run(trans, unit, 0, 1, NULL, 0,
				   params, &length, err);
	if (*err) {
		printf("Fail to run EFW transaction: %s\n", strerror(*err));
	} else {
		printf("Success to run EFW transaction.\n");
		for (i = 0; i < length; i++)
			printf("\tparam[%02d]: %08x\n", i, params[i]);
	}

	hinawa_efw_transaction_destroy(trans);
}

static void reactor_event(void *private_data, HinawaReactorState state, int *err)
{
	switch (state) {
	case HinawaReactorStateCreated:
		printf("Reactor: created\n");
	break;
	case HinawaReactorStateStarted:
		printf("Reactor: started\n");
	break;
	case HinawaReactorStateStopped:
		printf("Reactor: stopped\n");
	break;
	case HinawaReactorStateDestroyed:
		printf("Reactor: destroyed\n");
	break;
	}
}

/* NOTE: these need to be backported for linux kernel 3.15 or former. */
static const char *snd_unit_types[] = {
	"Not defined",
	"Dice",
	"Fireworks",
	"BeBoB",
};

int main(int argc, char *argv[])
{
	int err = 0;
	char *path;
	bool lock = false;

	unsigned int priority;

	HinawaReactor *reactor;

	HinawaSndUnit *snd_unit;
	char name[32];
	uint8_t guid[8];
	unsigned int type;

	HinawaFwUnit *fw_unit;

	if (argc < 2) {
		printf("ALSA hw device is required, i.e. hw:1\n");
		exit(EXIT_FAILURE);
	}
	path = argv[1];

	if (argc == 3)
		lock = true;

	priority = 0;
	reactor = hinawa_reactor_create(priority, reactor_event, NULL,
					&err);
	if (err)
		goto error;
	hinawa_reactor_start(reactor, 10, 100, &err);
	if (err) {
		hinawa_reactor_destroy(reactor);
		goto error_reactor;
	}

	snd_unit = hinawa_snd_unit_create(path, &err);
	if (err) {
		printf("fail to open.\n");
		goto error_reactor;
	}

	hinawa_snd_unit_get_name(snd_unit, name);
	printf("name: %s\n", name);

	hinawa_snd_unit_get_guid(snd_unit, guid);
	printf("GUID: %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       guid[0], guid[1], guid[2], guid[3],
	       guid[4], guid[5], guid[6], guid[7]);

	type = hinawa_snd_unit_get_type(snd_unit);
	if (type > sizeof(snd_unit_types) / sizeof(snd_unit_types[0])) {
		printf("Unsupported sound device.\n");
		goto error_reactor;
	}
	printf("This sound device is %s\n", snd_unit_types[type]);

	hinawa_snd_unit_listen(snd_unit, priority, &err);
	if (err) {
		printf("fail to listen: %s.\n", strerror(err));
		goto error_snd_listen;
	}

	if (hinawa_snd_unit_is_streaming(snd_unit))
		printf("streaming.\n");
	else
		printf("not streaming.\n");

	if (lock) {
		hinawa_snd_unit_reserve(snd_unit, &err);
		if (err) {
			printf("fail to reserve: %s\n", strerror(err));
			goto error_snd_listen;
		}
	}

	fw_unit = hinawa_fw_unit_from_snd_unit(snd_unit, &err);
	if (err) {
		printf("fail to get fw: %s.\n", strerror(err));
		goto error_snd_listen;
	}
	hinawa_fw_unit_listen(fw_unit, priority, &err);
	if (err) {
		printf("fail to listen fw: %s.\n", strerror(err));
		goto error_fw_listen;
	}

	read_transaction(fw_unit, &err);
	if (err)
		goto error_fw_listen;

	write_transaction(fw_unit, &err);
	if (err)
		goto error_fw_listen;

	/* Only for Fireworks/BeBoB/OXFW */
	if (type == 2 || type == 3 || type == 4) {
		fcp_transaction(fw_unit, &err);
		if (err)
			goto error_fw_listen;
	}

	/* Only for Fireworks */
	if (type == 2) {
		efw_transaction(snd_unit, &err);
		err = 0;
	}

	lock_transaction(fw_unit, &err);
	if (err)
		goto error_fw_listen;

	sleep(10);

	/* Only for Fireworks/BeBoB/OXFW */
	if (type == 2 ||  type == 3 || type == 4) {
		fcp_transaction(fw_unit, &err);
		if (err)
			goto error_fw_listen;
	}

	/* Only for Fireworks */
	if (type == 2) {
		efw_transaction(snd_unit, &err);
		err = 0;
	}

	write_transaction(fw_unit, &err);
error_fw_listen:
	hinawa_fw_unit_unlisten(fw_unit);
	hinawa_fw_unit_destroy(fw_unit);
error_snd_listen:
	hinawa_snd_unit_unlisten(snd_unit);
	hinawa_snd_unit_release(snd_unit);
	hinawa_snd_unit_destroy(snd_unit);
error_reactor:
	hinawa_reactor_stop(reactor);
	hinawa_reactor_destroy(reactor);
error:
	return err;
}
