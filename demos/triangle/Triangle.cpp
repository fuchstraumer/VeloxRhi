#include "Context.hpp"
#include "Application.hpp"
#include <cassert>
#include <string_view>
#include <span>
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "InputManager.hpp"
#include "TestShader.hpp"
#include <magic_enum/magic_enum.hpp>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

using namespace velox;

namespace
{
// Flip to inspect specific event or gesture types — recompile to change filter
constexpr bool kLogCursorPosition = false; // very noisy on mouse move
constexpr bool kLogPointerPress   = true;
constexpr bool kLogPointerMove    = false; // very noisy
constexpr bool kLogScroll         = true;
constexpr bool kLogKeys           = false;
constexpr bool kLogTextInput      = false;
constexpr bool kLogFocus          = true;
constexpr bool kLogPan            = true; // noisy during drag
constexpr bool kLogPinch          = true;
constexpr bool kLogRotation       = true;
constexpr bool kLogSwipe          = true;
constexpr bool kLogTap            = true;

std::string FormatModifiers(uint8_t mods)
{
    if (mods == 0) { return "None"; }
    std::string Result;
    auto Append = [&](std::string_view name)
    {
        if (!Result.empty()) { Result += '+'; }
        Result += name;
    };
    if (mods & 1u) { Append("Shift"); }
    if (mods & 2u) { Append("Ctrl"); }
    if (mods & 4u) { Append("Alt"); }
    if (mods & 8u) { Append("Meta"); }
    return Result;
}

void PrintReceivedEvent(const ReceivedEvent& ev)
{
    std::visit(
        [&](const auto& e)
        {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, CursorPositionEvent>)
            {
                if constexpr (kLogCursorPosition)
                {
                    std::println("[{:.1f}ms] Cursor ({:.1f}, {:.1f})", ev.Time, e.X, e.Y);
                }
            }
            else if constexpr (std::is_same_v<T, PointerEvent>)
            {
                if (e.State == PointerState::Moved)
                {
                    if constexpr (kLogPointerMove)
                    {
                        std::println("[{:.1f}ms] Pointer {}#{} Moved @ ({:.1f}, {:.1f})",
                            ev.Time,
                            magic_enum::enum_name(e.Type),
                            e.ID,
                            e.X,
                            e.Y);
                    }
                }
                else
                {
                    if constexpr (kLogPointerPress)
                    {
                        std::println("[{:.1f}ms] Pointer {}#{} {} @ ({:.1f}, {:.1f})",
                            ev.Time,
                            magic_enum::enum_name(e.Type),
                            e.ID,
                            magic_enum::enum_name(e.State),
                            e.X,
                            e.Y);
                    }
                }
            }
            else if constexpr (std::is_same_v<T, ScrollEvent>)
            {
                if constexpr (kLogScroll)
                {
                    std::println("[{:.1f}ms] Scroll ({:+.2f}, {:+.2f})", ev.Time, e.DeltaX, e.DeltaY);
                }
            }
            else if constexpr (std::is_same_v<T, KeyEvent>)
            {
                if constexpr (kLogKeys)
                {
                    std::println("[{:.1f}ms] Key {:3d} [{}] {}",
                        ev.Time,
                        e.KeyCode,
                        FormatModifiers(e.Modifiers),
                        magic_enum::enum_name(e.State));
                }
            }
            else if constexpr (std::is_same_v<T, TextInputEvent>)
            {
                if constexpr (kLogTextInput)
                {
                    std::println("[{:.1f}ms] TextInput U+{:04X}", ev.Time, e.Codepoint);
                }
            }
            else if constexpr (std::is_same_v<T, FocusEvent>)
            {
                if constexpr (kLogFocus)
                {
                    std::println("[{:.1f}ms] Focus {}", ev.Time, e.Focused ? "gained" : "lost");
                }
            }
        },
        ev.EventData);
}

