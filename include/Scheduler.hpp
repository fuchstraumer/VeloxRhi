#pragma once
#ifndef VELOX_CONTEXT_IMPL_HPP
#define VELOX_CONTEXT_IMPL_CPP
#include "VeloxErrors.hpp"
#include "utility/SlotMap.hpp"
#include <coroutine>
#include <expected>

/**
 * This file is mostly to serve as a "bus" for tying together our coroutines in AsyncTasks,
 * the SlotMap instances in Context, and all without creating a really gnarly and hard to
 * follow sourcetree of dependencies and control flow. It's still not great, but I'm at a loss
 * currently for how to do better
 */
namespace velox
{

// These coroutines will always be heap allocated, meaning 2-3 of the LSBs will be unused. We can use these
// for tagging / our boolean ready checks, and size "dynamically" for the platforms pointer width.
struct TaggedCoroutineSlot
{
private: // in hindsight this is really a class lol
    uintptr_t data{ 0u };
    static constexpr uintptr_t k_readyBit = 1ULL;
    static constexpr uintptr_t k_ptrMask = ~k_readyBit;
public:
    TaggedCoroutineSlot(std::coroutine_handle<> handle) noexcept;
    ~TaggedCoroutineSlot() noexcept = default;
    TaggedCoroutineSlot(const TaggedCoroutineSlot&) = delete;
    TaggedCoroutineSlot& operator=(const TaggedCoroutineSlot&) = delete;
    TaggedCoroutineSlot(TaggedCoroutineSlot&&) noexcept = default;
    TaggedCoroutineSlot& operator=(TaggedCoroutineSlot&&) noexcept = default;
    void SetHandle(std::coroutine_handle<> handle) noexcept;
    void SetReady(bool ready) noexcept;
    bool IsReady() const noexcept;
    std::coroutine_handle<> GetHandle() const noexcept;
};

struct Scheduler
{
    Scheduler();
    ~Scheduler();
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) noexcept = default;
    Scheduler& operator=(Scheduler&&) noexcept = default;
    std::expected<SlotHandle, RhiError> Enqueue(std::coroutine_handle<> handle);
    RhiError MarkReady(SlotHandle handle) noexcept;
    void Tick();

private:
    SlotMap<TaggedCoroutineSlot, 512> slotMap;
};

} // namespace velox

#endif // !VELOX_CONTEXT_IMPL_CPP
