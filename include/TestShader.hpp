#pragma once

constexpr const char* const shaderSource = R"(
struct VertexOutput
{
    @builtin(position) Position : vec4<f32>,
    @location(0) VertexColor : vec4<f32>
};

@vertex
fn VsMain(@builtin(vertex_index) inVertexIndex : u32) -> VertexOutput
{
    var pos = array<vec2<f32>, 3>
    (
        vec2<f32>(0.0, 0.5),
        vec2<f32>(-0.5, -0.5),
        vec2<f32>(0.5, -0.5)
    );

    var color = array<vec3<f32>, 3>
    (
        vec3<f32>(1.0, 0.0, 0.0),
        vec3<f32>(0.0, 1.0, 0.0),
        vec3<f32>(0.0, 0.0, 1.0)
    );

    var output : VertexOutput;
    output.Position = vec4<f32>(pos[inVertexIndex], 0.0, 1.0);
    output.VertexColor = vec4<f32>(color[inVertexIndex], 1.0);
    return output;
}

const kExposure : f32 = 4.0;
const kTonemap : u32 = 2u; // 0 = raw/hard-clip, 1 = Reinhard, 2 = crude ACES-ish

fn tonemap(c : vec3<f32>, mode : u32) -> vec3<f32>
{
    var result : vec3<f32>;
    if (mode == 0u)
    {
        result = c;
    }
    else if (mode == 1u)
    {
        result = c / (c + vec3<f32>(1.0));
    }
    else if (mode == 2u)
    {
        result = clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), vec3<f32>(0.0), vec3<f32>(1.0));
    }
    return result;
}

@fragment
fn FsMain(in: VertexOutput) -> @location(0) vec4<f32>
{
    var c = in.VertexColor.rgb * kExposure;

    c = tonemap(c, kTonemap);

    return vec4<f32>(c, in.VertexColor.a);
}
)";