void PrintGestureEvent(const GestureEvent& gest)
{
    std::visit(
        [](const auto& g)
        {
            using T = std::decay_t<decltype(g)>;
            if constexpr (std::is_same_v<T, PanGestureEvent>)
            {
                if constexpr (kLogPan)
                {
                    std::println("  Pan Δ({:+.1f}, {:+.1f})", g.DeltaX, g.DeltaY);
                }
            }
            else if constexpr (std::is_same_v<T, PinchGestureEvent>)
            {
                if constexpr (kLogPinch)
                {
                    std::println("  Pinch {:.4f}x @ ({:.0f}, {:.0f})", g.Scale, g.CenterX, g.CenterY);
                }
            }
            else if constexpr (std::is_same_v<T, RotationGestureEvent>)
            {
                if constexpr (kLogRotation)
                {
                    std::println("  Rotate {:+.5f}rad @ ({:.0f}, {:.0f})",
                        g.AngleDelta,
                        g.CenterX,
                        g.CenterY);
                }
            }
            else if constexpr (std::is_same_v<T, SwipeGestureEvent>)
            {
                if constexpr (kLogSwipe)
                {
                    std::println("  Swipe {}f vel=({:+.3f}, {:+.3f})px/ms @ ({:.0f}, {:.0f})",
                        g.FingerCount,
                        g.VelocityX,
                        g.VelocityY,
                        g.X,
                        g.Y);
                }
            }
            else if constexpr (std::is_same_v<T, TapGestureEvent>)
            {
                if constexpr (kLogTap)
                {
                    std::println("  Tap {}f x{} @ ({:.0f}, {:.0f})",
                        g.FingerCount,
                        g.TapCount,
                        g.X,
                        g.Y);
                }
            }
        },
        gest);
}
} // namespace

static const std::vector<float> vertexData
{
    0.0f, 0.5f, 1.0f, 0.0f, 0.0f,
   -0.5f,-0.5f, 0.0f, 1.0f, 0.0f,
    0.5f,-0.5f, 0.0f, 0.0f, 1.0f
};

static const float vertexPositions[]
{
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.0f,
    -0.55f,-0.5f,
    -0.05f, 0.5f,
    -0.55f, 0.5f
};

static const float vertexColors[]
{
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 1.0f
};

inline constexpr bool k_UseSplitVBOs = true;

class TriangleApplication final : public Application
{
public:

    TriangleApplication(Context* _context) noexcept : Application(_context), inputMgr(this) {}

