#pragma once
#ifndef VELOX_APPLICATION_HPP
#define VELOX_APPLICATION_HPP
#include <cstdint>
#include "VeloxErrors.hpp"

struct GLFWwindow;

namespace wgpu
{
class TextureView;
}

namespace velox
{
class Context;

/**
 * @brief Lifecylcle interface any instance of a Velox application must implement. Takes a context reference
 * in each function to separate those two concerns and make ownership less annoying. Effectively serves as a
 * stub for the key main loop functions we'll need to run a demo/showcase.
 */
class Application
{
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Context* contextPtr;
public:

    Application(Context* context) noexcept;
    virtual ~Application();

    // todo: same as Context, make this a less brittle FSM by packaging it in a variant w std::visit
    enum class LifecyclePhase : uint8_t
    {
        Invalid = 0,
        Initialization,
        Execution,
        Shutdown
    };

    // Attaches a Context to this application: call after initial instance creation, but before
    // Context runs bootstrapping (that must be handled asynchronously in event loop)
    LifecyclePhase CurrentPhase() const noexcept;

    // non-virtual setup function: used to get Context ready for further work
    Result<LifecyclePhase> RunSetup() noexcept;

    // Override to perform required work *after* Context is ready (resource creation, etc)
    virtual Result<LifecyclePhase> OnSetup() noexcept;

    // Called whenever the surface needs to be (re)configured, including
    // once up front with the initial window/canvas size.
    virtual void OnResize(uint32_t width, uint32_t height);

    // Called once per frame, before OnRender. dt is in seconds.
    virtual void OnUpdate(double dt);

    // Called once per frame. `backbuffer` is the surface's current texture
    // view, already acquired - just record and submit your command buffer.
    virtual void OnRender(wgpu::TextureView& backbuffer) = 0;

    virtual void RequestShutdown() noexcept;

    // Called once, before the Context is torn down.
    virtual void OnShutdown();
protected:
    Context& GetContext() noexcept;
private:
    LifecyclePhase phase{ LifecyclePhase::Invalid };
};

// Anything after we call RunApplication() in main will not be called on web, for now
void ApplicationMainLoop(Context& context, Application& app);

} // namespace velox

#endif // !VELOX_APPLICATION_HPP
