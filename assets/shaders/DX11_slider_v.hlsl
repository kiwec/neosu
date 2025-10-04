###DirectX11Interface::VertexShader#############################################################################################

##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::POSITION::DXGI_FORMAT_R32G32B32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::COLOR0::DXGI_FORMAT_R32G32B32A32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::TEXCOORD0::DXGI_FORMAT_R32G32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA

##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::mvp::float4x4
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::style::int
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::bodyAlphaMultiplier::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::bodyColorSaturation::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::borderSizeMultiplier::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::borderFeather::float
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::colBorder::float3
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::colBody::float3
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::padding1::float

cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	float4x4 mvp;
	int style;
	float bodyAlphaMultiplier;
	float bodyColorSaturation;
	float borderSizeMultiplier;
	float borderFeather;
	float3 colBorder;
	float3 colBody;
	float padding1;
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
	int style	: BLENDINDICES0;
	float bodyAlphaMultiplier	: PSIZE0;
	float bodyColorSaturation	: PSIZE1;
	float borderSizeMultiplier	: PSIZE2;
	float borderFeather			: PSIZE3;
	float4 colBorder	: COLOR1;
	float4 colBody		: COLOR2;
};

VS_OUTPUT vsmain(in VS_INPUT In)
{
	VS_OUTPUT Out;
	In.pos.w = 1.0f;
	Out.pos = mul(In.pos, mvp);
	Out.tex = In.tex;
	Out.style = style;
	Out.bodyAlphaMultiplier = bodyAlphaMultiplier;
	Out.bodyColorSaturation = bodyColorSaturation;
	Out.borderSizeMultiplier = borderSizeMultiplier;
	Out.borderFeather = borderFeather;
	Out.colBorder.rgb = colBorder;
	Out.colBorder.a = 1.0f;
	Out.colBody.rgb = colBody;
	Out.colBody.a = 1.0f;
	return Out;
};
