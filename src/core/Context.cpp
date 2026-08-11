#include "core/Context.hpp"
#include "core/AsyncTasks.hpp"
#include "core/Scheduler.hpp"
#include <algorithm>
#include <magic_enum/magic_enum.hpp>
#include <print>
#include <stdexcept>
#include <webgpu/webgpu_glfw.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
// sorry for this doing inline but I want to get it done and over with
#if !defined(__EMSCRIPTEN__) && defined(_WIN32)
#undef APIENTRY // glfw also defines this, need to undef to compile
#define WIN32_LEAN_AND_MEAN
#include <dawn/native/DawnNative.h>
#include <windows.h>
// :(
std::string GetSystemDirectory()
{
    char buffer[MAX_PATH];
    UINT result = GetSystemDirectoryA(buffer, MAX_PATH);
    if (result == 0 || result > MAX_PATH)
    {
        std::println(stderr, "Failed to get system directory: {}", GetLastError());
        return std::string{};
    }
    // path has to end with //, as dawn won't append it automatically lol
    std::string strResult(buffer);
    if (strResult.back() != '\\')
    {
        strResult += '\\';
    }
    return strResult;
}
#endif

namespace
{

// todo-ship: we should sink these somewhere more portable, and which could actually give us debug info
// in live clients maybe?
[[noreturn]] void LogUncapturedError([[maybe_unused]] const wgpu::Device&,
                                     wgpu::ErrorType type,
                                     wgpu::StringView message)
{
    std::println(stderr,
                 "[wgpu] Uncaptured error, exiting | Error Type \"{}\" | Message: {}",
                 magic_enum::enum_name(type),
                 std::string_view(message.data, message.length));
    std::exit(1);
}

// todo-ship: this needs to signal through stop_token system so inflight coroutines can be cancelled
void LogDeviceLost([[maybe_unused]] const wgpu::Device&,
                   wgpu::DeviceLostReason reason,
                   wgpu::StringView message)
{
    // note that this is also called for routine destruction, so messages from here don't always
    // mean something went wrong
    std::println(stderr,
                 "[wgpu] Device lost | Reason: \"{}\" | Message: {}",
                 magic_enum::enum_name(reason),
                 std::string_view(message.data, message.length));
}

} // namespace

