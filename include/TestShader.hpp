#pragma once

constexpr const char* const shaderSource = R"(
struct VertexOutput
{
    @builtin(position) Position : vec4<f32>,
    @location(0) VertexColor : vec4<f32>
};

@vertex
fn VsMain(@location(0) inVertexPos : vec2f, @location(1) inVertexColor : vec3f) -> VertexOutput
{
    var output : VertexOutput;
    output.Position = vec4<f32>(inVertexPos, 0.0, 1.0);
    output.VertexColor = vec4<f32>(inVertexColor, 1.0);
    return output;
}

@fragment
fn FsMain(in: VertexOutput) -> @location(0) vec4<f32>
{
    return vec4<f32>(in.VertexColor);
}

)";
