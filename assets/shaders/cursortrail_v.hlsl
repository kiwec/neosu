###DirectX11Interface::VertexShader#############################################################################################

##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::POSITION::DXGI_FORMAT_R32G32B32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::COLOR0::DXGI_FORMAT_R32G32B32A32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::TEXCOORD0::DXGI_FORMAT_R32G32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA

##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::mvp::float4x4
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::time::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::padding1::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::padding2::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::padding3::float

cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	float4x4 mvp;
	float time;
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
	float2 tex	: TEXCOORD0;
	float time  : PSIZE0;
	float vtx_alpha : PSIZE1;
};

VS_OUTPUT vsmain(in VS_INPUT In)
{
	VS_OUTPUT Out;
	In.pos.w = 1.0f;
	Out.pos = mul(In.pos, mvp);
	Out.tex = In.tex;
	Out.time = time;
	Out.vtx_alpha = In.pos.z;
	return Out;
};
