/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <assert.h>
#include <hwloc.h>
#include <stdlib.h>

#include "base_alloc_global.h"
#include "memory_target_numa.h"
#include "memspace_internal.h"
#include "memspace_numa.h"
#include "utils_concurrency.h"

static umf_result_t umfMemspaceHBWCreate(umf_memspace_handle_t *hMemspace) {
    if (!hMemspace) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    umf_result_t umf_ret = UMF_RESULT_SUCCESS;

    hwloc_topology_t topology;
    if (hwloc_topology_init(&topology)) {
        // TODO: What would be an approrpiate err?
        return UMF_RESULT_ERROR_UNKNOWN;
    }

    if (hwloc_topology_load(topology)) {
        umf_ret = UMF_RESULT_ERROR_UNKNOWN;
        goto err_topology_destroy;
    }

    // Shouldn't return -1, as 'HWLOC_OBJ_NUMANODE' doesn't appear to be an
    // object that can be present on multiple levels.
    // Source: https://www.open-mpi.org/projects/hwloc/doc/hwloc-v2.10.0-letter.pdf
    int nNodes = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NUMANODE);
    assert(nNodes != -1);

    size_t *nodeIds = umf_ba_global_alloc(nNodes * sizeof(size_t));
    if (!nodeIds) {
        umf_ret = UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        goto err_topology_destroy;
    }

    // Collect all available NUMA node ids on the platform
    int nodeIdx = 0;
    hwloc_obj_t numaNodeObj = NULL;
    while ((numaNodeObj = hwloc_get_next_obj_by_type(
                topology, HWLOC_OBJ_NUMANODE, numaNodeObj)) != NULL) {
        // Shouldn't be possible to iterate over more 'HWLOC_OBJ_NUMANODE' objects
        // than the number returned by hwloc_get_nbobjs_by_type.
        assert(nodeIdx < nNodes);

        fprintf(stderr, "Numa node: %u \n", numaNodeObj->os_index);
        struct hwloc_location initiator = {0};
        hwloc_uint64_t value = 0;
        int ret =
            hwloc_memattr_get_best_initiator(topology, HWLOC_MEMATTR_ID_BANDWIDTH,
                                             numaNodeObj, 0, &initiator, &value);
        assert(errno != ENOENT);
        assert(errno != EINVAL);
        assert(ret == 0);
        // hwloc_uint64_t value = 0;
        // int ret = hwloc_memattr_get_value(topology, HWLOC_MEMATTR_ID_BANDWIDTH,
        //                                   numaNodeObj, NULL, 0, &value);
        // assert(errno != ENOENT);
        // assert(errno != EINVAL);
        // assert(ret == 0);
        // fprintf(stderr, "Numa node value: %lu \n", value);
        nodeIds[nodeIdx++] = numaNodeObj->os_index;
    }

    umf_ret =
        umfMemspaceCreateFromNumaArray(nodeIds, (size_t)nNodes, hMemspace);

    umf_ba_global_free(nodeIds);

err_topology_destroy:
    hwloc_topology_destroy(topology);
    return umf_ret;
}

static umf_memspace_handle_t UMF_MEMSPACE_HBW = NULL;
static UTIL_ONCE_FLAG UMF_MEMSPACE_HBW_INITIALIZED = UTIL_ONCE_FLAG_INIT;

void umfMemspaceHBWDestroy(void) {
    if (UMF_MEMSPACE_HBW) {
        umfMemspaceDestroy(UMF_MEMSPACE_HBW);
        UMF_MEMSPACE_HBW = NULL;
    }
}

static void umfMemspaceHBWInit(void) {
    umf_result_t ret = umfMemspaceHBWCreate(&UMF_MEMSPACE_HBW);
    assert(ret == UMF_RESULT_SUCCESS);
    (void)ret;

    // TODO: Setup appropriate cleanup when HBW memspace becomes available
    // on Windows. HBW memspace depends on OS provider, which currently
    // doesn't support Windows.
}

umf_memspace_handle_t umfMemspaceHBWGet(void) {
    util_init_once(&UMF_MEMSPACE_HBW_INITIALIZED, umfMemspaceHBWInit);
    return UMF_MEMSPACE_HBW;
}
