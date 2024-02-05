/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include "provider_tracking.h"

#include <stddef.h>

umf_memory_tracker_handle_t TRACKER = NULL;

void __attribute__((constructor)) umfCreate(void) {
    TRACKER = umfMemoryTrackerCreate();
}
void __attribute__((destructor)) umfDestroy(void) {
    umfMemoryTrackerDestroy(TRACKER);
}

void libumfInit(void) {
    // do nothing, additional initialization not needed
}