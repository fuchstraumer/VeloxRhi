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

#ifndef NDEBUG
#include <print>
#endif

namespace velox
{

double PlatformNowSeconds() noexcept;

Application::Application(Context* _context) noexcept
    : contextPtr{ _context }
{
}

Application::~Application()
{
}

void Application::TickClock(double now) noexcept
{
    clock.DeltaTime = now - clock.CurrentTime;
    clock.CurrentTime = now;
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
    
    if (contextPtr->GetCurrentPhase() != Context::BootstrapPhase::Complete)
    {
        Result<Context::BootstrapPhase> bootstrap = contextPtr->RunBootstrap();
        if (!bootstrap)
        {
            return std::unexpected(bootstrap.error());
        }
        return LifecyclePhase::Initialization;
    }
    else
    {
        Result<LifecyclePhase> setup = OnSetup();
        if (setup.has_value())
        {
            phase = *setup;
        }
        else
        {
            return std::unexpected(setup.error());
        }
        return setup;
    }
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

void Application::OnUpdate()
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
#ifdef __EMSCRIPTEN__
    // default implementation does nothing
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
    // absolute first step: get time from platform
    double now = PlatformNowSeconds();
    app.TickClock(now);

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
#ifndef NDEBUG
            std::println(stderr, "App setup failed. Exiting.");
#endif
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

        app.OnUpdate();
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

double PlatformNowSeconds() noexcept
{
    return emscripten_get_now() / 1000.0;
}

struct MainLoopState
{
    Context* context;
    Application* app;
};

void EmMainLoopStep(void* user_data)
{
    MainLoopState* mainLoopState = reinterpret_cast<MainLoopState*>(user_data);
    MainLoopStep(*mainLoopState->context, *mainLoopState->app);
    if (mainLoopState->app->CurrentPhase() == Application::LifecyclePhase::Shutdown)
    {
        emscripten_cancel_main_loop();
    }
}

void ApplicationMainLoop(Context& context, Application& app)
{
    static MainLoopState state{ &context, &app };
    emscripten_set_main_loop_arg(EmMainLoopStep, &state, 0, true);
}

#else // ndef __EMSCRIPTEN__

double PlatformNowSeconds() noexcept
{
    return glfwGetTime();
}

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