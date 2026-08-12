#pragma once
#ifndef VELOX_RHI_ASYNC_TASKS_HPP
#define VELOX_RHI_ASYNC_TASKS_HPP
#include "AsyncCore.hpp"
#include "Scheduler.hpp"
#include <coroutine>
#include <memory>
#include <webgpu/webgpu_cpp.h>

#ifndef NDEBUG
#include <magic_enum/magic_enum.hpp>
#include <print>
#endif

namespace velox
{

namespace detail
{

    // todo: for operations that have more than just success/fail status enum values, we need to
    // have Velox error enum values that match those to store in std::unexpected
    template<typename StatusEnum>
    inline void PrintStatusMessage(const char* ourMsg, StatusEnum status, wgpu::StringView message)
    {
#ifndef NDEBUG
        std::string_view statusText = magic_enum::enum_name(status);
        std::string_view wgpuMessage(message.data, message.length);
        std::println(stderr, "[velox][async] {} | status: {} | message: {}", ourMsg, statusText, wgpuMessage);
#endif
    }

} // namespace detail


// todo-ship: Since everything needs a Scheduler pointer at this point, that should be created by the Future
// and used with await_transform so that it gets forwarded without us needing to pass it
// even better: move the Future functions to be Context members so they get it direct from that

// todo-design: It would be nice to wrap WGPU types in a way that we can include just the types and structs
// required at the header level, and then only include full WGPU headers in source files. This would make
// things less messy and make portability easier in the future.

struct AdapterAwaitable final : SchedulerDispatchedAwaitable<AdapterAwaitable>
{
    wgpu::Instance instance;
    wgpu::RequestAdapterOptions options;
    Result<wgpu::Adapter> result;
    Scheduler* scheduler;

    AdapterAwaitable(wgpu::Instance _instance,
                     wgpu::RequestAdapterOptions _options,
                     Scheduler* _scheduler) noexcept;

    void Dispatch(Scheduler* _scheduler, SlotHandle slot_handle, std::coroutine_handle<> coro_handle) noexcept;

    Result<wgpu::Adapter> await_resume() noexcept;

    constexpr explicit operator bool() const noexcept
    {
        return result.has_value();
    }
};

struct DeviceAwaitable final : SchedulerDispatchedAwaitable<DeviceAwaitable>
{
    wgpu::Adapter adapter;
    wgpu::DeviceDescriptor descriptor;
    Result<wgpu::Device> result;
    Scheduler* scheduler;

    DeviceAwaitable(wgpu::Adapter _adapter,
                    wgpu::DeviceDescriptor _descriptor,
                    Scheduler* _scheduler) noexcept;
    void Dispatch(Scheduler* _scheduler, SlotHandle slot_handle, std::coroutine_handle<> coro_handle) noexcept;
    Result<wgpu::Device> await_resume() noexcept;

    constexpr explicit operator bool() const noexcept
    {
        return result.has_value();
    }
};

struct MapReadAwaitable final : SchedulerDispatchedAwaitable<MapReadAwaitable>
{
private:
    wgpu::Buffer buffer{};
    size_t size{ std::numeric_limits<size_t>::max() };
    size_t offset{ 0u };
    Result<const void*> result;
    Scheduler* scheduler{ nullptr };

public:
    MapReadAwaitable(wgpu::Buffer _buffer,
                     size_t _size,
                     size_t _offset = 0u,
                     Scheduler* _scheduler = nullptr) noexcept;

    void Dispatch(Scheduler* _scheduler, SlotHandle slot_handle, std::coroutine_handle<> coro_handle) noexcept;

    Result<const void*> await_resume() const noexcept;
};

struct MapWriteAwaitable final : SchedulerDispatchedAwaitable<MapWriteAwaitable>
{
private:
    wgpu::Buffer buffer{};
    size_t size{ std::numeric_limits<size_t>::max() };
    size_t offset{ 0u };
    Result<void*> result;
    Scheduler* scheduler{ nullptr };

public:
    MapWriteAwaitable(wgpu::Buffer _buffer,
                      size_t _size,
                      size_t _offset = 0u,
                      Scheduler* _scheduler = nullptr) noexcept;

    void Dispatch(Scheduler* _scheduler, SlotHandle slot_handle, std::coroutine_handle<> coro_handle) noexcept;

    Result<void*> await_resume() const noexcept;
};

template<typename MapType>
struct MapSession
{
    using PointerType = std::conditional_t<std::is_same_v<MapReadAwaitable, MapType>, const void*, void*>;

public:
    MapSession(const MapSession&) = delete;
    MapSession& operator=(const MapSession&) = delete;
    ~MapSession()
    {
        buffer.Unmap();
    }

