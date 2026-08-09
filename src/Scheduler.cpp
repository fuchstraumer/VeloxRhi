#include "Scheduler.hpp"
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
    assert((data & k_readyBit) == 0 && "Coroutine handle not aligned as expected");
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
    assert((ptrValue & k_readyBit) == 0);
#ifndef __EMSCRIPTEN__
    std::atomic_ref<uintptr_t> dataAtomic{ data };
    dataAtomic.store(ptrValue, std::memory_order::release);
#else
    data = ptrValue;
#endif
}

bool TaggedCoroutineSlot::SetReady(std::coroutine_handle<> handle) noexcept
{
#ifndef __EMSCRIPTEN__
    // need to cmpexchg to make sure whole handle didn't change underneath us
    // we expect to find this address, unchanged yet, and want to set the ready bit
    uintptr_t expected = reinterpret_cast<uintptr_t>(handle.address());
    uintptr_t desired = expected | k_readyBit;
    std::atomic_ref<uintptr_t> dataAtomic{ data };
    return dataAtomic.compare_exchange_strong(expected, desired, std::memory_order_release);
#else
    data |= k_readyBit;
    return true;
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

Scheduler::Scheduler()
{
}

Scheduler::~Scheduler()
{
    // todo: ensure coroutine queue in scheduler is fully drained before destruction (or during)
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
    auto erasePredicate = [](TaggedCoroutineSlot& slot) -> bool
    {
        if (std::coroutine_handle<> coro = slot.GetHandleIfReady())
        {
            coro.resume();
            return true;
        }
        else
        {
            return false;
        }
    };
    slotMap.EraseIf(erasePredicate);
}

} // namespace velox