namespace velox
{

Context::Context(ContextCreateInfo _createInfo)
    : createInfo{ std::forward<ContextCreateInfo>(_createInfo) },
      phase{ BootstrapPhase::Invalid },
      scheduler{ std::make_unique<Scheduler>() }
{
    // only these two objects aren't dependent on async work
    instance = ValidOrExit(requestInstance());
    nativeWindow = ValidOrExit(createNativeWindow());
    phase = BootstrapPhase::InstanceCreated;
}

Context::~Context()
{
    glfwDestroyWindow(nativeWindow);
    glfwTerminate();
}

Context::BootstrapPhase Context::GetCurrentPhase() const noexcept
{
    return phase;
}

Result<Context::BootstrapPhase> Context::RunBootstrap()
{
    if (phase == Context::BootstrapPhase::Complete)
    {
        return phase;
    }

    assert(phase > BootstrapPhase::Invalid && "RunBootstrap called before Instance creation completed");
    switch (phase)
    {
    case BootstrapPhase::Invalid:
        return std::unexpected(RhiError::BootstrapInInvalidState);
    case BootstrapPhase::InstanceCreated:
        [[fallthrough]];
    case BootstrapPhase::RequestingAdapter:
    {
        Result<Context::BootstrapPhase> result = bootstrapAdapter();
        if (result.has_value())
        {
            phase = *result;
        }
        return result;
    }
    case BootstrapPhase::RequestingDevice:
    {
        Result<Context::BootstrapPhase> result = bootstrapDevice();
        if (result.has_value())
        {
            phase = *result;
        }
        return result;
    }
    case BootstrapPhase::Complete:
        return BootstrapPhase::Complete;
    default:
        return std::unexpected(RhiError::BootstrapInInvalidState);
    }

    return phase;
}

ResizeStatus Context::Resize(uint32_t width, uint32_t height)
{
    if (width == surfaceConfig.width && height == surfaceConfig.height)
    {
        return ResizeStatus::Unchanged;
    }
    else if (width == 0 || height == 0)
    {
        return ResizeStatus::Minimized;
    }
    else
    {
        surfaceConfig.width = width;
        surfaceConfig.height = height;
        surface.Configure(&surfaceConfig);
        return ResizeStatus::Resized;
    }
}

// todo-ship: Result<wgpu::TextureView> instead of returning an empty view on failure, so we can bubble up errors
wgpu::TextureView Context::AcquireNextFrame()
{
    wgpu::SurfaceTexture surfaceTexture{};
    surface.GetCurrentTexture(&surfaceTexture);

    switch (surfaceTexture.status)
    {
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
        return surfaceTexture.texture.CreateView();
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
        std::println(stderr, "[velox][context] Next frame acquisition returned SuccessSuboptimal");
        return surfaceTexture.texture.CreateView();
    default:
        return wgpu::TextureView{};
    }
}

void Context::Present()
{
#ifndef __EMSCRIPTEN__
    // Emscripten presents implicitly at the end of each browser frame;
    // calling Present() there is a validation error.
    surface.Present();
#endif
}

wgpu::Instance& Context::GetInstance() noexcept
{
    return instance;
}

wgpu::Adapter& Context::GetAdapter() noexcept
{
    return adapter;
}

wgpu::Device& Context::GetDevice() noexcept
{
    return device;
}

wgpu::Queue& Context::GetQueue() noexcept
{
    return queue;
}

wgpu::Surface& Context::GetSurface() noexcept
{
    return surface;
}

wgpu::TextureFormat Context::GetSurfaceFormat() const noexcept
{
    return surfaceConfig.format;
}

bool Context::HasFeature(wgpu::FeatureName feature) const noexcept
{
    return device.HasFeature(feature);
}

#ifndef __EMSCRIPTEN__
GLFWwindow* Context::GetNativeWindow() const noexcept
{
    return nativeWindow;
}
#endif

Scheduler* Context::GetScheduler() noexcept
{
    return scheduler.get();
}

Result<wgpu::Instance> Context::requestInstance()
{
    wgpu::InstanceDescriptor instanceDesc{};
#if !defined(__EMSCRIPTEN__) && defined(_WIN32)
    // as mentioned above, we want to make sure Dawn can find vulkan-1.dll on windows
    // (only when we're compiling for Native on Win32, of course!)
    std::string sys32Path = GetSystemDirectory();
    const char* searchPaths[] = { sys32Path.c_str() };

    dawn::native::DawnInstanceDescriptor dawnDescriptor{};
    dawnDescriptor.additionalRuntimeSearchPathsCount = std::size(searchPaths);
    dawnDescriptor.additionalRuntimeSearchPaths = searchPaths;
    instanceDesc.nextInChain = &dawnDescriptor;
#else
    instanceDesc.nextInChain = nullptr;
#endif
    instance = wgpu::CreateInstance(&instanceDesc);
    if (!instance)
    {
        return std::unexpected(RhiError::InstanceRequestFailed);
    }
    std::println(stderr, "[velox][context] Instance creation successful.");
    return std::move(instance);
}

Result<GLFWwindow*> Context::createNativeWindow()
{
    if (!glfwInit())
    {
        return std::unexpected(RhiError::GLFWInitFailed);
    }

    // works like Vulkan - no context, just platform window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(createInfo.InitialWidth,
                                          createInfo.InitialHeight,
                                          createInfo.ApplicationName.data(),
                                          nullptr,
                                          nullptr);
    if (!window)
    {
        return std::unexpected(RhiError::GLFWWindowCreationFailed);
    }

    // incase we need this for callbacks later
    glfwSetWindowUserPointer(window, this);

    return window;
}

wgpu::RequestAdapterOptions Context::getAdapterOptions() const
{
    wgpu::RequestAdapterOptions options{};
#ifndef __EMSCRIPTEN__
    // can't use vulkan on desktop, as nvidia drivers have severe bugs with f16
    // our entire framework is built on f16 LMAO
    // https://issues.chromium.org/issues/42251215
    options.backendType = wgpu::BackendType::D3D12;
#endif
    options.featureLevel = createInfo.FeatureLevel;
    options.powerPreference = createInfo.PowerPreference;
    return options;
}

Result<Context::BootstrapPhase> Context::bootstrapAdapter()
{
    if (!adapterFuture && !adapter)
    {
        adapterFuture = RequestAdapter(instance, std::move(getAdapterOptions()), scheduler.get());
        phase = BootstrapPhase::RequestingAdapter;
    }

    if (auto adapterResult = adapterFuture.TryGet())
    {
        if (!adapterResult->has_value()) [[unlikely]]
        {
            return std::unexpected(adapterResult->error());
        }
        else
        {
            adapter = std::move(adapterResult->value());
            std::println(stderr, "[velox][context] Adapter creation successful.");
            return BootstrapPhase::RequestingDevice;
        }
    }
    else
    {
        return BootstrapPhase::RequestingAdapter;
    }

    return BootstrapPhase::Invalid;
}

