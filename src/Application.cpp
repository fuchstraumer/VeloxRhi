#include "Application.hpp"
#include "Context.hpp"
#include "Scheduler.hpp"
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace velox
{

struct MainLoopState
{
    Context* context{ nullptr };
    Application* application{ nullptr };
};

Application::Application(Context* _context) noexcept
    : contextPtr{ _context }
{
}

Application::~Application()
{
}

Application::LifecyclePhase Application::CurrentPhase() const noexcept
{
    return phase;
}

Result<Application::LifecyclePhase> Application::RunSetup() noexcept
{
    if (phase == LifecyclePhase::Invalid)
    {
        phase = LifecyclePhase::Initialization;
    }

    Result<Context::BootstrapPhase> bootstrap = contextPtr->RunBootstrap();
    if (!bootstrap)
    {
        return std::unexpected(bootstrap.error());
    }

    if (bootstrap.value() != Context::BootstrapPhase::Complete)
    {
        // still running init, until bootstrap finishes
        return LifecyclePhase::Initialization;
    }

    Result<LifecyclePhase> setup = OnSetup();
    if (setup)
    {
        phase = *setup;
    }

    return setup;
}

Result<Application::LifecyclePhase> Application::OnSetup() noexcept
{
    // by default, just return ready
    return LifecyclePhase::Execution;
}

void Application::OnResize(uint32_t width, uint32_t height)
{
    // default implementation does nothing
}

void Application::OnUpdate(double dt)
{
    // default implementation does nothing
}

void Application::RequestShutdown() noexcept
{
    OnShutdown();
    phase = LifecyclePhase::Shutdown;
}

void Application::OnShutdown()
{
#ifdef __EMSCRIPTEN
    
#else
    // default implementation does nothing
#endif
}

Context& Application::GetContext() noexcept
{
    assert(contextPtr);
    return *contextPtr;
}

// common main loop stepping function.
// todo: this is where the variant will run visit. i think
void MainLoopStep(Context& context, Application& app) noexcept
{
    context.GetScheduler()->Tick();
    context.GetInstance().ProcessEvents();

    switch (app.CurrentPhase())
    {
    case Application::LifecyclePhase::Invalid:
        [[fallthrough]];
    case Application::LifecyclePhase::Initialization:
    {
        Result<Application::LifecyclePhase> setupResult = app.RunSetup();
        if (!setupResult)
        {
            std::println(stderr, "App setup failed. Exiting.");
            app.RequestShutdown();
        }
        break;
    }
    case Application::LifecyclePhase::Execution:
    {
        static auto last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        last = now;

        app.OnUpdate(dt);
        wgpu::TextureView backbuffer = context.AcquireNextFrame();
        app.OnRender(backbuffer);
        context.Present();
        break;
    }
    case Application::LifecyclePhase::Shutdown:
        break;
    }
}

#ifdef __EMSCRIPTEN__

void ApplicationMainLoop(Application& app)
{
    Application::LifecyclePhase currentPhase = app.CurrentPhase();
}

#else // ndef __EMSCRIPTEN__

void ApplicationMainLoop(Context& context, Application& app)
{
    while (app.CurrentPhase() != Application::LifecyclePhase::Shutdown)
    {
        glfwPollEvents();
        if (glfwWindowShouldClose(context.GetNativeWindow()))
        {
            app.RequestShutdown();
            continue;
        }
        MainLoopStep(context, app);
    }
}

#endif // __EMSCRIPTEN__

} // namespace velox