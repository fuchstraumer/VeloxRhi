#pragma once
#ifndef VELOX_ASYNC_CORE_HPP
#define VELOX_ASYNC_CORE_HPP
#include "common/VeloxErrors.hpp"
#include "utility/SlotMap.hpp"
#include <type_traits>
#include <coroutine>
#include <expected>

#ifdef _MSC_VER
#define FINAL_AWAITER_NOINLINE __declspec(noinline)
#else
#define FINAL_AWAITER_NOINLINE
#endif

/**@brief Defines core types behind the coroutine-based async framework used in this engine: does not 
 * define any actual awaitables or futures, but provides the base types (and Concept contracts) for 
 * those to be built on.
 */
namespace velox
{

struct Scheduler;

struct BasePromise
{
    std::coroutine_handle<> Continuation;
    // todo/design: We may want to promote this to an Interface class, but that's not needed now. Just keep it in mind.
    Scheduler* Scheduler;
    SlotHandle SlotHandle;
};

template<typename PromiseType>
concept ValidPromiseType = requires(PromiseType promise)
{
    typename PromiseType::ValueType;
    typename PromiseType::ErrorType;
    
};

template<typename PromiseType>
concept ValidPromiseType = 
    std::derived_from<PromiseType, BasePromise> &&
    requires(PromiseType promise)
    {
        typename PromiseType::ValueType;
        { promise.result_value } -> std::same_as<Result<typename PromiseType::ValueType>&>;
    };

template<typename T>
concept Awaitable = requires(T&& awaitable)
{
    { awaitable.await_ready() } -> std::convertible_to<bool>;
    { awaitable.await_suspend(std::coroutine_handle<>{}) };
    { awaitable.await_resume() };
} || requires(T&& awaitable)
{
    { operator co_await(std::forward<T>(awaitable)) } -> Awaitable;
};

enum class FutureType : uint8_t
{
    Invalid,
    Immediate, // Future that is executed immediately and synchronously
    Deferred,  // Future that is deferred and will be executed lazily
};

struct FinalAwaiter
{
    constexpr bool await_ready() const noexcept
    {
        return false;
    }

    template<ValidPromiseType Promise>
    FINAL_AWAITER_NOINLINE std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept
    {
        auto& promise = h.promise();
        // besides checking continuation, should we check error?
        // not yet
        if (promise.Continuation)
        {
            return promise.Continuation;
        }
        else
        {
            return std::noop_coroutine();
        }
    }

    constexpr void await_resume() const noexcept
    {
        std::unreachable();
    }
};

// CRTP for our base Scheduler-dispatched awaitable: allows dedupe of common code, not total template overrun
// (as awaitables can still put *some* of their code in source files), and gives us a common interface
// it has been almost a decade since ive used CRTP, so this is kind of fun
template<typename Derived>
struct SchedulerDispatchedAwaitable
{
    constexpr bool await_ready() const noexcept
    {
        return false;
    }

    template<ValidPromiseType Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> coroHandle) noexcept
    {
        BasePromise& promise = coroHandle.promise();
        if (promise.Scheduler)
        {
            promise.SlotHandle = promise.Scheduler->Enqueue(coroHandle);
        }
        // now each derived awaitable will dispatch to... dispatch function. this is where
        // we'll put actual logic, in source files ideally, since it can pull in other headers
        static_cast<Derived*>(this)->Dispatch(promise.Scheduler, promise.SlotHandle, coroHandle);
    }

    // no await_resume() as that depends on result type
};

} // namespace velox

#endif // !VELOX_ASYNC_CORE_HPP