wgpu::DeviceDescriptor Context::getDeviceDescriptor() const
{
    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.label = createInfo.ApplicationName;
    deviceDesc.SetUncapturedErrorCallback(LogUncapturedError);
    deviceDesc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous, LogDeviceLost);
    if (!createInfo.RequiredFeatures.empty())
    {
        deviceDesc.requiredFeatureCount = createInfo.RequiredFeatures.size();
        deviceDesc.requiredFeatures = createInfo.RequiredFeatures.data();
    }
    else
    {
        deviceDesc.requiredFeatureCount = 0;
        deviceDesc.requiredFeatures = nullptr;
    }
    return deviceDesc;
}

Result<Context::BootstrapPhase> Context::bootstrapDevice()
{
    if (!deviceFuture && !device)
    {
        deviceFuture = RequestDevice(adapter, std::move(getDeviceDescriptor()), scheduler.get());
    }

    if (auto deviceResult = deviceFuture.TryGet())
    {
        if (!deviceResult->has_value()) [[unlikely]]
        {
            return std::unexpected(deviceResult->error());
        }
        else
        {
            device = std::move(deviceResult->value());
            // now run surface setup while we're here, since it uses all 3 prev objects
            Result<wgpu::Surface> surfaceResult = createSurface();
            if (surfaceResult.has_value()) [[likely]]
            {
                surface = surfaceResult.value();
                configureSurface();
                queue = device.GetQueue();
                std::println(stderr, "[velox][context] Device creation successful.");
                return BootstrapPhase::Complete;
            }
            else
            {
                return std::unexpected(RhiError::SurfaceCreationFailed);
            }
        }
    }
    else
    {
        return BootstrapPhase::RequestingDevice;
    }
}

Result<wgpu::Surface> Context::createSurface()
{
    wgpu::Surface createdSurface = wgpu::glfw::CreateSurfaceForWindow(instance, nativeWindow);
    if (!createdSurface)
    {
        return std::unexpected(RhiError::SurfaceCreationFailed);
    }
    return createdSurface;
}

void Context::configureSurface()
{
    wgpu::SurfaceCapabilities capabilities{};
    surface.GetCapabilities(adapter, &capabilities);

    auto format_match = [this](wgpu::TextureFormat format)
    {
        return format == createInfo.PreferredSurfaceFormat;
    };

    // print suported formats
    std::string supportedFormats;
    for (size_t i = 0; i < capabilities.formatCount; ++i)
    {
        supportedFormats += std::format(" {} |", magic_enum::enum_name(capabilities.formats[i]));
    }
    std::println(stderr, "[velox][context] Surface supported formats:{}", supportedFormats);

    auto format_iter =
        std::find_if(capabilities.formats, capabilities.formats + capabilities.formatCount, format_match);
    wgpu::TextureFormat surfaceFormat{};
    if (format_iter != capabilities.formats + capabilities.formatCount)
    {
        surfaceFormat = *format_iter;
        std::println(stderr,
                     "[velox][context] Using preferred surface format {}",
                     magic_enum::enum_name(surfaceFormat));
    }
    else
    {
        // if we can't find our preferred format, just pick the first one the surface supports
        surfaceFormat = capabilities.formats[0];
        std::println(stderr,
                     "[velox][context] Preferred surface format {} not supported by surface, using "
                     "{} instead",
                     magic_enum::enum_name(createInfo.PreferredSurfaceFormat),
                     magic_enum::enum_name(surfaceFormat));
    }

    surfaceConfig.device = device;
    surfaceConfig.format = surfaceFormat;
    surfaceConfig.usage = wgpu::TextureUsage::RenderAttachment;
    surfaceConfig.width = createInfo.InitialWidth;
    surfaceConfig.height = createInfo.InitialHeight;
    // todo-ship: assess later how/if we may want to change alpha mode
    surfaceConfig.alphaMode = wgpu::CompositeAlphaMode::Auto;
    surfaceConfig.presentMode = createInfo.PreferredPresentationMode;
#ifdef __EMSCRIPTEN__
    // this is only valid on emscripten: on native, we don't need to tell the compositor
    // or surface what colorspace we're in or to respect our tonemapping
    wgpu::SurfaceColorManagement colorManagement{};
    colorManagement.colorSpace = createInfo.PreferredColorSpace;
    colorManagement.toneMappingMode = createInfo.PreferredToneMappingMode;
    surfaceConfig.nextInChain = &colorManagement;
#endif
    surface.Configure(&surfaceConfig);
}

} // namespace velox
