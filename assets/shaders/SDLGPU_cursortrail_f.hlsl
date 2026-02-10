Texture2D<float4> tex0 : register(t0, space2);
SamplerState samp0 : register(s0, space2);

struct PSInput {
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float vtx_alpha : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0 {
    float4 color = tex0.Sample(samp0, input.tex_coord);
    color.a *= input.vtx_alpha;
    return color;
}
