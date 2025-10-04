###DirectX11Interface::VertexShader#############################################################################################

##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::POSITION::DXGI_FORMAT_R32G32B32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::COLOR0::DXGI_FORMAT_R32G32B32A32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::TEXCOORD0::DXGI_FORMAT_R32G32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA

##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::SmoothClipConstantBuffer::mvp::float4x4
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::SmoothClipConstantBuffer::rect_min::float2
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::SmoothClipConstantBuffer::rect_max::float2
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::SmoothClipConstantBuffer::edge_softness::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::SmoothClipConstantBuffer::padding1::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::SmoothClipConstantBuffer::padding2::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::SmoothClipConstantBuffer::padding3::float

cbuffer SmoothClipConstantBuffer : register(b0)
{
	float4x4 mvp;
	float2 rect_min;
	float2 rect_max;
	float edge_softness;
	float padding1;
	float padding2;
	float padding3;
};

struct VS_INPUT
{
	float4 pos	: POSITION;
	float4 col	: COLOR0;
	float2 tex	: TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 pos	: SV_Position;
	float4 col	: COLOR0;
	float2 tex	: TEXCOORD0;
	float2 rect_min : TEXCOORD1;
	float2 rect_max : TEXCOORD2;
	float edge_softness : PSIZE0;
};

VS_OUTPUT vsmain(in VS_INPUT In)
{
	VS_OUTPUT Out;
	In.pos.w = 1.0f;
	Out.pos = mul(In.pos, mvp);
	Out.col = In.col;
	Out.tex = In.tex;
	Out.rect_min = rect_min;
	Out.rect_max = rect_max;
	Out.edge_softness = edge_softness;
	return Out;
};
