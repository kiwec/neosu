//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX implementation of RenderTarget / render to texture
//
// $NoKeywords: $drt
//===============================================================================//

#pragma once
#ifndef DIRECTX11RENDERTARGET_H
#define DIRECTX11RENDERTARGET_H

#include "RenderTarget.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "d3d11.h"

class DirectX11Interface;

class DirectX11RenderTarget final : public RenderTarget
{
public:
	DirectX11RenderTarget(int x, int y, int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType = Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);
	~DirectX11RenderTarget() override {destroy();}

	void draw(int x, int y) override;
	void draw(int x, int y, int width, int height) override;
	void drawRect(int x, int y, int width, int height) override;

	void enable() override;
	void disable() override;

	void bind(unsigned int textureUnit = 0) override;
	void unbind() override;

	// ILLEGAL:
	void setDirectX11InterfaceHack(DirectX11Interface *dxi) {m_interfaceOverrideHack = dxi;}
	inline ID3D11Texture2D *getRenderTexture() const {return m_renderTexture;}

private:
	void init() override;
	void initAsync() override;
	void destroy() override;

	ID3D11Texture2D *m_renderTexture;
	ID3D11Texture2D *m_depthStencilTexture;
	ID3D11RenderTargetView *m_renderTargetView;
	ID3D11DepthStencilView *m_depthStencilView;
	ID3D11ShaderResourceView *m_shaderResourceView;

	ID3D11RenderTargetView *m_prevRenderTargetView;
	ID3D11DepthStencilView *m_prevDepthStencilView;

	unsigned int m_iTextureUnitBackup;
	ID3D11ShaderResourceView *m_prevShaderResourceView;

	DirectX11Interface *m_interfaceOverrideHack;
};

#endif

#endif