    Result<LifecyclePhase> OnSetup() noexcept final
    {
        using namespace wgpu;
        if (!shaderModule)
        {
            ShaderSourceWGSL wgslSource{};
            wgslSource.code = shaderSource;
            ShaderModuleDescriptor shaderDesc{};
            shaderDesc.nextInChain = &wgslSource;
            shaderModule = GetContext().GetDevice().CreateShaderModule(&shaderDesc);
            if (!shaderModule)
            {
                return std::unexpected(RhiError::ShaderModuleCreationFailed);
            }
        }

        
        if constexpr (k_UseSplitVBOs)
        {
            if (!vertexBuffer && !vertexColorBuffer)
            {
                wgpu::BufferDescriptor vPosDescr{};
                vPosDescr.label = "VertexPositions";
                vPosDescr.mappedAtCreation = false;
                vPosDescr.size = sizeof(vertexPositions);
                vPosDescr.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
                wgpu::Device dvc = GetContext().GetDevice();
                vertexBuffer = dvc.CreateBuffer(&vPosDescr);
                if (!vertexBuffer)
                {
                    return std::unexpected(RhiError::BufferMapFailed);
                }
                wgpu::BufferDescriptor vColDescr{};
                vColDescr.label = "VertexColors";
                vColDescr.mappedAtCreation = false;
                vColDescr.size = sizeof(vertexColors);
                vColDescr.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
                vertexColorBuffer = dvc.CreateBuffer(&vColDescr);
                if (!vertexColorBuffer)
                {
                    return std::unexpected(RhiError::BufferMapFailed);
                }
                // now copy the data into the buffers
                wgpu::Queue queue = dvc.GetQueue();
                queue.WriteBuffer(vertexBuffer, 0, vertexPositions, sizeof(vertexPositions));
                queue.WriteBuffer(vertexColorBuffer, 0, vertexColors, sizeof(vertexColors));
            }
        }
        else
        {
            if (!vertexBuffer)
            {
                wgpu::BufferDescriptor vboDescr{};
                vboDescr.label = "VBO";
                vboDescr.mappedAtCreation = false;
                vboDescr.size = sizeof(float) * vertexData.size();
                vboDescr.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
                wgpu::Device dvc = GetContext().GetDevice();
                vertexBuffer = dvc.CreateBuffer(&vboDescr);
                if (!vertexBuffer)
                {
                    return std::unexpected(RhiError::BufferMapFailed);
                }
                wgpu::Queue queue = dvc.GetQueue();
                queue.WriteBuffer(vertexBuffer, 0, vertexData.data(), vboDescr.size);
            }
        }

        // undispatched. dispatch future and populate 
        if (!pipelineFuture && !pipeline)
        {
            ColorTargetState colorTarget{};
            colorTarget.format = GetContext().GetSurfaceFormat();
            // because this is taken as a pointer by pipeline, it needs to stay alive at the top
            // level of this function, and we'll jsut set count based on k_UseSplitVBOs
            VertexAttribute vboAttributes[2]{ VertexAttribute{}, VertexAttribute{} };
            VertexBufferLayout vboLayout[2]{ VertexBufferLayout{}, VertexBufferLayout{} };
            if constexpr (k_UseSplitVBOs)
            {
                vboAttributes[0].format = wgpu::VertexFormat::Float32x2;
                vboAttributes[0].offset = 0;
                vboAttributes[0].shaderLocation = 0;
                
                vboAttributes[1].format = wgpu::VertexFormat::Float32x3;
                vboAttributes[1].offset = 0;
                vboAttributes[1].shaderLocation = 1;
                
                vboLayout[0].arrayStride = sizeof(float) * 2;
                vboLayout[0].stepMode = wgpu::VertexStepMode::Vertex;
                vboLayout[0].attributeCount = 1;
                vboLayout[0].attributes = &vboAttributes[0];

                vboLayout[1].arrayStride = sizeof(float) * 3;
                vboLayout[1].stepMode = wgpu::VertexStepMode::Vertex;
                vboLayout[1].attributeCount = 1;
                vboLayout[1].attributes = &vboAttributes[1];

                vertexCount = static_cast<uint32_t>(std::size(vertexPositions)) / 2; // two floats per vertex
            }
            else
            {
                vboAttributes[0].format = wgpu::VertexFormat::Float32x2;
                vboAttributes[0].offset = 0;
                vboAttributes[0].shaderLocation = 0;
                
                vboAttributes[1].format = wgpu::VertexFormat::Float32x3;
                vboAttributes[1].offset = sizeof(float) * 2;
                vboAttributes[1].shaderLocation = 1;

                vboLayout[0].arrayStride = sizeof(float) * 5;
                vertexCount = static_cast<uint32_t>(vertexData.size() * sizeof(float)) / static_cast<uint32_t>(vboLayout[0].arrayStride);
                vboLayout[0].stepMode = wgpu::VertexStepMode::Vertex;
                vboLayout[0].attributeCount = std::size(vboAttributes);
                vboLayout[0].attributes = vboAttributes;
            }
            
            VertexState vertexState{};
            vertexState.module = shaderModule;
            if constexpr (k_UseSplitVBOs)
            {
                vertexState.bufferCount = 2;
            }
            else
            {
                vertexState.bufferCount = 1;
            }

            vertexState.buffers = &vboLayout[0];
            vertexState.entryPoint = "VsMain";

            PrimitiveState primitiveState{};
            primitiveState.topology = PrimitiveTopology::TriangleList;
            primitiveState.cullMode = CullMode::None;
            primitiveState.frontFace = FrontFace::CCW;

            FragmentState fragmentState{};
            fragmentState.module = shaderModule;
            fragmentState.targetCount = 1;
            fragmentState.targets = &colorTarget;
            fragmentState.entryPoint = "FsMain";

            RenderPipelineDescriptor pipelineDesc{};
            pipelineDesc.label = "TestTrianglePipeline";
            pipelineDesc.nextInChain = nullptr;
            pipelineDesc.vertex = vertexState;
            pipelineDesc.fragment = &fragmentState;
            pipelineDesc.primitive = primitiveState;

            pipelineFuture = RequestRenderPipeline(GetContext().GetDevice(), pipelineDesc, GetContext().GetScheduler());
            // returning this indicates all is as expected, but phase hasn't evolved
            return LifecyclePhase::Initialization;
        }

        if (pipelineFuture)
        {
            auto resultOpt = pipelineFuture.TryGet();
            if (!resultOpt)
            {
                return LifecyclePhase::Initialization;
            }
            
            Result<wgpu::RenderPipeline> result = resultOpt.value();
            if (result.has_value())
            {
                pipeline = result.value();
                return LifecyclePhase::Execution;
            }
            else
            {
                return std::unexpected(result.error());
            }
        }

        return LifecyclePhase::Initialization;
    }

