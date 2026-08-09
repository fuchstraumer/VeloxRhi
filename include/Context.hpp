#pragma once

#ifndef VELOX_WEB_GPU_CONTEXT_HPP
#define VELOX_WEB_GPU_CONTEXT_HPP
#include "VeloxErrors.hpp"
#include "Future.hpp"
#include <cstdint>
#include <span>
#include <memory>
#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

namespace velox
{

struct Scheduler;

enum class ResizeStatus : uint8_t
{
    Unchanged = 0,
    Resized = 1,
    Minimized = 2
};

struct ContextCreateInfo
{
    uint32_t InitialWidth{ 800u };
    uint32_t InitialHeight{ 600u };
    std::string_view ApplicationName{ "WebGPU App" };
    // This is for *device* features only
    std::span<wgpu::FeatureName> RequiredFeatures;
    wgpu::FeatureLevel FeatureLevel{ wgpu::FeatureLevel::Core };
    wgpu::PowerPreference PowerPreference{ wgpu::PowerPreference::HighPerformance };

    // following are swapchain parameters: named "preferred" because we will try to use them,
    // but won't crash or fail if the surface doesn't support them (support varies a LOT ime)
    // Undefined => pick the first format the surface reports as supported
    wgpu::TextureFormat PreferredSurfaceFormat{ wgpu::TextureFormat::Undefined };
    // todo: how does HDR support actually work? we'll need a tonemapper too....
    wgpu::PredefinedColorSpace PreferredColorSpace{ wgpu::PredefinedColorSpace::SRGB };
    wgpu::ToneMappingMode PreferredToneMappingMode{ wgpu::ToneMappingMode::Standard };
    wgpu::PresentMode PreferredPresentationMode{ wgpu::PresentMode::Fifo };
};

/**
 * @brief Owns the classical Instance/Adapter/Device trio, but also has full
 * ownership of the surface and queue. Also handles a few bookkeeping and
 * common callbacks like device losses, surface reconfig, etc. This class
 * mostly exists to centralize setup.
 *
 * Based on my RhiSystem implementation from DiamondDogs repo. Does not
 * allow access to queues, resources, or command buffers: purely holds
 * the baseline objects we need to get a WebGPU context online.
 */
class Context
{
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

public:
    enum class BootstrapPhase : uint8_t
    {
        Invalid = 0,
        InstanceCreated,
        RequestingAdapter,
        RequestingDevice,
        Complete
    };

    /* This *only* sets the createInfo and creates the instance */
    Context(ContextCreateInfo _createInfo);
    ~Context();

    ResizeStatus Resize(uint32_t width, uint32_t height);
    wgpu::TextureView AcquireNextFrame();
    void Present();

    wgpu::Instance& GetInstance() noexcept;
    wgpu::Adapter& GetAdapter() noexcept;
    wgpu::Device& GetDevice() noexcept;
    wgpu::Queue& GetQueue() noexcept;
    wgpu::Surface& GetSurface() noexcept;
    wgpu::TextureFormat GetSurfaceFormat() const noexcept;

    bool HasFeature(wgpu::FeatureName feature) const noexcept;
    GLFWwindow* GetNativeWindow() const noexcept;

    Scheduler* GetScheduler() noexcept;

private:

    ContextCreateInfo createInfo;
    BootstrapPhase phase{ BootstrapPhase::Invalid };
    std::unique_ptr<Scheduler> scheduler{ nullptr };

    Result<wgpu::Instance> requestInstance();
    Result<GLFWwindow*> createNativeWindow();
    wgpu::RequestAdapterOptions getAdapterOptions() const;
    wgpu::DeviceDescriptor getDeviceDescriptor() const;
    Result<wgpu::Surface> createSurface();

    void configureSurface();

    wgpu::Instance instance;
    AdapterFuture adapterFuture;
    wgpu::Adapter adapter;
    DeviceFuture deviceFuture;
    wgpu::Device device;
    wgpu::Queue queue;
    GLFWwindow* nativeWindow{ nullptr };

    wgpu::Surface surface;
    // we store the surface config to make reconfiguring not need the whole create info
    wgpu::SurfaceConfiguration surfaceConfig{};
};

} // namespace velox

#endif // !VELOX_WEB_GPU_CONTEXT_HPP
