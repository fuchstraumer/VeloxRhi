#pragma once
#ifndef VELOX_COROUTINE_ALLOCATOR_HPP
#define VELOX_COROUTINE_ALLOCATOR_HPP
#include <cstdint>
#include <cstddef>
#include <memory>
#include <array>

namespace velox
{

/**@brief Simple O(1) freelist memory arena for Coroutines, but using statically allocated memory
 * so we can reduce how many dynamic allocations we make while still having enough memory to work with
 */
template<std::size_t BlockSizeInBytes, size_t PoolBlockCapacity>
class CoroutinePool
{
    // alignas pointer type for platform
    constexpr static size_t k_ArraySizeInBytes = BlockSizeInBytes * PoolBlockCapacity;
    alignas(16) std::array<std::byte, k_ArraySizeInBytes> memory;
    size_t freeCount;
    std::array<std::byte*, PoolBlockCapacity> freeList;
public:
    CoroutinePool() : freeCount{ PoolBlockCapacity }
    {
        for (size_t i = 0; i < PoolBlockCapacity; ++i)
        {
            freeList[i] = &memory[i * BlockSizeInBytes];
        }
    }

    void* Allocate(size_t size)
    {
        assert(size <= BlockSizeInBytes && "Coroutine frame exceeded block size limit");
        assert(freeCount > 0 && "CoroutinePool exhausted");
        return freeList[--freeCount];
    }

    void Deallocate(void* ptr)
    {
        freeList[freeCount++] = ptr;
    }

};

}

#endif // !VELOX_COROUTINE_ALLOCATOR_HPP
