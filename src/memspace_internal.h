/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMSPACE_INTERNAL_H
#define UMF_MEMSPACE_INTERNAL_H 1

#include "memory_target.h"

#ifdef __cplusplus
extern "C" {
#endif

struct umf_memspace_t {
    size_t size;

    // TODO: consider flexible array member (if we don't want to expose the
    // struct definition in the headers)
    umf_memory_target_handle_t *nodes;
};

#ifdef __cplusplus
}
#endif

#endif /* UMF_MEMSPACE_INTERNAL_H */
