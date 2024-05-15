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
#include <sched.h>
#include <thread>
#include <umf/memspace.h>

using umf_test::test;

// In HWLOC v2.3.0, the 'hwloc_location_type_e' enum is defined inside an
// 'hwloc_location' struct. In newer versions, this enum is defined globally.
// To prevent compile errors in C++ tests related this scope change
// 'hwloc_location_type_e' has been aliased.
using hwloc_location_type_alias = decltype(hwloc_location::type);

static bool canQueryBandwidth(size_t nodeId) {
    hwloc_topology_t topology = nullptr;
    int ret = hwloc_topology_init(&topology);
    UT_ASSERTeq(ret, 0);
    ret = hwloc_topology_load(topology);
    UT_ASSERTeq(ret, 0);

    hwloc_obj_t numaNode =
        hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, nodeId);
    UT_ASSERTne(numaNode, nullptr);

    // Setup initiator structure.
    struct hwloc_location initiator;
    initiator.location.cpuset = numaNode->cpuset;
    initiator.type = hwloc_location_type_alias::HWLOC_LOCATION_TYPE_CPUSET;

    hwloc_uint64_t value = 0;
    ret = hwloc_memattr_get_value(topology, HWLOC_MEMATTR_ID_BANDWIDTH,
                                  numaNode, &initiator, 0, &value);

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
    umf_result_t ret =
        umfMemoryProviderCreateFromMemspace(hMemspace, nullptr, &hProvider);
    UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
    UT_ASSERTne(hProvider, nullptr);

    umfMemoryProviderDestroy(hProvider);
}

TEST_F(memspaceHighestBandwidthProviderTest, allocFree) {
    void *ptr = nullptr;
    size_t size = SIZE_4K;
    size_t alignment = 0;

    umf_result_t ret = umfMemoryProviderAlloc(hProvider, size, alignment, &ptr);
    UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
    UT_ASSERTne(ptr, nullptr);

    // Access the allocation, so that all the pages associated with it are
    // allocated on some NUMA node.
    memset(ptr, 0xFF, size);

    ret = umfMemoryProviderFree(hProvider, ptr, size);
    UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
}

static std::vector<int> get_available_cpus() {
    std::vector<int> available_cpus;
    cpu_set_t *mask = CPU_ALLOC(CPU_SETSIZE);
    CPU_ZERO(mask);

    int ret = sched_getaffinity(0, sizeof(cpu_set_t), mask);
    UT_ASSERTeq(ret, 0);

    for (size_t i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, mask)) {
            available_cpus.emplace_back(i);
        }
    }
    CPU_FREE(mask);

    return available_cpus;
}

TEST_F(memspaceHighestBandwidthProviderTest, allocFreeMt) {
    auto pinAllocValidate = [&](umf_memory_provider_handle_t hProvider,
                                int cpu) {
        // Pin current thread to the provided CPU mask.
        cpu_set_t *mask = CPU_ALLOC(CPU_SETSIZE);
        CPU_ZERO(mask);
        CPU_SET(cpu, mask);
        UT_ASSERTeq(sched_setaffinity(0, sizeof(*mask), mask), 0);
        CPU_FREE(mask);

        // Confirm that the thread is pinned to the provided CPU.
        int curCpu = sched_getcpu();
        UT_ASSERTeq(curCpu, cpu);

        const size_t size = SIZE_4K;
        const size_t alignment = 0;
        void *ptr = nullptr;

        umf_result_t ret =
            umfMemoryProviderAlloc(hProvider, size, alignment, &ptr);
        UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
        UT_ASSERTne(ptr, nullptr);

        // Access the allocation, so that all the pages associated with it are
        // allocated on some NUMA node.
        memset(ptr, 0xFF, size);

        int mode = -1;
        std::vector<size_t> boundNodeIds;
        size_t allocNodeId = SIZE_MAX;
        getAllocationPolicy(ptr, maxNodeId, mode, boundNodeIds, allocNodeId);

        // Get the CPUs associated with the specified NUMA node.
        struct bitmask *nodeCpus = numa_allocate_cpumask();
        UT_ASSERTeq(numa_node_to_cpus((int)allocNodeId, nodeCpus), 0);

        // Confirm that the allocation from this thread was made to a local
        // NUMA node.
        fprintf(stderr, "Node #%d\n", allocNodeId);
        for (int i = 0; i < numa_max_node(); i++) {
            if (numa_bitmask_isbitset(nodeCpus, i)) {
                fprintf(stderr, "Cpuset bit #%d present\n", i);
            }
        }

        UT_ASSERT(numa_bitmask_isbitset(nodeCpus, cpu));
        numa_free_cpumask(nodeCpus);

        ret = umfMemoryProviderFree(hProvider, ptr, size);
        UT_ASSERTeq(ret, UMF_RESULT_SUCCESS);
    };

    const auto cpus = get_available_cpus();
    std::vector<std::thread> threads;
    for (auto cpu : cpus) {
        threads.emplace_back(pinAllocValidate, hProvider, cpu);
    }

    for (auto &thread : threads) {
        thread.join();
    }
}
