#include "core/Future.hpp"
#include "core/Scheduler.hpp"

namespace velox
{

AdapterFuture RequestAdapter(wgpu::Instance _instance,
                              wgpu::RequestAdapterOptions _options,
                              Scheduler* _scheduler)
{
    auto result = co_await AdapterAwaitable{ _instance,
                                            std::forward<wgpu::RequestAdapterOptions>(_options),
                                            _scheduler };
    co_return result;
}

DeviceFuture RequestDevice(wgpu::Adapter _adapter,
                           wgpu::DeviceDescriptor _descriptor,
                           Scheduler* _scheduler)
{
    auto result = co_await DeviceAwaitable{ _adapter,
                                            std::forward<wgpu::DeviceDescriptor>(_descriptor),
                                            _scheduler };
    co_return result;
}

RenderPipelineFuture RequestRenderPipeline(wgpu::Device _device,
                                           wgpu::RenderPipelineDescriptor _descr,
                                           Scheduler* _scheduler)
{
    auto result = co_await RenderPipelineAwaitable{  _device,
                                                     std::forward<wgpu::RenderPipelineDescriptor>(_descr),
                                                     _scheduler };
    co_return result;
}

ComputePipelineFuture RequestComputePipeline(wgpu::Device _device,
                                             wgpu::ComputePipelineDescriptor _descriptor,
                                             Scheduler* _scheduler)
{
    auto result = co_await ComputePipelineAwaitable{ _device,
                                                     std::forward<wgpu::ComputePipelineDescriptor>(_descriptor),
                                                     _scheduler };
    co_return result;
}

} // namespace velox