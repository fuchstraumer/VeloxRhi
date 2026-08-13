#pragma once
#ifndef VELOX_ASYNC_FUTURE_HPP
#define VELOX_ASYNC_FUTURE_HPP
#include "AsyncTasks.hpp"
#include "CoroutineAllocator.hpp"

namespace velox
{

/**Brainblast moment: this is what holds the promise type. This is what defines the type of the result, what
 * actually carries some type information, and also what owns the coroutine_handle. Scheduler is not just a
 * big pile of type-erased coroutine handles. This simplifies awaitable dispatching tremendously.
 * todo: stop_token, stop_source, stop_callback etc: need to cancel pending coroutines on device loss or quit
 * this will be important for shipping to clients without grenading their browser tab state etc
 */
template<typename T>
struct Future
{
    struct promise_type final : BasePromise
    {
        using ValueType = T;
        Result<T> result_value;

        Future<T> get_return_object() noexcept
        {
            return Future<T>(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        // todo: await_transform should eventually be used to enforce boundaries between 
        // execution/module domains of awaitables, so we don't have rendering waiting on DSP etc

        // don't suspend initially, because we call the async function and let the awaitable
        // that constructs be what first sends us into suspension (after enqueuing our action)
        constexpr std::suspend_never initial_suspend() noexcept
        {
            return {};
        }

        // suspend_always->FinalAwaiter to handle continuation chaining
        FinalAwaiter final_suspend() noexcept
        {
            return {};
        }

        static void* operator new(std::size_t size)
        {
            return g_CoroutineAllocator.Allocate(size);
        }

        static void operator delete(void* ptr, std::size_t size)
        {
            g_CoroutineAllocator.Deallocate(ptr, size);
        }

        template<typename U>
        void return_value(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)
        {
            result_value = std::forward<U>(value);
        }

        void unhandled_exception()
        {
#ifndef __EMSCRIPTEN__
            std::terminate();
#else
            emscripten_force_exit(1);
#endif
        }
    };

    std::coroutine_handle<promise_type> handle;

    constexpr Future() noexcept
        : handle{ nullptr }
    {
    }

    constexpr explicit Future(std::coroutine_handle<promise_type> _handle) noexcept
        : handle{ _handle }
    {
    }

    ~Future()
    {
        release();
    }

    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    Future(Future&& other) noexcept : handle{ other.handle }
    {
        other.handle = nullptr;
    }

    Future& operator=(Future&& other) noexcept
    {
        if (this != &other)
        {
            release();
            handle = std::exchange(other.handle, nullptr);
        }
        return *this;
    }

    // because this now holds the coroutine and promise, this simplifies tremendously
    std::optional<Result<T>> TryGet()
    {
        // simple as can be: if handle isn't done, return nullopt. awesome.
        if (!handle || !handle.done())
        {
            return std::nullopt;
        }

        Result<T> value = std::move(handle.promise().result_value);
        release(); // call release here too, just to be standard about where we use handle.destroy()
        return value;
    }

    constexpr explicit operator bool() const noexcept
    {
        return handle != nullptr;
    }

private:

    void release() noexcept
    {
        if (!handle)
        {
            return;
        }

        if (handle.done())
        {
            // body ran to final suspend, so we can destroy the coroutine frame now
            // (there are no other pending owners, final_suspend was reached, all clear)
            handle.destroy();
        }
        else
        {
            // body is still running: get the promise, and use that to hand off to the 
            // scheduler. the scheduler will tick and destroy the frame once the callback lands
            auto& promise = handle.promise();
            promise.Scheduler->Abandon(promise.SlotHandle, handle);
        }

        handle = nullptr;
    }
};

using AdapterFuture = Future<wgpu::Adapter>;
using DeviceFuture = Future<wgpu::Device>;
using RenderPipelineFuture = Future<wgpu::RenderPipeline>;
using ComputePipelineFuture = Future<wgpu::ComputePipeline>;
using MapReadFuture = Future<Result<const void*>>;
using MapWriteFuture = Future<Result<void*>>;

AdapterFuture RequestAdapter(wgpu::Instance _instance,
                             wgpu::RequestAdapterOptions _options,
                             Scheduler* _scheduler);

DeviceFuture RequestDevice(wgpu::Adapter _adapter, wgpu::DeviceDescriptor _descr, Scheduler* _scheduler);

RenderPipelineFuture RequestRenderPipeline(wgpu::Device _device,
                                           wgpu::RenderPipelineDescriptor _descr,
                                           Scheduler* _scheduler);

ComputePipelineFuture RequestComputePipeline(wgpu::Device _device,
                                             wgpu::ComputePipelineDescriptor _descr,
                                             Scheduler* _scheduler);

} // namespace velox

#endif // !VELOX_ASYNC_FUTURE_HPP