    void OnUpdate() final
    {
        if (!inputInitialized)
        {
#ifndef __EMSCRIPTEN__
            inputMgr.Initialize(GetContext().GetNativeWindow());
#else
            inputMgr.Initialize(nullptr);
#endif
            inputInitialized = true;
        }
        inputMgr.PrepareFrame();
        for (const ReceivedEvent& Ev : inputMgr.GetEventsForFrame())
        {
            PrintReceivedEvent(Ev);
        }
        for (const GestureEvent& Gest : inputMgr.GetGesturesForFrame())
        {
            PrintGestureEvent(Gest);
        }
    }

    void OnRender(wgpu::TextureView& backbuffer) final
    {
        Context& context = GetContext();

        wgpu::RenderPassColorAttachment colorAttachment{};
        colorAttachment.view = backbuffer;
        colorAttachment.loadOp = wgpu::LoadOp::Clear;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = { 113.0 / 255.0, 153.0 / 255.0, 1.0, 1.0 };

        wgpu::RenderPassDescriptor renderPassDesc{};
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachment;

        wgpu::CommandEncoder encoder = context.GetDevice().CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);
        renderPass.SetPipeline(pipeline);
        if constexpr (k_UseSplitVBOs)
        {
            renderPass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
            renderPass.SetVertexBuffer(1, vertexColorBuffer, 0, vertexColorBuffer.GetSize());
        }
        else
        {
            renderPass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
        }
        renderPass.Draw(vertexCount, 1, 0, 0);
        renderPass.End();

        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        auto queue = context.GetQueue(); assert(queue);
        context.GetQueue().Submit(1, &commandBuffer);
    }

private:
    uint32_t vertexCount;
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer vertexColorBuffer;
    wgpu::Buffer indexBuffer;
    wgpu::ShaderModule shaderModule;
    wgpu::RenderPipeline pipeline;
    RenderPipelineFuture pipelineFuture;
    MapWriteFuture mapWriteFuture;
    InputManager inputMgr;
    bool inputInitialized{ false };
};


int main()
{
    std::println(stderr, "Application started.");
    ContextCreateInfo createInfo{};
    createInfo.ApplicationName = "Velox Test App";
    wgpu::FeatureName requestedFeatures[] = 
    {
        wgpu::FeatureName::ShaderF16,
        wgpu::FeatureName::Subgroups
    };
    std::span<wgpu::FeatureName> requestedFeaturesSpan(requestedFeatures);
    createInfo.InitialWidth = 1280;
    createInfo.InitialHeight = 720;
    createInfo.RequiredFeatures = requestedFeaturesSpan;
    createInfo.FeatureLevel = wgpu::FeatureLevel::Compatibility;
    createInfo.PowerPreference = wgpu::PowerPreference::HighPerformance;
    createInfo.PreferredSurfaceFormat = wgpu::TextureFormat::RGBA16Float;
    createInfo.PreferredColorSpace = wgpu::PredefinedColorSpace::DisplayP3;
    createInfo.PreferredToneMappingMode = wgpu::ToneMappingMode::Extended;
    
    Context context(createInfo);
    TriangleApplication app(&context);
    ApplicationMainLoop(context, app);
    std::println(stderr, "Application shutdown");
    return 0;
}

