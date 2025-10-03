###DirectX11Interface::VertexShader#############################################################################################

##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::POSITION::DXGI_FORMAT_R32G32B32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::COLOR0::DXGI_FORMAT_R32G32B32A32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::TEXCOORD0::DXGI_FORMAT_R32G32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA

##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::FlashlightConstantBuffer::mvp::float4x4
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::FlashlightConstantBuffer::max_opacity::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::FlashlightConstantBuffer::flashlight_radius::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::FlashlightConstantBuffer::flashlight_center::float2

cbuffer FlashlightConstantBuffer : register(b0)
{
	float4x4 mvp;
	float max_opacity;
	float flashlight_radius;
	float2 flashlight_center;
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
	float max_opacity : PSIZE0;
	float flashlight_radius : PSIZE1;
	float2 flashlight_center : PSIZE2;
};

VS_OUTPUT vsmain(in VS_INPUT In)
{
	VS_OUTPUT Out;
	In.pos.w = 1.0f;
	Out.pos = mul(In.pos, mvp);
	Out.max_opacity = max_opacity;
	Out.flashlight_radius = flashlight_radius;
	Out.flashlight_center = flashlight_center;
	return Out;
};
