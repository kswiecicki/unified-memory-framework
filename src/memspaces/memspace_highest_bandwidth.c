/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <assert.h>
#include <ctype.h>
#include <hwloc.h>
#include <stdlib.h>

#include "base_alloc_global.h"
#include "memory_target_numa.h"
#include "memspace_internal.h"
#include "memspace_numa.h"
#include "topology.h"
#include "utils_common.h"
#include "utils_concurrency.h"

#define MAX_NODES 512

static bool is_number_str(const char *token) {
    if (*token == '\0') {
        return false;
    }

    while (*token != '\0') {
        if (*token < '0' || *token > '9') {
            return false;
        }

        token++;
    }

    return true;
}

static umf_result_t getenv_to_nodes_array(char *envStr, size_t *arr,
                                          size_t arrSize, size_t *outSize) {
    if (envStr == NULL || arr == NULL || arrSize == 0 || outSize == NULL) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    char *value = getenv(envStr);
    if (!value) {
        return UMF_RESULT_ERROR_NOT_SUPPORTED;
    }

    size_t nNodes = 0;
    char *token = strtok(value, " ,");
    while (token != NULL) {
        if (!is_number_str(token)) {
            fprintf(stderr, "Error: Parsing env variable %s\n", envStr);
            return UMF_RESULT_ERROR_INVALID_ARGUMENT;
        }

        if (arrSize > nNodes) {
            size_t node = (size_t)strtoul(token, NULL, 10);
            arr[nNodes] = node;
        }

        nNodes++;
        token = strtok(NULL, " ,");
    }

    *outSize = nNodes;

    return UMF_RESULT_SUCCESS;
}

static umf_result_t
umfMemspaceHighestBandwidthCreate(umf_memspace_handle_t *hMemspace) {
    if (!hMemspace) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Check env var 'UMF_MEMSPACE_HIGHEST_BANDWIDTH' for the nodes assigned
    // by the user.
    size_t nodeIds[MAX_NODES];
    size_t nNodes = 0;
    umf_result_t ret = getenv_to_nodes_array("UMF_MEMSPACE_HIGHEST_BANDWIDTH",
                                             nodeIds, MAX_NODES, &nNodes);
    if (ret == UMF_RESULT_SUCCESS) {
        assert(nNodes > 0 && nNodes <= MAX_NODES);
        return umfMemspaceCreateFromNumaArray(nodeIds, nNodes, hMemspace);
    }

    umf_memspace_handle_t hostAllMemspace = umfMemspaceHostAllGet();
    if (!hostAllMemspace) {
        return UMF_RESULT_ERROR_UNKNOWN;
    }

    umf_memspace_handle_t highBandwidthMemspace = NULL;
    ret = umfMemspaceClone(hostAllMemspace, &highBandwidthMemspace);
    if (ret != UMF_RESULT_SUCCESS) {
        return ret;
    }

    ret = umfMemspaceSortDesc(highBandwidthMemspace,
                              (umfGetPropertyFn)&umfMemoryTargetGetBandwidth);
    if (ret != UMF_RESULT_SUCCESS) {
        // TODO: Should we have an alternative way to query 'bandwidth' value
        //       of each NUMA node?
        // HWLOC could possibly return an 'EINVAL' error, which in this context
        // means that the HMAT is unavailable and we can't obtain the
        // 'bandwidth' value of any NUMA node.
        goto err_destroy_memspace;
    }

    *hMemspace = highBandwidthMemspace;
    return UMF_RESULT_SUCCESS;

err_destroy_memspace:
    umfMemspaceDestroy(highBandwidthMemspace);
    return ret;
}

static umf_memspace_handle_t UMF_MEMSPACE_HIGHEST_BANDWIDTH = NULL;
static UTIL_ONCE_FLAG UMF_MEMSPACE_HBW_INITIALIZED = UTIL_ONCE_FLAG_INIT;

void umfMemspaceHighestBandwidthDestroy(void) {
    if (UMF_MEMSPACE_HIGHEST_BANDWIDTH) {
        umfMemspaceDestroy(UMF_MEMSPACE_HIGHEST_BANDWIDTH);
        UMF_MEMSPACE_HIGHEST_BANDWIDTH = NULL;
    }
}

static void umfMemspaceHighestBandwidthInit(void) {
    umf_result_t ret =
        umfMemspaceHighestBandwidthCreate(&UMF_MEMSPACE_HIGHEST_BANDWIDTH);
    if (ret != UMF_RESULT_SUCCESS) {
        assert(ret == UMF_RESULT_ERROR_NOT_SUPPORTED);
    }

    // TODO: Setup appropriate cleanup when HBW memspace becomes available
    // on Windows. HBW memspace depends on OS provider, which currently
    // doesn't support Windows.
}

umf_memspace_handle_t umfMemspaceHighestBandwidthGet(void) {
    util_init_once(&UMF_MEMSPACE_HBW_INITIALIZED,
                   umfMemspaceHighestBandwidthInit);
    return UMF_MEMSPACE_HIGHEST_BANDWIDTH;
}
