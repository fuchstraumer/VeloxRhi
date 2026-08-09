#pragma once
#ifndef VELOX_COROUTINE_ALLOCATOR_HPP
#define VELOX_COROUTINE_ALLOCATOR_HPP
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <array>
#include <atomic>
#ifdef VELOX_ENABLE_DIAGNOSTICS
#include <print>
#endif

namespace velox
{

/**@brief Simple O(1) freelist memory arena for Coroutines, but using statically allocated memory
 * so we can reduce how many dynamic allocations we make while still having enough memory to work with
 */
template<std::size_t BlockSizeInBytes, size_t PoolBlockCapacity>
class CoroutinePool
{

#ifdef VELOX_ENABLE_DIAGNOSTICS
    inline static std::size_t NumFramesAllocated{0u};
    inline static std::size_t NumFramesDeallocated{0u};
    inline static std::size_t BytesAllocatedFreeList{0u};
    inline static std::size_t BytesDeallocatedFreeList{0u};
    inline static std::size_t BytesAllocatedMalloc{0u};
#endif

    // alignas pointer type for platform
    constexpr static size_t k_ArraySizeInBytes = BlockSizeInBytes * PoolBlockCapacity;
    alignas(16) std::array<std::byte, k_ArraySizeInBytes> memory;
    size_t freeCount;
    std::array<std::byte*, PoolBlockCapacity> freeList;

    constexpr void* allocFromFreeList() noexcept
    {
        return freeList[--freeCount];
    }

    constexpr bool addressInArena(void* addr) const noexcept
    {
        std::byte* ptr = static_cast<std::byte*>(addr);
        return (ptr >= memory.data()) && ptr < (memory.data() + memory.size());
    }

    constexpr void pushToFreeList(void* addr) noexcept
    {
        std::byte* ptr = static_cast<std::byte*>(addr);
        freeList[freeCount++] = static_cast<std::byte*>(ptr);
    }

public:

    CoroutinePool() : freeCount{ PoolBlockCapacity }
    {
        for (size_t i = 0; i < PoolBlockCapacity; ++i)
        {
            freeList[i] = &memory[i * BlockSizeInBytes];
        }
    }

    ~CoroutinePool()
    {
#ifdef VELOX_ENABLE_DIAGNOSTICS
        std::println("[velox][async] Coroutines allocated: {}", NumFramesAllocated);
        std::println("[velox][async] Coroutines deallocated: {}", NumFramesDeallocated);
        std::println("[velox][async] Coroutine frame bytes allocated: {}", BytesAllocatedFreeList);
        std::println("[velox][async] Coroutine frame bytes deallocated: {}", BytesDeallocatedFreeList);
        const size_t blockOverhead = BytesDeallocatedFreeList - BytesAllocatedFreeList;
        std::println("[velox][async] Byte overhead due to block size: {}", blockOverhead);
        std::println("[velox][async] Bytes allocated via malloc(): {}", BytesAllocatedMalloc);
#endif
    }

    void* Allocate(size_t size)
    {
        if (size <= BlockSizeInBytes) [[likely]]
        {
#ifdef VELOX_ENABLE_DIAGNOSTICS
            ++NumFramesAllocated;
            BytesAllocatedFreeList += size;
#endif
            return allocFromFreeList();
        }
        else
        {
#ifdef VELOX_ENABLE_DIAGNOSTICS
            ++NumFramesAllocated;
            BytesAllocatedMalloc += size;
            std::println("[velox][async] CoroutinePool falling back to malloc for frame of {} bytes", size);
#endif
            return malloc(size);
        }
    }

    void Deallocate(void* ptr, size_t size)
    {
        if (size <= BlockSizeInBytes) [[likely]]
        {
#ifdef VELOX_ENABLE_DIAGNOSTICS
            ++NumFramesDeallocated;
            BytesDeallocatedFreeList += BlockSizeInBytes;
#endif
            pushToFreeList(ptr);
        }
        else
        {
#ifdef VELOX_ENABLE_DIAGNOSTICS
            ++NumFramesDeallocated;
#endif
            free(ptr);
        }
    }

};

#ifdef NDEBUG
inline CoroutinePool<512, 2048> g_CoroutineAllocator;
#else
inline CoroutinePool<1024, 2048> g_CoroutineAllocator;
#endif

}

#endif // !VELOX_COROUTINE_ALLOCATOR_HPP
