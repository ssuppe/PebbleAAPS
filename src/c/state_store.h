#ifndef STATE_STORE_H
#define STATE_STORE_H

#include "logic.h"
#include <stdbool.h>

#define PERSIST_KEY_AAPS_STATE    100
#define PERSIST_KEY_STATE_VERSION 101
#define CURRENT_STATE_VERSION       1

/**
 * Load persisted AAPSState from flash storage.
 * Returns true if data was found and read successfully, false otherwise.
 */
bool state_store_load(AAPSState *state);

/**
 * Persist the given AAPSState to flash storage.
 */
void state_store_save(const AAPSState *state);

/**
 * Erase the persisted AAPSState from flash storage.
 */
void state_store_clear(void);

#endif // STATE_STORE_H
