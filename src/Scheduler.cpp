#include "Scheduler.hpp"
#include <cassert>
#include <stdexcept>

namespace velox
{

TaggedCoroutineSlot::TaggedCoroutineSlot(std::coroutine_handle<> handle) noexcept
    : data{ reinterpret_cast<uintptr_t>(handle.address()) }
{
    assert((data & k_readyBit) == 0 && "Coroutine handle not aligned as expected");
}

void TaggedCoroutineSlot::SetHandle(std::coroutine_handle<> handle) noexcept
{
    uintptr_t ptrValue = reinterpret_cast<uintptr_t>(handle.address());
    assert((ptrValue & k_readyBit == 0) && "Coroutine handle not aligned correctly");
    data = ptrValue;
}

void TaggedCoroutineSlot::SetReady(bool ready) noexcept
{
    if (ready)
    {
        data |= k_readyBit;
    }
    else
    {
        data &= k_ptrMask;
    }
}

bool TaggedCoroutineSlot::IsReady() const noexcept
{
    return (data & k_readyBit) != 0;
}

std::coroutine_handle<> TaggedCoroutineSlot::GetHandle() const noexcept
{
    void* coroPtr = reinterpret_cast<void*>(data & k_ptrMask);
    return std::coroutine_handle<>::from_address(coroPtr);
}

Scheduler::Scheduler()
{
}

Scheduler::~Scheduler()
{
    // todo: ensure coroutine queue in scheduler is fully drained before destruction (or during)
    assert(slotMap.Empty());
}

std::expected<SlotHandle, RhiError> Scheduler::Enqueue(std::coroutine_handle<> handle)
{
    SlotHandle result = slotMap.Emplace(handle);
    if (result.IsValid()) [[likely]]
    {
        return result;
    }
    else
    {
        return std::unexpected(RhiError::AsyncSchedulerEnqueueFailed);
    }
}

RhiError Scheduler::MarkReady(SlotHandle handle) noexcept
{
    if (!slotMap.Contains(handle))
    {
        return RhiError::AsyncSchedulerMarkReadyFailed;
    }
    TaggedCoroutineSlot& slot = slotMap[handle];
    slot.SetReady(true);
    return RhiError::Success;
}

void Scheduler::Tick()
{
    // use this lambda as the predicate for EraseIf: if the value is ready, we resume it *then* return
    // true in the predicate (so it can be erased. easy!)
    auto erasePredicate = [](TaggedCoroutineSlot& slot)->bool
    {
        if (slot.IsReady())
        {
            std::coroutine_handle<> coro = slot.GetHandle();
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
