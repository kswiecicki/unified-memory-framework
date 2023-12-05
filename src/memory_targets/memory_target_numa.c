/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <stdlib.h>

#include "../memory_pool_internal.h"
#include "memory_target_numa.h"
#include <umf/pools/pool_disjoint.h>
#include <umf/providers/provider_os_memory.h>

struct numa_memory_target_t {
    size_t id;
};

static enum umf_result_t numa_initialize(void *params, void **mem_target) {
    struct umf_numa_memory_target_config_t *config =
        (struct umf_numa_memory_target_config_t *)params;

    struct numa_memory_target_t *numa_target =
        malloc(sizeof(struct numa_memory_target_t));
    if (!numa_target) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    numa_target->id = config->id;
    *mem_target = numa_target;
    return UMF_RESULT_SUCCESS;
}

static void numa_finalize(void *mem_target) { free(mem_target); }

static const umf_os_memory_provider_params_t
    UMF_OS_MEMORY_PROVIDER_PARAMS_DEFAULT = {
        // Visibility & protection
        .protection = UMF_PROTECTION_READ | UMF_PROTECTION_WRITE,
        .visibility = UMF_VISIBILITY_PRIVATE,

        // NUMA config
        .nodemask = NULL,
        .maxnode = 0, // TODO: numa_max_node/GetNumaHighestNodeNumber
        .numa_mode = UMF_NUMA_MODE_DEFAULT,
        .numa_flags = UMF_NUMA_FLAGS_STRICT, // TODO: determine default behavior

        // Logging
        .traces = 0, // TODO: parse env variable for log level?
};

static unsigned long
numa_targets_get_nodemask(struct numa_memory_target_t **targets,
                          size_t num_targets) {
    unsigned long nodemask = 0;
    for (size_t i = 0; i < num_targets; i++) {
        nodemask |= (1UL << targets[i]->id);
    }

    return nodemask;
}

static unsigned long numa_nodemask_get_maxnode(unsigned long nodemask) {
    for (int bitIdx = sizeof(nodemask) * 8 - 1; bitIdx >= 0; bitIdx--) {
        if ((nodemask & (1UL << bitIdx)) != 0) {
            return (unsigned long)bitIdx;
        }
    }

    return 0;
}

static enum umf_result_t numa_memory_provider_create_from_memspace(
    umf_memspace_handle_t memspace, void **mem_targets, size_t num_targets,
    umf_memspace_policy_handle_t policy,
    umf_memory_provider_handle_t *provider) {
    (void)memspace;
    // TODO: apply policy
    (void)policy;

    struct numa_memory_target_t **numa_targets =
        (struct numa_memory_target_t **)mem_targets;

    // Create node mask from IDs
    unsigned long nodemask =
        numa_targets_get_nodemask(numa_targets, num_targets);
    unsigned long maxnode = numa_nodemask_get_maxnode(nodemask);

    umf_os_memory_provider_params_t params =
        UMF_OS_MEMORY_PROVIDER_PARAMS_DEFAULT;
    params.nodemask = &nodemask;
    params.maxnode = maxnode;

    umf_memory_provider_handle_t numa_provider = NULL;
    enum umf_result_t ret = umfMemoryProviderCreate(&UMF_OS_MEMORY_PROVIDER_OPS,
                                                    &params, &numa_provider);
    if (ret) {
        return ret;
    }

    *provider = numa_provider;

    return UMF_RESULT_SUCCESS;
}

static enum umf_result_t numa_pool_create_from_memspace(
    umf_memspace_handle_t memspace, void **mem_targets, size_t num_targets,
    umf_memspace_policy_handle_t policy, umf_memory_pool_handle_t *pool) {
    (void)memspace;
    // TODO: apply policy
    (void)policy;

    struct numa_memory_target_t **numa_targets =
        (struct numa_memory_target_t **)mem_targets;

    // Create node mask from IDs
    unsigned long nodemask =
        numa_targets_get_nodemask(numa_targets, num_targets);
    unsigned long maxnode = numa_nodemask_get_maxnode(nodemask);

    umf_os_memory_provider_params_t provider_params =
        UMF_OS_MEMORY_PROVIDER_PARAMS_DEFAULT;
    provider_params.nodemask = &nodemask;
    provider_params.maxnode = maxnode;

    // TODO: determine which pool implementation to use
    umf_disjoint_pool_params_t pool_params = umfDisjointPoolParamsDefault();
    umf_memory_pool_handle_t disjoint_pool = NULL;
    enum umf_result_t ret = umfPoolCreateEx(
        &UMF_DISJOINT_POOL_OPS, &pool_params, &UMF_OS_MEMORY_PROVIDER_OPS,
        &provider_params, &disjoint_pool);
    if (ret) {
        return ret;
    }

    *pool = disjoint_pool;

    return UMF_RESULT_SUCCESS;
}

struct umf_memory_target_ops_t UMF_MEMORY_TARGET_NUMA_OPS = {
    .version = UMF_VERSION_CURRENT,
    .initialize = numa_initialize,
    .finalize = numa_finalize,
    .pool_create_from_memspace = numa_pool_create_from_memspace,
    .memory_provider_create_from_memspace =
        numa_memory_provider_create_from_memspace};
