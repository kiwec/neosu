cbuffer VertexUniforms : register(b0, space1) {
    float4x4 mvp;
};

struct VSInput {
    float3 inPos : TEXCOORD0;
    float4 inCol : TEXCOORD1;
    float2 inTex : TEXCOORD2;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 fragColor : TEXCOORD0;
    float2 fragTexcoord : TEXCOORD1;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mvp, float4(input.inPos, 1.0));
    output.fragColor = input.inCol;
    output.fragTexcoord = input.inTex;
    return output;
}
