###DirectX11Interface::VertexShader#############################################################################################

##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::POSITION::DXGI_FORMAT_R32G32B32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::COLOR0::DXGI_FORMAT_R32G32B32A32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA
##D3D11_INPUT_ELEMENT_DESC::VS_INPUT::TEXCOORD0::DXGI_FORMAT_R32G32_FLOAT::D3D11_INPUT_PER_VERTEX_DATA

##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::mvp::float4x4
//##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::world::float4x4
//##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::view::float4x4
//##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::projection::float4x4
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::col::float4
##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::misc::float4

cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	float4x4 mvp;		// world matrix for object
//	matrix world;		// world matrix for object
//	matrix view;		// view matrix
//	matrix projection;	// projection matrix
	float4 col;			// global color
	float4 misc;		// misc params. [0] = textured or flat, [1] = color inversion
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
	float4 misc	: COLOR1;
	float2 tex	: TEXCOORD0;
};

VS_OUTPUT vsmain(in VS_INPUT In)
{
	VS_OUTPUT Out;
	In.pos.w = 1.0f;
	Out.pos = mul(In.pos, mvp);
//	Out.pos.z = (Out.pos.z + Out.pos.w)/2.0f; // TODO: not sure if necessary to compensate clip space range here, no artifacts so far (OpenGL NDC z from -1 to 1, DirectX NDC z from 0 to 1)
	Out.col = In.col;
	Out.misc = misc;
	Out.tex = In.tex;
	return Out;
}
