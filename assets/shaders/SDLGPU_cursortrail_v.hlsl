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
    float2 tex_coord : TEXCOORD0;
    float vtx_alpha : TEXCOORD1;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(mvp, float4(input.inPos.xy, 0.0, 1.0));
    output.tex_coord = input.inTex;
    output.vtx_alpha = input.inPos.z;
    return output;
}
