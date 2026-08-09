#pragma once
#ifndef VELOX_SCHEDULER_HPP
#define VELOX_SCHEDULER_HPP
#include "VeloxErrors.hpp"
#include "utility/SlotMap.hpp"
#include <coroutine>
#include <expected>

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
    SlotHandle Enqueue(std::coroutine_handle<> handle) noexcept;
    RhiError MarkReady(SlotHandle handle) noexcept;
    void Tick();

private:
    SlotMap<TaggedCoroutineSlot, 2048> slotMap;
};

} // namespace velox

#endif // !VELOX_SCHEDULER_HPP
