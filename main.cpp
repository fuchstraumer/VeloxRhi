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
#include "TestShader.hpp"
#include "HdrTestPatternShader.hpp"

using namespace velox;

class TriangleApplication final : public Application
{
public:

    TriangleApplication(Context* _context) noexcept : Application(_context) {}

    Result<LifecyclePhase> OnSetup() noexcept final
    {
        using namespace wgpu;
        if (!shaderModule)
        {
            ShaderSourceWGSL wgslSource{};
            wgslSource.code = hdrTestPatternShaderSource;
            ShaderModuleDescriptor shaderDesc{};
            shaderDesc.nextInChain = &wgslSource;
            shaderModule = GetContext().GetDevice().CreateShaderModule(&shaderDesc);
            if (!shaderModule)
            {
                return std::unexpected(RhiError::ShaderModuleCreationFailed);
            }
        }

        // undispatched. dispatch future and populate 
        if (!pipelineFuture && !pipeline)
        {
            ColorTargetState colorTarget{};
            colorTarget.format = GetContext().GetSurfaceFormat();

            VertexState vertexState{};
            vertexState.module = shaderModule;
            vertexState.bufferCount = 0;
            vertexState.buffers = nullptr;
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
        renderPass.Draw(3);
        renderPass.End();

        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        auto queue = context.GetQueue(); assert(queue);
        context.GetQueue().Submit(1, &commandBuffer);
    }

private:
    wgpu::ShaderModule shaderModule;
    wgpu::RenderPipeline pipeline;
    RenderPipelineFuture pipelineFuture;
};

int main()
{
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
    
#ifndef __EMSCRIPTEN__
    ApplicationMainLoop(context, app);
#else
    MainLoopState mainLoopState{ &context, pipeline };
    emscripten_set_main_loop_arg(EmMainLoopArg, &mainLoopState, 0, true);
#endif

    
    return 0;
}

