Texture2D<float4> tex0 : register(t0, space2);
SamplerState samp0 : register(s0, space2);

cbuffer FragParams : register(b0, space3) {
    float2 rect_min;
    float2 rect_max;
    float edge_softness;
    float _pad0; float _pad1; float _pad2;
    float4 col;
};

struct PSInput {
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float4 vtx_col : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0 {
    float2 dist_to_edges = min(input.position.xy - rect_min, rect_max - input.position.xy);
    float min_dist = min(dist_to_edges.x, dist_to_edges.y);

    float alpha = smoothstep(-edge_softness, 0.0, min_dist);
    float4 texColor = tex0.Sample(samp0, input.tex_coord);
    return float4(texColor.rgb * col.rgb, texColor.a * col.a * alpha);
}
