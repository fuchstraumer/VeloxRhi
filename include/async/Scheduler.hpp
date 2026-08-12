#pragma once
#ifndef VELOX_SCHEDULER_HPP
#define VELOX_SCHEDULER_HPP
#include "common/VeloxErrors.hpp"
#include "utility/SlotMap.hpp"
#include <coroutine>

namespace velox
{

// What Tick() should do with a slot this pass. Ready and abandoned are two independent tag bits, but they
// are only ever acted on together, and they must be read in one load: a callback landing between two
// separate reads would otherwise let an abandoned frame be resumed instead of destroyed.
enum class SlotDisposition : uint8_t
{
    Invalid = 0,
    Pending,  // callback hasn't landed, leave it alone
    Resume,   // callback landed, owning Future is still alive
    Destroy,  // callback landed, owning Future let go of the frame while it was in flight
};

// These coroutines will always be heap allocated, meaning 2-3 of the LSBs will be unused. We can use these
// for tagging / our boolean ready checks, and size "dynamically" for the platforms pointer width.
struct TaggedCoroutineSlot
{
private:
    uintptr_t data{ 0u };
    static constexpr uintptr_t k_readyBit = 1ULL;
    static constexpr uintptr_t k_abandonedBit = 2ULL;
    static constexpr uintptr_t k_tagMask = k_readyBit | k_abandonedBit;
    static constexpr uintptr_t k_ptrMask = ~k_tagMask;
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
    // unconditional OR rather than a cmpexchg: the callback may land on another thread at any point during
    // this call, and either interleaving has to leave both bits set
    void SetAbandoned() noexcept;
    std::coroutine_handle<> GetHandleIfReady() const noexcept;
    SlotDisposition GetDisposition(std::coroutine_handle<>& out_handle) const noexcept;
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
    // ownership transfer from frame to scheduler. the frame is *not* destroyed here: an in-flight callback
    // still holds a pointer into it, and writes through that pointer when it lands. Tick() destroys the
    // frame once the slot reports ready, which is the point the callback is provably done touching it
    void Abandon(SlotHandle mapHandle, std::coroutine_handle<> coroHandle) noexcept;
private:
    SlotMap<TaggedCoroutineSlot, 2048> slotMap;
};

} // namespace velox

#endif // !VELOX_SCHEDULER_HPP
