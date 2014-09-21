#ifndef __ALSA_TOOLS_HINAWA_REACTOR__
#define __ALSA_TOOLS_HINAWA_REACTOR__

#include <stdint.h>

typedef struct hinawa_reactant HinawaReactant;
typedef enum {
	HinawaReactantStateCreated,
	HinawaReactantStateAdded,
	HinawaReactantStateReadable,
	HinawaReactantStateWritable,
	HinawaReactantStateError,
	HinawaReactantStateRemoved,
	HinawaReactantStateDestroyed,
} HinawaReactantState;
typedef void (*HinawaReactantCallback)(void *private_data,
				       HinawaReactantState state, int *err);

HinawaReactant *hinawa_reactant_create(unsigned int priority, int fd,
				       HinawaReactantCallback callback,
				       void *private_data, int *err);
void hinawa_reactant_add(HinawaReactant *reactant, uint32_t events, int *err);
void hinawa_reactant_remove(HinawaReactant *reactant);
void hinawa_reactant_destroy(HinawaReactant *reactant);

typedef struct hinawa_reactor HinawaReactor;
typedef enum {
	HinawaReactorStateCreated,
	HinawaReactorStateStarted,
	HinawaReactorStateStopped,
	HinawaReactorStateDestroyed,
} HinawaReactorState;
typedef void (*HinawaReactorCallback)(void *private_data,
				      HinawaReactorState state, int *err);

HinawaReactor *hinawa_reactor_create(unsigned int priority,
				     HinawaReactorCallback callback,
				     void *private_data, int *err);
void hinawa_reactor_start(HinawaReactor *reactor, unsigned int result_count,
			  unsigned int period, int *err);
void hinawa_reactor_stop(HinawaReactor *reactor);
void hinawa_reactor_destroy(HinawaReactor *reactor);

#endif
