#include "rail_seq.h"

// Stub vtable providing NULL function pointers for host-side builds that do
// not include the sequencer runtime.
//
// The symbol is marked weak to avoid duplicate definition if the real
// sequencer implementation is linked in another build configuration.

__WEAK const volatile RAIL_SEQ_Vtable_t railSeqVtable = { 0 };
