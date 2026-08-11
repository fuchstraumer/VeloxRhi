#pragma once
#ifndef VELOX_SCHEDULER_HPP
#define VELOX_SCHEDULER_HPP
#include "common/VeloxErrors.hpp"
#include "utility/SlotMap.hpp"
#include <coroutine>

namespace velox
{

// These coroutines will always be heap allocated, meaning 2-3 of the LSBs will be unused. We can use these
// for tagging / our boolean ready checks, and size "dynamically" for the platforms pointer width.
struct TaggedCoroutineSlot
{
private:
    uintptr_t data{ 0u };
    static constexpr uintptr_t k_readyBit = 1ULL;
    static constexpr uintptr_t k_ptrMask = ~k_readyBit;
public:
    TaggedCoroutineSlot(std::coroutine_handle<> handle) noexcept;
    ~TaggedCoroutineSlot() noexcept = default;
    TaggedCoroutineSlot(const TaggedCoroutineSlot&) = delete;
    TaggedCoroutineSlot& operator=(const TaggedCoroutineSlot&) = delete;
    TaggedCoroutineSlot(TaggedCoroutineSlot&&) noexcept;
    TaggedCoroutineSlot& operator=(TaggedCoroutineSlot&&) noexcept;
    void SetHandle(std::coroutine_handle<> handle) noexcept;
    // we need to take handle on desktop cmpexchg against stored handle, to make sure it hasn't swapped underneath us
    bool SetReady([[maybe_unused]] std::coroutine_handle<> handle) noexcept;
    std::coroutine_handle<> GetHandleIfReady() const noexcept;
};

// todo: Eventually we will want continuations, sort them so continuation series' are memory adjacent then
struct Scheduler
{
    Scheduler();
    ~Scheduler();
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) noexcept = default;
    Scheduler& operator=(Scheduler&&) noexcept = default;
    SlotHandle Enqueue(std::coroutine_handle<> handle) noexcept;
    RhiError MarkReady(SlotHandle mapHandle, std::coroutine_handle<> coroHandle) noexcept;
    void Tick();
    // if handle (future) was destroyed, the callback lambda (which captures by value) will still have that handle
    // value saved. first, we want to check if the handle is around: if it's not, that means the future was destroyed,
    // and we need to abandon the coroutine frame. If it is still around, we can resume it.
    bool IsHandleAlive(SlotHandle mapHandle, std::coroutine_handle<> coroHandle) const noexcept;
    // ownership transfer from frame to scheduler, scheduler will destroy the coroutine only when it is *done*
    void Abandon(SlotHandle mapHandle, std::coroutine_handle<> coroHandle) noexcept;
private:
    SlotMap<TaggedCoroutineSlot, 2048> slotMap;
};

} // namespace velox

#endif // !VELOX_SCHEDULER_HPP
