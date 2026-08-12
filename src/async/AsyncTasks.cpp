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

void AdapterAwaitable::await_suspend(std::coroutine_handle<> coro_handle)
{
    SlotHandle slotHandle;
    if (scheduler)
    {
        slotHandle = scheduler->Enqueue(coro_handle);
    }

    instance.RequestAdapter(&options,
                            wgpu::CallbackMode::AllowSpontaneous,
                            [this, slotHandle, coro_handle](wgpu::RequestAdapterStatus status,
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
                            });
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

void DeviceAwaitable::await_suspend(std::coroutine_handle<> coro_handle)
{
    SlotHandle slotHandle;
    if (scheduler)
    {
        slotHandle = scheduler->Enqueue(coro_handle);
    }

    adapter.RequestDevice(&descriptor,
                          wgpu::CallbackMode::AllowSpontaneous,
                          [this, slotHandle, coro_handle](
                              wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message)
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
                          });
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

void MapReadAwaitable::await_suspend(std::coroutine_handle<> coro_handle)
{
    SlotHandle slotHandle;
    if (scheduler)
    {
        slotHandle = scheduler->Enqueue(coro_handle);
    }

    buffer.MapAsync(wgpu::MapMode::Read,
                    offset,
                    size,
                    wgpu::CallbackMode::AllowSpontaneous,
                    [this, slotHandle, coro_handle](wgpu::MapAsyncStatus status, wgpu::StringView message)
                    {
                        if (status != wgpu::MapAsyncStatus::Success)
                        {
                            detail::PrintStatusMessage("MapBuffer", status, message);
                            result = std::unexpected(RhiError::AsyncBufferMapFailed);
                        }

                        if (!scheduler)
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
                    });
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

void MapWriteAwaitable::await_suspend(std::coroutine_handle<> coro_handle)
{
    // await suspend registers the callback, meaning we immediately return to the caller
    // webgpu will call this captured callback, at which point it resumes the coroutine

    // this is a deferred resume coroutine, register with context
    SlotHandle slotHandle;
    if (scheduler)
    {
        slotHandle = scheduler->Enqueue(coro_handle);
    }

    buffer.MapAsync(wgpu::MapMode::Write,
                    offset,
                    size,
                    wgpu::CallbackMode::AllowSpontaneous,
                    [this, slotHandle, coro_handle](wgpu::MapAsyncStatus status, wgpu::StringView message)
                    {
                        if (status != wgpu::MapAsyncStatus::Success)
                        {
                            detail::PrintStatusMessage("MapBuffer", status, message);
                            result = std::unexpected(RhiError::AsyncBufferMapFailed);
                        }

                        if (!scheduler)
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
                    });
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

void RenderPipelineAwaitable::await_suspend(std::coroutine_handle<> coro_handle)
{
    SlotHandle slotHandle;
    if (scheduler)
    {
        slotHandle = scheduler->Enqueue(coro_handle);
    }

    device.CreateRenderPipelineAsync(
        &descriptor,
        wgpu::CallbackMode::AllowSpontaneous,
        [this, slotHandle, coro_handle](
            wgpu::CreatePipelineAsyncStatus status, wgpu::RenderPipeline pipeline, wgpu::StringView message)
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

            if (scheduler)
            {
                RhiError error = scheduler->MarkReady(slotHandle, coro_handle);
                if (error != RhiError::Success) [[unlikely]]
                {
                    result = std::unexpected(error);
                }
            }
            else
            {
                coro_handle.resume();
            }
        });
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

void ComputePipelineAwaitable::await_suspend(std::coroutine_handle<> coro_handle)
{
    SlotHandle slotHandle;
    if (scheduler)
    {
        slotHandle = scheduler->Enqueue(coro_handle);
    }

    device.CreateComputePipelineAsync(
        &descriptor,
        wgpu::CallbackMode::AllowSpontaneous,
        [this, slotHandle, coro_handle](
            wgpu::CreatePipelineAsyncStatus status, wgpu::ComputePipeline pipeline, wgpu::StringView message)
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
                RhiError error = scheduler->MarkReady(slotHandle, coro_handle);
                if (error != RhiError::Success) [[unlikely]]
                {
                    result = std::unexpected(error);
                }
            }
            else
            {
                coro_handle.resume();
            }
        });
}

Result<wgpu::ComputePipeline> ComputePipelineAwaitable::await_resume() noexcept
{
    return std::move(result);
}

} // namespace velox