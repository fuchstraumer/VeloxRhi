#include "async/AsyncTasks.hpp"
#include "async/Scheduler.hpp"
#include "utility/SlotMap.hpp"

namespace
{
// todo-ship: Finish creating shims to convert wgpu request statuses into RhiErrors
constexpr velox::RhiError RhiErrorFromWgpuAdapterStatus(wgpu::RequestAdapterStatus status) noexcept
{
    return velox::RhiError::Success;
}

// We need to convert the error code to some common schema: so far we know that the code 2 is always shared between all
// status enums; cancelled is as well. Others beyond those two are specific to the object. The first two codes tell us
// we need to abandon the coroutine frame, handing it off to scheduler to tick and then destroy
template<typename StatusEnum>
constexpr bool StatusEnumIsAbandonable(StatusEnum status) noexcept
{
    return static_cast<std::underlying_type_t<StatusEnum>>(status) == 2 ||
           static_cast<std::underlying_type_t<StatusEnum>>(status) == 3;
}
} // namespace

namespace velox
{

AdapterAwaitable::AdapterAwaitable(wgpu::Instance _instance,
                                   wgpu::RequestAdapterOptions _options,
                                   Scheduler* _scheduler) noexcept
    : instance{ _instance },
      options{ std::forward<wgpu::RequestAdapterOptions>(_options) },
      scheduler{ _scheduler }
{
}

void AdapterAwaitable::Dispatch(Scheduler* _scheduler, SlotHandle slotHandle, std::coroutine_handle<> coro_handle) noexcept
{
    auto reqFn = [this, _scheduler, slotHandle, coro_handle](wgpu::RequestAdapterStatus status,
                                                             wgpu::Adapter adapter,
                                                             wgpu::StringView message)
    {
        if (status != wgpu::RequestAdapterStatus::Success)
        {
            detail::PrintStatusMessage("RequestAdapter", status, message);
            result = std::unexpected(velox::RhiError::AdapterRequestFailed);
        }
        else [[likely]]
        {
            result = adapter;
        }

        if (!scheduler) [[unlikely]]
        {
            coro_handle.resume();
        }
        else
        {
            RhiError error = scheduler->MarkReady(slotHandle, coro_handle);
            if (error != RhiError::Success) [[unlikely]]
            {
                result = std::unexpected(error);
            }
        }
    };
    instance.RequestAdapter(&options,
                            wgpu::CallbackMode::AllowSpontaneous,
                            std::move(reqFn));
}

Result<wgpu::Adapter> AdapterAwaitable::await_resume() noexcept
{
    return std::move(result);
}

DeviceAwaitable::DeviceAwaitable(wgpu::Adapter _adapter,
                                 wgpu::DeviceDescriptor _descriptor,
                                 Scheduler* _scheduler) noexcept
    : adapter{ _adapter },
      descriptor{ std::forward<wgpu::DeviceDescriptor>(_descriptor) },
      scheduler{ _scheduler }
{
}

void DeviceAwaitable::Dispatch(Scheduler* _scheduler, SlotHandle slot, std::coroutine_handle<> coro) noexcept
{
    auto reqFn = [this, _scheduler, slot, coro](wgpu::RequestDeviceStatus status,
                                                wgpu::Device device,
                                                wgpu::StringView message)
    {
        if (status != wgpu::RequestDeviceStatus::Success)
        {
            detail::PrintStatusMessage("RequestDevice", status, message);
            result = std::unexpected(velox::RhiError::DeviceRequestFailed);
        }
        else [[likely]]
        {
            result = device;
        }

        if (!scheduler) [[unlikely]]
        {
            coro.resume();
        }
        else
        {
            RhiError error = scheduler->MarkReady(slot, coro);
            if (error != RhiError::Success) [[unlikely]]
            {
                result = std::unexpected(error);
            }
        }
    };
    adapter.RequestDevice(&descriptor,
                          wgpu::CallbackMode::AllowSpontaneous,
                          std::move(reqFn));
}

Result<wgpu::Device> DeviceAwaitable::await_resume() noexcept
{
    return std::move(result);
}

MapReadAwaitable::MapReadAwaitable(wgpu::Buffer _buffer,
                                   size_t _size,
                                   size_t _offset,
                                   Scheduler* _scheduler) noexcept
    : buffer{ _buffer },
      size{ _size },
      offset{ _offset },
      scheduler{ _scheduler }
{
}

void MapReadAwaitable::Dispatch(Scheduler* _scheduler, SlotHandle slot, std::coroutine_handle<> coro) noexcept
{
    auto reqFn = [this, _scheduler, slot, coro](wgpu::MapAsyncStatus status, wgpu::StringView message)
    {
        if (status != wgpu::MapAsyncStatus::Success)
        {
            detail::PrintStatusMessage("MapBuffer", status, message);
            result = std::unexpected(RhiError::AsyncBufferMapFailed);
        }

        if (!scheduler)
        {
            coro.resume();
        }
        else
        {
            RhiError error = scheduler->MarkReady(slot, coro);
            if (error != RhiError::Success) [[unlikely]]
            {
                result = std::unexpected(error);
            }
        }
    };
    buffer.MapAsync(wgpu::MapMode::Read,
                    offset,
                    size,
                    wgpu::CallbackMode::AllowSpontaneous,
                    std::move(reqFn));
}

Result<const void*> MapReadAwaitable::await_resume() const noexcept
{
    if (!result.has_value())
    {
        return result;
    }
    else [[likely]]
    {
        return buffer.GetConstMappedRange(offset, size);
    }
}

MapWriteAwaitable::MapWriteAwaitable(wgpu::Buffer _buffer,
                                     size_t _size,
                                     size_t _offset,
                                     Scheduler* _scheduler) noexcept
    : buffer{ _buffer },
      size{ _size },
      offset{ _offset },
      scheduler{ _scheduler }
{
}

void MapWriteAwaitable::Dispatch(Scheduler* _scheduler, SlotHandle slot, std::coroutine_handle<> coro) noexcept
{
    auto reqFn = [this, _scheduler, slot, coro](wgpu::MapAsyncStatus status, wgpu::StringView message)
    {
        if (status != wgpu::MapAsyncStatus::Success)
        {
            detail::PrintStatusMessage("MapBuffer", status, message);
            result = std::unexpected(RhiError::AsyncBufferMapFailed);
        }

        if (!scheduler)
        {
            coro.resume();
        }
        else
        {
            RhiError error = scheduler->MarkReady(slot, coro);
            if (error != RhiError::Success) [[unlikely]]
            {
                result = std::unexpected(error);
            }
        }
    };
    buffer.MapAsync(wgpu::MapMode::Write,
                    offset,
                    size,
                    wgpu::CallbackMode::AllowSpontaneous,
                    std::move(reqFn));
}

Result<void*> MapWriteAwaitable::await_resume() const noexcept
{
    if (!result.has_value())
    {
        return result;
    }
    else [[likely]]
    {
        return buffer.GetMappedRange(offset, size);
    }
}

RenderPipelineAwaitable::RenderPipelineAwaitable(wgpu::Device _device,
                                                 wgpu::RenderPipelineDescriptor _descriptor,
                                                 Scheduler* _scheduler) noexcept
    : device{ _device },
      descriptor{ std::forward<wgpu::RenderPipelineDescriptor>(_descriptor) },
      scheduler{ _scheduler }
{
}

void RenderPipelineAwaitable::Dispatch(Scheduler* _scheduler, SlotHandle slot, std::coroutine_handle<> coro) noexcept
{
    auto reqFn = [this, _scheduler, slot, coro](wgpu::CreatePipelineAsyncStatus status,
                                                wgpu::RenderPipeline pipeline,
                                                wgpu::StringView message)
    {
        if (status != wgpu::CreatePipelineAsyncStatus::Success)
        {
            detail::PrintStatusMessage("CreateRenderPipelineAsync", status, message);
            result = std::unexpected(RhiError::PipelineCreationFailed);
        }
        else [[likely]]
        {
            result = pipeline;
        }

        if (!scheduler)
        {
            coro.resume();
        }
        else
        {
            RhiError error = scheduler->MarkReady(slot, coro);
            if (error != RhiError::Success) [[unlikely]]
            {
                result = std::unexpected(error);
            }
        }
    };

    device.CreateRenderPipelineAsync(
        &descriptor,
        wgpu::CallbackMode::AllowSpontaneous,
        std::move(reqFn));
}

Result<wgpu::RenderPipeline> RenderPipelineAwaitable::await_resume() noexcept
{
    return std::move(result);
}

ComputePipelineAwaitable::ComputePipelineAwaitable(wgpu::Device _device,
                                                   wgpu::ComputePipelineDescriptor _descriptor,
                                                   Scheduler* _scheduler) noexcept
    : device{ _device },
      descriptor{ std::forward<wgpu::ComputePipelineDescriptor>(_descriptor) },
      scheduler{ _scheduler }
{
}

void ComputePipelineAwaitable::Dispatch(Scheduler* _scheduler, SlotHandle slot, std::coroutine_handle<> coro) noexcept
{
    auto reqFn = [this, _scheduler, slot, coro](wgpu::CreatePipelineAsyncStatus status,
                                                wgpu::ComputePipeline pipeline,
                                                wgpu::StringView message)
    {
        if (status != wgpu::CreatePipelineAsyncStatus::Success)
        {
            detail::PrintStatusMessage("CreateComputePiplineAsync", status, message);
            result = std::unexpected(RhiError::PipelineCreationFailed);
        }
        else [[likely]]
        {
            result = pipeline;
        }

        if (scheduler)
        {
            RhiError error = scheduler->MarkReady(slot, coro);
            if (error != RhiError::Success) [[unlikely]]
            {
                result = std::unexpected(error);
            }
        }
        else
        {
            coro.resume();
        }
    };

    device.CreateComputePipelineAsync(
        &descriptor,
        wgpu::CallbackMode::AllowSpontaneous,
        std::move(reqFn));
}

Result<wgpu::ComputePipeline> ComputePipelineAwaitable::await_resume() noexcept
{
    return std::move(result);
}

} // namespace velox