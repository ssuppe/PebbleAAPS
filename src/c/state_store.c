/**
 * state_store.c — Pebble SDK implementation of the storage layer.
 *
 * This file uses the Pebble persist_* API and is compiled ONLY for the
 * Pebble target.  It is NOT compiled in host unit tests; those tests
 * provide their own mock implementation via tests/test_logic.c.
 */
#include "state_store.h"
#include <pebble.h>

bool state_store_load(AAPSState *state) {
  if (!state) return false;
  if (!persist_exists(PERSIST_KEY_AAPS_STATE)) return false;
  int bytes = persist_read_data(PERSIST_KEY_AAPS_STATE, state, sizeof(AAPSState));
  return (bytes == (int)sizeof(AAPSState));
}

void state_store_save(const AAPSState *state) {
  if (!state) return;
  persist_write_data(PERSIST_KEY_AAPS_STATE, state, sizeof(AAPSState));
}

void state_store_clear(void) {
  persist_delete(PERSIST_KEY_AAPS_STATE);
}
