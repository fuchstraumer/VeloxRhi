#include "async/Scheduler.hpp"
#include <cassert>
#include <stdexcept>
#ifndef __EMSCRIPTEN__
// use std::atomic_ref on desktop to handle async threads (dx12/vk pipeline creation workers)
// using the ref keeps us scoped to just where we need atomicity and doesn't break SlotMap constraints
#include <atomic>
#endif

namespace velox
{

TaggedCoroutineSlot::TaggedCoroutineSlot(std::coroutine_handle<> handle) noexcept
    : data{ reinterpret_cast<uintptr_t>(handle.address()) }
{
    assert((data & k_tagMask) == 0 && "Coroutine handle not aligned as expected");
}

TaggedCoroutineSlot::TaggedCoroutineSlot(TaggedCoroutineSlot&& other) noexcept : data{ other.data }
{
    other.data = 0;
}

TaggedCoroutineSlot& TaggedCoroutineSlot::operator=(TaggedCoroutineSlot&& other) noexcept
{
    if (this != &other)
    {
        data = other.data;
        other.data = uintptr_t(0);
    }
    return *this;
}

void TaggedCoroutineSlot::SetHandle(std::coroutine_handle<> handle) noexcept
{
    uintptr_t ptrValue = reinterpret_cast<uintptr_t>(handle.address());
    assert((ptrValue & k_tagMask) == 0);
#ifndef __EMSCRIPTEN__
    std::atomic_ref<uintptr_t> dataAtomic{ data };
    dataAtomic.store(ptrValue, std::memory_order::release);
#else
    data = ptrValue;
#endif
}

bool TaggedCoroutineSlot::SetReady(std::coroutine_handle<> handle) noexcept
{
    // long explanation incoming, because with cmpexchg and atomics it helps to be clear: especially
    // if we need to come back and debug or modify this later!
#ifndef __EMSCRIPTEN__
    // need to cmpexchg to make sure the handle didn't change underneath us, but only the pointer bits carry
    // that identity. SetAbandoned can land on this word at any point from the main thread, and toggle the
    // abandon bit. this actually makes it *more* important that we're able to set the ready bit, as the ready
    // bit is required to free the frame. previously, we weren't retrying to set the ready bit, which was fine
    // without the other potential writer.... but now we need to make sure we succeed, at least if the pointer bits are same!
    const uintptr_t expected = reinterpret_cast<uintptr_t>(handle.address());
    std::atomic_ref<uintptr_t> dataAtomic{ data };
    uintptr_t observed = dataAtomic.load(std::memory_order::acquire);
    // so, to fix the aforementioned problem: while the pointer bits are the same, we will try to set the ready
    // bit. If the pointer bits change, we will stop trying to set the ready bit, and return false to indicate
    // that the handle changed underneat us.
    while ((observed & k_ptrMask) == expected)
    {
        // now that we know the pointer bits are the same, we can try to set the ready bit. while doing this,
        // the abandoned bit could be set by another thread: in that case, use memory_order_acquire to make sure
        // we see that bit, and then try again to set our bit and use release to indicate we're done
        if (dataAtomic.compare_exchange_weak(observed,
                                             observed | k_readyBit,
                                             std::memory_order::release,
                                             std::memory_order::acquire))
        {
            return true;
        }
    }

    return false;
#else
    data |= k_readyBit;
    return true;
#endif
}

void TaggedCoroutineSlot::SetAbandoned() noexcept
{
#ifndef __EMSCRIPTEN__
    // memory_order_release is not strictly required here, but costs next to nothing on x86-64 and is good for our
    // goal of "thread-hardened" code as we work now instead of immediately threaded. this makes it easier, later.
    std::atomic_ref<uintptr_t> dataAtomic{ data };
    dataAtomic.fetch_or(k_abandonedBit, std::memory_order::release);
#else
    data |= k_abandonedBit;
#endif
}

std::coroutine_handle<> TaggedCoroutineSlot::GetHandleIfReady() const noexcept
{
#ifndef __EMSCRIPTEN__
    std::atomic_ref<const uintptr_t> dataAtomic{ data };
    uintptr_t value = dataAtomic.load(std::memory_order_acquire);
#else
    uintptr_t value = data;
#endif
    if (value & k_readyBit)
    {
        return std::coroutine_handle<>::from_address(reinterpret_cast<void*>(value & k_ptrMask));
    }
    else
    {
        return nullptr;
    }
}

SlotDisposition TaggedCoroutineSlot::GetDisposition(std::coroutine_handle<>& out_handle) const noexcept
{
#ifndef __EMSCRIPTEN__
    std::atomic_ref<const uintptr_t> dataAtomic{ data };
    const uintptr_t value = dataAtomic.load(std::memory_order_acquire);
#else
    const uintptr_t value = data;
#endif
    if ((value & k_readyBit) == 0u)
    {
        out_handle = nullptr;
        return SlotDisposition::Pending;
    }

    out_handle = std::coroutine_handle<>::from_address(reinterpret_cast<void*>(value & k_ptrMask));
    return (value & k_abandonedBit) ? SlotDisposition::Destroy : SlotDisposition::Resume;
}

Scheduler::Scheduler()
{
}

Scheduler::~Scheduler()
{
    // todo-ship: ensure coroutine queue in scheduler is fully drained before destruction (or during)
    // we have groundwork for this now - Abandon() will enqueue canceled or faulty coroutines to be destroyed
    assert(slotMap.Empty());
}

SlotHandle Scheduler::Enqueue(std::coroutine_handle<> handle) noexcept
{
    assert(!slotMap.Full() && "SlotMap at capacity, too many coroutines emplaced without clearing!");
    return slotMap.Emplace(handle);
}

RhiError Scheduler::MarkReady(SlotHandle mapHandle, std::coroutine_handle<> coroHandle) noexcept
{
    if (!slotMap.Contains(mapHandle))
    {
        return RhiError::AsyncSchedulerMarkReadyFailed;
    }
    TaggedCoroutineSlot& slot = slotMap[mapHandle];
    return slot.SetReady(coroHandle) ? RhiError::Success : RhiError::AsyncSchedulerMarkReadyFailed;
}

void Scheduler::Tick()
{
    // use this lambda as the predicate for EraseIf: if the value is ready, we resume it *then* return
    // true in the predicate (so it can be erased. easy!)
    // an abandoned frame is destroyed rather than resumed: its Future is gone, so there is nobody left to
    // read the result, and resuming would only run the body and final_suspend to no purpose. destroying
    // here still runs the suspended awaitable's destructor, which is what releases the wgpu object the
    // callback deposited into it
    auto erasePredicate = [](TaggedCoroutineSlot& slot) -> bool
    {
        std::coroutine_handle<> coro;
        switch (slot.GetDisposition(coro))
        {
        case SlotDisposition::Resume:
            coro.resume();
            return true;
        case SlotDisposition::Destroy:
            coro.destroy();
            return true;
        default:
            return false;
        }
    };
    slotMap.EraseIf(erasePredicate);
}

bool Scheduler::IsHandleAlive(SlotHandle mapHandle, std::coroutine_handle<> coroHandle) const noexcept
{
    if (!slotMap.Contains(mapHandle))
    {
        return false;
    }
    const TaggedCoroutineSlot& slot = slotMap[mapHandle];
    return slot.GetHandleIfReady() == coroHandle;
}

void Scheduler::Abandon(SlotHandle mapHandle, [[maybe_unused]] std::coroutine_handle<> coroHandle) noexcept
{
    TaggedCoroutineSlot* slot = slotMap.TryGet(mapHandle);
    if (!slot)
    {
        // the slot was already drained by a Tick, so the frame either ran to completion or was resumed and
        // is no longer ours to hand off. the caller owns it and must destroy it itself
        assert(coroHandle && coroHandle.done());
        return;
    }

    slot->SetAbandoned();
}

} // namespace velox
