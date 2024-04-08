// Copyright (C) 2024 Intel Corporation
// Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "memory_target_numa.h"
#include "memspace_helpers.hpp"
#include "memspace_internal.h"
#include "test_helpers.h"

#include <hwloc.h>
#include <numa.h>
#include <numaif.h>
#include <umf/memspace.h>

using umf_test::test;

static bool canQueryBandwidth(size_t nodeId) {
    hwloc_topology_t topology = nullptr;
    int ret = hwloc_topology_init(&topology);
    UT_ASSERTeq(ret, 0);
    ret = hwloc_topology_load(topology);
    UT_ASSERTeq(ret, 0);

    hwloc_obj_t numaNode =
        hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, nodeId);
    UT_ASSERTne(numaNode, nullptr);

    // Get local cpuset.
    // hwloc_const_cpuset_t cCpuset = hwloc_topology_get_allowed_cpuset(topology);
    // hwloc_cpuset_t cpuset = hwloc_bitmap_dup(cCpuset);

    // Setup initiator structure.
    struct hwloc_location initiator;
    initiator.location.cpuset = numaNode->cpuset;
    initiator.type = HWLOC_LOCATION_TYPE_CPUSET;

    hwloc_uint64_t value = 0;
    ret = hwloc_memattr_get_value(topology, HWLOC_MEMATTR_ID_BANDWIDTH,
                                  numaNode, &initiator, 0, &value);

    // hwloc_bitmap_free(cpuset);
    hwloc_topology_destroy(topology);
    return (ret == 0);
}

struct memspaceHighestBandwidthTest : ::numaNodesTest {
    void SetUp() override {
        ::numaNodesTest::SetUp();

        if (!canQueryBandwidth(nodeIds.front())) {
            GTEST_SKIP();
        }

        hMemspace = umfMemspaceHighestBandwidthGet();
        ASSERT_NE(hMemspace, nullptr);
    }

    umf_memspace_handle_t hMemspace = nullptr;
};

struct memspaceHighestBandwidthProviderTest : ::memspaceHighestBandwidthTest {
    void SetUp() override {
        ::memspaceHighestBandwidthTest::SetUp();

        if (!canQueryBandwidth(nodeIds.front())) {
            GTEST_SKIP();
        }

        umf_result_t ret =
            umfMemoryProviderCreateFromMemspace(hMemspace, nullptr, &hProvider);
        ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
        ASSERT_NE(hProvider, nullptr);
    }

    void TearDown() override {
        ::memspaceHighestBandwidthTest::TearDown();

        if (hProvider) {
            umfMemoryProviderDestroy(hProvider);
        }
    }

    umf_memory_provider_handle_t hProvider = nullptr;
};

TEST_F(memspaceHighestBandwidthTest, providerFromMemspace) {
    umf_memory_provider_handle_t hProvider = nullptr;
    enum umf_result_t ret =
        umfMemoryProviderCreateFromMemspace(hMemspace, nullptr, &hProvider);
    UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
    UT_ASSERTne(hProvider, nullptr);

    umfMemoryProviderDestroy(hProvider);
}

TEST_F(memspaceHighestBandwidthProviderTest, allocFree) {
    void *ptr = nullptr;
    size_t size = SIZE_4K;
    size_t alignment = 0;

    enum umf_result_t ret =
        umfMemoryProviderAlloc(hProvider, size, alignment, &ptr);
    UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
    UT_ASSERTne(ptr, nullptr);

    memset(ptr, 0xFF, size);

    ret = umfMemoryProviderFree(hProvider, ptr, size);
    UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
}
