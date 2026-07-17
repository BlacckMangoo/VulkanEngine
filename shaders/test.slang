struct VsInput {
    float3 position ; 
    float2 texCoord;
};

struct VsOutput {

    float4 pos : SV_Position;
    float2 fragTexCoord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;
Sampler2D texture;

[shader("vertex")]
VsOutput vertMain(VsInput input) {
    VsOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 1.0))));
    output.fragTexCoord = input.texCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VsOutput vertIn) : SV_Target
{
    return texture.Sample(vertIn.fragTexCoord);
}