    // static Task<MapSession<Mode>> CreateAsync(wgpu::Buffer buffer, size_t _size, size_t _offset)
    //{
    //     MapResultType<Mode> mapResult = co_await BufferMapAwaitable<Mode>{ buffer, _size, _offset };
    //     co_return MapSession<Mode>(buffer, _size, _offset, data);
    // }

    template<typename T>
        requires(std::is_same_v<MapType, MapWriteAwaitable>)
    std::span<T> GetDataAs() noexcept
    {
        assert(reinterpret_cast<std::uintptr_t>(mappedPtr) % alignof(T) == 0);
        static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>,
                      "T must be standard layout to interpret mapped data as span of T");
        const size_t numElements = size / sizeof(T);
#ifndef __EMSCRIPTEN__
        T* typeArray = std::start_lifetime_as_array<T>(mappedPtr, numElements);
#else
        T* typeArray = reinterpret_cast<T*>(mappedPtr);
#endif
        return std::span<T>(typeArray, typeArray + numElements);
    }

    template<typename T>
        requires(std::is_same_v<MapType, MapReadAwaitable>)
    std::span<const T> GetDataAs() const noexcept
    {
        assert(reinterpret_cast<std::uintptr_t>(mappedPtr) % alignof(T) == 0);
        static_assert(
            std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>,
            "T must be standard layout and trivially copyable to interpret mapped data as span of T");
        const size_t numElements = size / sizeof(T);
#ifndef __EMSCRIPTEN__
        const T* typeArray = std::start_lifetime_as_array<T>(mappedPtr, numElements);
#else
        const T* typeArray = reinterpret_cast<const T*>(mappedPtr);
#endif
        return std::span<const T>(typeArray, typeArray + numElements);
    }

    void* GetDataPtr() noexcept
        requires(std::is_same_v<MapType, MapWriteAwaitable>)
    {
        return mappedPtr;
    }

    const void* GetDataPtr() const noexcept
        requires(std::is_same_v<MapType, MapReadAwaitable>)
    {
        return mappedPtr;
    }

private:
    MapSession(wgpu::Buffer _buffer, size_t _size, size_t _offset, PointerType _data) noexcept
        : buffer{ _buffer },
          size{ _size },
          offset{ _offset },
          mappedPtr{ _data }
    {
    }
    wgpu::Buffer buffer;
    size_t size;
    size_t offset;
    PointerType mappedPtr;
};

struct RenderPipelineAwaitable final : SchedulerDispatchedAwaitable<RenderPipelineAwaitable>
{
private:
    wgpu::Device device{};
    wgpu::RenderPipelineDescriptor descriptor{};
    Result<wgpu::RenderPipeline> result{};
    Scheduler* scheduler{ nullptr };

public:
    RenderPipelineAwaitable(wgpu::Device _device,
                            wgpu::RenderPipelineDescriptor _descriptor,
                            Scheduler* _scheduler) noexcept;
    ~RenderPipelineAwaitable() noexcept = default;
    RenderPipelineAwaitable(const RenderPipelineAwaitable&) = delete;
    RenderPipelineAwaitable& operator=(const RenderPipelineAwaitable&) = delete;
    void Dispatch(Scheduler* _scheduler, SlotHandle slot_handle, std::coroutine_handle<> coro_handle) noexcept;
    Result<wgpu::RenderPipeline> await_resume() noexcept;
};

struct ComputePipelineAwaitable final : SchedulerDispatchedAwaitable<ComputePipelineAwaitable>
{
private:
    wgpu::Device device{};
    wgpu::ComputePipelineDescriptor descriptor{};
    Result<wgpu::ComputePipeline> result{};
    Scheduler* scheduler{ nullptr };

public:
    ComputePipelineAwaitable(wgpu::Device _device,
                             wgpu::ComputePipelineDescriptor _descriptor,
                             Scheduler* _scheduler) noexcept;
    ~ComputePipelineAwaitable() noexcept = default;
    ComputePipelineAwaitable(const ComputePipelineAwaitable&) = delete;
    ComputePipelineAwaitable& operator=(const ComputePipelineAwaitable&) = delete;
    void Dispatch(Scheduler* _scheduler, SlotHandle slot_handle, std::coroutine_handle<> coro_handle) noexcept;
    Result<wgpu::ComputePipeline> await_resume() noexcept;
};

} // namespace velox

#endif // !VELOX_RHI_ASYNC_TASKS_HPP
