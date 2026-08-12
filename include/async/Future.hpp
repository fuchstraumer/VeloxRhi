#pragma once
#ifndef VELOX_ASYNC_FUTURE_HPP
#define VELOX_ASYNC_FUTURE_HPP
#include "AsyncTasks.hpp"
#include "CoroutineAllocator.hpp"

namespace velox
{

struct Scheduler;

namespace detail
{
    
}

/**Brainblast moment: this is what holds the promise type. This is what defines the type of the result, what
 * actually carries some type information, and also what owns the coroutine_handle. Scheduler is not just a
 * big pile of type-erased coroutine handles. This simplifies awaitable dispatching tremendously.
 * todo: stop_token, stop_source, stop_callback etc: need to cancel pending coroutines on device loss or quit
 * this will be important for shipping to clients without grenading their browser tab state etc
 */
template<typename T>
struct Future
{
    struct promise_type
    {
        Result<T> result_value;
        std::coroutine_handle<> continuation;

        struct FinalAwaiter
        {
            constexpr bool await_ready() const noexcept
            {
                return false;
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> handle) noexcept
            {
                auto& promise = handle.promise();
                if (promise.continuation)
                {
                    return promise.continuation;
                }
                else
                {
                    return std::noop_coroutine();
                }
            }

            constexpr void await_resume() const noexcept
            {
            }
        };

        Future<T> get_return_object() noexcept
        {
            return Future<T>(std::coroutine_handle<promise_type>::from_promise(*this));
        }

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
        if (handle && !handle.done())
        {
            handle.destroy();
        }
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
            if (handle && !handle.done())
            {
                handle.destroy();
            }
            handle = other.handle;
            other.handle = nullptr;
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
        // now clean up coroutine, as we're totally done
        handle.destroy();
        handle = nullptr;
        return value;
    }

    constexpr explicit operator bool() const noexcept
    {
        return handle != nullptr;
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
