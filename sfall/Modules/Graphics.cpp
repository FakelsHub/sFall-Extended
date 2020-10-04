/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2012  The sfall team
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"
#include "..\InputFuncs.h"
#include "..\Version.h"
#include "LoadGameHook.h"
#include "ScriptShaders.h"

#include "Graphics.h"

namespace sfall
{

#define UNUSEDFUNCTION { return DDERR_GENERIC; }
#define SAFERELEASE(a) { if (a) { a->Release(); a = nullptr; } }

typedef HRESULT (__stdcall *DDrawCreateProc)(void*, IDirectDraw**, void*);
///typedef IDirect3D9* (__stdcall *D3DCreateProc)(UINT version);

static IDirectDrawSurface* primaryDDSurface = nullptr; // aka _GNW95_DDPrimarySurface

static DWORD ResWidth;
static DWORD ResHeight;

DWORD Graphics::GPUBlt;
DWORD Graphics::mode;

bool Graphics::PlayAviMovie = false;
bool Graphics::AviMovieWidthFit = false;

static DWORD yoffset;
///static DWORD xoffset;

static bool DeviceLost = false;
static bool mainTexLock = false;
static char textureFilter; // 1 - auto, 2 - force

static DDSURFACEDESC surfaceDesc;
static DDSURFACEDESC mveDesc;
static D3DSURFACE_DESC movieDesc;

static DWORD palette[256];
//static bool paletteInit = false;

static DWORD gWidth;
static DWORD gHeight;

static long moveWindowKey[2];

static bool windowInit = false;
static DWORD windowLeft = 0;
static DWORD windowTop = 0;
static HWND  window;
static DWORD windowStyle = WS_CAPTION | WS_BORDER | WS_MINIMIZEBOX;

static int windowData;

static DWORD ShaderVersion;

IDirect3D9* d3d9 = 0;
IDirect3DDevice9* d3d9Device = 0;

static IDirect3DTexture9* mainTex = 0;
static IDirect3DTexture9* sTex1 = 0;
static IDirect3DTexture9* sTex2 = 0;

static IDirect3DSurface9* sSurf1 = 0;
static IDirect3DSurface9* sSurf2 = 0;
static IDirect3DSurface9* backBuffer = 0;

static IDirect3DVertexBuffer9* vBuffer;
static IDirect3DVertexBuffer9* vBuffer2;
static IDirect3DVertexBuffer9* movieBuffer;

static IDirect3DTexture9* gpuPalette;
static IDirect3DTexture9* movieTex;

static ID3DXEffect* gpuBltEffect;
static const char* gpuEffect =
	"texture image;"
	"texture palette;"
	"texture head;"
	"texture highlight;"
	"sampler s0 = sampler_state { texture=<image>; };"
	"sampler s1 = sampler_state { texture=<palette>; minFilter=none; magFilter=none; addressU=clamp; addressV=clamp; };"
	"sampler s2 = sampler_state { texture=<head>; minFilter=linear; magFilter=linear; addressU=clamp; addressV=clamp; };"
	"sampler s3 = sampler_state { texture=<highlight>; minFilter=linear; magFilter=linear; addressU=clamp; addressV=clamp; };"
	"float2 size;"
	"float2 corner;"
	"float2 sizehl;"
	"float2 cornerhl;"
	"int showhl;"

	// shader for displaying head textures
	"float4 P1( in float2 Tex : TEXCOORD0 ) : COLOR0 {"
	  "float backdrop = tex2D(s0, Tex).a;"
	  "float3 result;"
	  "if (abs(backdrop - 1.0) < 0.001) {" // (48.0 / 255.0) // 48 - key index color
	    "result = tex2D(s2, saturate((Tex - corner) / size));"
	  "} else {"
	    "result = tex1D(s1, backdrop);"
	    "result = float3(result.b, result.g, result.r);"
	  "}"
	// blend highlights
	"if (showhl) {"
		"float4 h = tex2D(s3, saturate((Tex - cornerhl) / sizehl));"
		"result = saturate(result + h);" // saturate(result * (1 - h.a) * h.rgb * h.a)"
	"}"
	  "return float4(result.r, result.g, result.b, 1);"
	"}"

	"technique T1"
	"{"
	  "pass p1 { PixelShader = compile ps_2_0 P1(); }"
	"}"

	"float4 P0( in float2 Tex : TEXCOORD0 ) : COLOR0 {"
	  "float3 result = tex1D(s1, tex2D(s0, Tex).a);"    // get color in palette
	  "return float4(result.b, result.g, result.r, 1);" // swap R <> B
	"}"

	"technique T0"
	"{"
	  "pass p0 { PixelShader = compile ps_2_0 P0(); }"
	"}";

static D3DXHANDLE gpuBltMainTex;
static D3DXHANDLE gpuBltPalette;
static D3DXHANDLE gpuBltHead;
static D3DXHANDLE gpuBltHeadSize;
static D3DXHANDLE gpuBltHeadCorner;
static D3DXHANDLE gpuBltHighlight;
static D3DXHANDLE gpuBltHighlightSize;
static D3DXHANDLE gpuBltHighlightCorner;
static D3DXHANDLE gpuBltShowHighlight;

static float rcpres[2];

#define _VERTEXFORMAT D3DFVF_XYZRHW|D3DFVF_TEX1

struct VertexFormat {
	float x, y, z, w, u, v;
};

static VertexFormat ShaderVertices[] = {
	// x      y    z rhw u  v
	{-0.5,  -0.5,  0, 1, 0, 0}, // 0 - left top
	{-0.5,  479.5, 0, 1, 0, 1}, // 1 - left bottom
	{639.5, -0.5,  0, 1, 1, 0}, // 2 - right top
	{639.5, 479.5, 0, 1, 1, 1}  // 3 - right bottom
};

HWND GetFalloutWindowInfo(RECT* rect) {
	if (rect) {
		rect->left = windowLeft;
		rect->top = windowTop;
		rect->right = gWidth;
		rect->bottom = gHeight;
	}
	return window;
}

long Graphics::GetGameWidthRes() {
	return (fo::var::scr_size.offx - fo::var::scr_size.x) + 1;
}

long Graphics::GetGameHeightRes() {
	return (fo::var::scr_size.offy - fo::var::scr_size.y) + 1;
}

int __stdcall GetShaderVersion() {
	return ShaderVersion;
}

static void WindowInit() {
	windowInit = true;
	rcpres[0] = 1.0f / (float)Graphics::GetGameWidthRes();
	rcpres[1] = 1.0f / (float)Graphics::GetGameHeightRes();
	ScriptShaders::LoadGlobalShader();
}

// pixel size for the current game resolution
const float* Graphics::rcpresGet() {
	return rcpres;
}

static void GetDisplayMode(D3DDISPLAYMODE &ddm) {
	ZeroMemory(&ddm, sizeof(ddm));
	d3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &ddm);
	dlog_f(" Display mode format ID: %d\n", DL_INIT, ddm.Format);
}

static void ResetDevice(bool createNew) {
	D3DPRESENT_PARAMETERS params;
	ZeroMemory(&params, sizeof(params));

	D3DDISPLAYMODE dispMode;
	GetDisplayMode(dispMode);

	params.BackBufferCount = 1;
	params.BackBufferFormat = dispMode.Format; // (Graphics::mode != 4) ? D3DFMT_UNKNOWN : D3DFMT_X8R8G8B8;
	params.BackBufferWidth = gWidth;
	params.BackBufferHeight = gHeight;
	params.EnableAutoDepthStencil = false;
	//params.MultiSampleQuality = 0;
	//params.MultiSampleType = D3DMULTISAMPLE_NONE;
	params.Windowed = (Graphics::mode != 4);
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.hDeviceWindow = window;
	params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	if (!params.Windowed) params.FullScreen_RefreshRateInHz = dispMode.RefreshRate;

	static bool software = false;
	if (createNew) {
		dlog("Creating D3D9 Device...", DL_MAIN);
		if (FAILED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_PUREDEVICE | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &params, &d3d9Device))) {
			MessageBoxA(window, "Failed to create hardware vertex processing.\nWill be used by software vertex processing.",
			                    "D3D9 Device", MB_TASKMODAL | MB_ICONWARNING);
			software = true;
			d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &params, &d3d9Device);
		}

		D3DCAPS9 caps;
		d3d9Device->GetDeviceCaps(&caps);
		ShaderVersion = ((caps.PixelShaderVersion & 0x0000FF00) >> 8) * 10 + (caps.PixelShaderVersion & 0xFF);

		// Use: 0 - only CPU, 1 - force GPU, 2 - Auto Mode (GPU or switch to CPU)
		if (Graphics::GPUBlt == 2 && ShaderVersion < 20) Graphics::GPUBlt = 0;

		if (Graphics::GPUBlt) {
			D3DXCreateEffect(d3d9Device, gpuEffect, strlen(gpuEffect), 0, 0, 0, 0, &gpuBltEffect, 0);
			gpuBltMainTex = gpuBltEffect->GetParameterByName(0, "image");
			gpuBltPalette = gpuBltEffect->GetParameterByName(0, "palette");
			// for head textures
			gpuBltHead = gpuBltEffect->GetParameterByName(0, "head");
			gpuBltHeadSize = gpuBltEffect->GetParameterByName(0, "size");
			gpuBltHeadCorner = gpuBltEffect->GetParameterByName(0, "corner");
			gpuBltHighlight = gpuBltEffect->GetParameterByName(0, "highlight");
			gpuBltHighlightSize = gpuBltEffect->GetParameterByName(0, "sizehl");
			gpuBltHighlightCorner = gpuBltEffect->GetParameterByName(0, "cornerhl");
			gpuBltShowHighlight = gpuBltEffect->GetParameterByName(0, "showhl");

			Graphics::SetDefaultTechnique();
		}
	} else {
		dlog("Reseting D3D9 Device...", DL_MAIN);
		d3d9Device->Reset(&params);
		if (gpuBltEffect) gpuBltEffect->OnResetDevice();
		ScriptShaders::OnResetDevice();
		mainTexLock = false;
	}

	if (d3d9Device->CreateTexture(ResWidth, ResHeight, 1, D3DUSAGE_DYNAMIC, Graphics::GPUBlt ? D3DFMT_A8 : D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &mainTex, 0) != D3D_OK) {
		d3d9Device->CreateTexture(ResWidth, ResHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &mainTex, 0);
		Graphics::GPUBlt = 0;
		MessageBoxA(window, "Video card unsupported the D3DFMT_A8 texture format.\nNow CPU is used to convert the palette.",
		                    "Texture format error", MB_TASKMODAL | MB_ICONWARNING);
	}

	d3d9Device->CreateTexture(ResWidth, ResHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &sTex1, 0);
	d3d9Device->CreateTexture(ResWidth, ResHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &sTex2, 0);
	sTex1->GetSurfaceLevel(0, &sSurf1);
	sTex2->GetSurfaceLevel(0, &sSurf2);

	if (Graphics::GPUBlt) {
		d3d9Device->CreateTexture(256, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &gpuPalette, 0);
		gpuBltEffect->SetTexture(gpuBltMainTex, mainTex);
		gpuBltEffect->SetTexture(gpuBltPalette, gpuPalette);
	}

	d3d9Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);

	ShaderVertices[1].y = ResHeight - 0.5f;
	ShaderVertices[2].x = ResWidth - 0.5f;
	ShaderVertices[3].y = ResHeight - 0.5f;
	ShaderVertices[3].x = ResWidth - 0.5f;

	d3d9Device->CreateVertexBuffer(4 * sizeof(VertexFormat), D3DUSAGE_WRITEONLY | (software ? D3DUSAGE_SOFTWAREPROCESSING : 0), _VERTEXFORMAT, D3DPOOL_DEFAULT, &vBuffer, 0);
	void* vertexPointer;
	vBuffer->Lock(0, 0, &vertexPointer, 0);
	CopyMemory(vertexPointer, ShaderVertices, sizeof(ShaderVertices));
	vBuffer->Unlock();

	VertexFormat shaderVertices[4] = {
		ShaderVertices[0],
		ShaderVertices[1],
		ShaderVertices[2],
		ShaderVertices[3]
	};

	shaderVertices[1].y = (float)gHeight - 0.5f;
	shaderVertices[2].x = (float)gWidth - 0.5f;
	shaderVertices[3].y = (float)gHeight - 0.5f;
	shaderVertices[3].x = (float)gWidth - 0.5f;

	d3d9Device->CreateVertexBuffer(4 * sizeof(VertexFormat), D3DUSAGE_WRITEONLY | (software ? D3DUSAGE_SOFTWAREPROCESSING : 0), _VERTEXFORMAT, D3DPOOL_DEFAULT, &vBuffer2, 0);
	vBuffer2->Lock(0, 0, &vertexPointer, 0);
	CopyMemory(vertexPointer, shaderVertices, sizeof(shaderVertices));
	vBuffer2->Unlock();

	d3d9Device->CreateVertexBuffer(4 * sizeof(VertexFormat), D3DUSAGE_WRITEONLY | (software ? D3DUSAGE_SOFTWAREPROCESSING : 0), _VERTEXFORMAT, D3DPOOL_DEFAULT, &movieBuffer, 0);

	d3d9Device->SetFVF(_VERTEXFORMAT);
	d3d9Device->SetTexture(0, mainTex);
	d3d9Device->SetStreamSource(0, vBuffer, 0, sizeof(VertexFormat));

	//d3d9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, false); // default false
	//d3d9Device->SetRenderState(D3DRS_ALPHATESTENABLE, false);  // default false
	d3d9Device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	d3d9Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	d3d9Device->SetRenderState(D3DRS_LIGHTING, false);

	if (textureFilter) {
		d3d9Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		d3d9Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	}
	///d3d9Device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 0, 255), 1.0f, 0); // for debbuging
	dlogr(" Done", DL_MAIN);
}

static void Present() {
	if ((moveWindowKey[0] != 0 && KeyDown(moveWindowKey[0])) ||
	    (moveWindowKey[1] != 0 && KeyDown(moveWindowKey[1])))
	{
		int winX, winY;
		GetMouse(&winX, &winY);
		windowLeft += winX;
		windowTop += winY;

		RECT r, r2;
		r.left = windowLeft;
		r.right = windowLeft + gWidth;
		r.top = windowTop;
		r.bottom = windowTop + gHeight;
		AdjustWindowRect(&r, WS_CAPTION | WS_BORDER, false);

		r.right -= (r.left - windowLeft);
		r.left = windowLeft;
		r.bottom -= (r.top - windowTop);
		r.top = windowTop;

		if (GetWindowRect(GetShellWindow(), &r2)) {
			if (r.right > r2.right) {
				DWORD move = r.right - r2.right;
				windowLeft -= move;
				r.right -= move;
			}
			else if (r.left < r2.left) {
				DWORD move = r2.left - r.left;
				windowLeft += move;
				r.right += move;
			}
			if (r.bottom > r2.bottom) {
				DWORD move = r.bottom - r2.bottom;
				windowTop -= move;
				r.bottom -= move;
			}
			else if (r.top < r2.top) {
				DWORD move = r2.top - r.top;
				windowTop += move;
				r.bottom += move;
			}
		}
		MoveWindow(window, windowLeft, windowTop, r.right - windowLeft, r.bottom - windowTop, true);
	}

	if (d3d9Device->Present(0, 0, 0, 0) == D3DERR_DEVICELOST) {
		#ifndef NDEBUG
		dlogr("\nPresent: D3DERR_DEVICELOST", DL_MAIN);
		#endif
		DeviceLost = true;
		d3d9Device->SetTexture(0, 0);
		SAFERELEASE(mainTex)
		SAFERELEASE(backBuffer);
		SAFERELEASE(sSurf1);
		SAFERELEASE(sSurf2);
		SAFERELEASE(sTex1);
		SAFERELEASE(sTex2);
		SAFERELEASE(vBuffer);
		SAFERELEASE(vBuffer2);
		SAFERELEASE(movieBuffer);
		SAFERELEASE(gpuPalette);
		Graphics::ReleaseMovieTexture();
		if (gpuBltEffect) gpuBltEffect->OnLostDevice();
		ScriptShaders::OnLostDevice();
	}
}

static void Refresh() {
	if (DeviceLost) return;

	d3d9Device->BeginScene();
	d3d9Device->SetStreamSource(0, vBuffer, 0, sizeof(VertexFormat));
	d3d9Device->SetRenderTarget(0, sSurf1);

	UINT passes;
	if ((textureFilter || ScriptShaders::Count()) && Graphics::GPUBlt) {
		gpuBltEffect->Begin(&passes, 0);
		gpuBltEffect->BeginPass(0);
		d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		gpuBltEffect->EndPass();
		gpuBltEffect->End();

		d3d9Device->StretchRect(sSurf1, 0, sSurf2, 0, D3DTEXF_NONE); // copy sSurf1 to sSurf2
		d3d9Device->SetTexture(0, sTex2);
	} else {
		d3d9Device->SetTexture(0, mainTex);
	}
	ScriptShaders::Refresh(sSurf1, sSurf2, sTex2);

	d3d9Device->SetStreamSource(0, vBuffer2, 0, sizeof(VertexFormat));
	d3d9Device->SetRenderTarget(0, backBuffer);

	if (!textureFilter && !ScriptShaders::Count() && Graphics::GPUBlt) {
		gpuBltEffect->Begin(&passes, 0);
		gpuBltEffect->BeginPass(0);
	}
	d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
	if (!textureFilter && !ScriptShaders::Count() && Graphics::GPUBlt) {
		gpuBltEffect->EndPass();
		gpuBltEffect->End();
	}

	d3d9Device->EndScene();
	Present();
}

void Graphics::RefreshGraphics() {
	if (!Graphics::PlayAviMovie) Refresh();
}

HRESULT Graphics::CreateMovieTexture(D3DSURFACE_DESC &desc) {
	HRESULT hr = d3d9Device->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_DEFAULT, &movieTex, nullptr);
	if (movieTex) movieTex->GetLevelDesc(0, &movieDesc);
	return hr;
}

void Graphics::ReleaseMovieTexture() {
	SAFERELEASE(movieTex);
}

void Graphics::SetMovieTexture(bool state) {
	dlog("\nSet movie texture.", DL_INIT);
	if (!state) {
		PlayAviMovie = false;
		return;
	} else if (PlayAviMovie) return;

	if (!movieTex) Graphics::CreateMovieTexture(movieDesc);
	D3DSURFACE_DESC &desc = movieDesc;

	float aviAspect = (float)desc.Width / (float)desc.Height;
	float winAspect = (float)gWidth / (float)gHeight;

	VertexFormat shaderVertices[4] = {
		ShaderVertices[0],
		ShaderVertices[1],
		ShaderVertices[2],
		ShaderVertices[3]
	};

	shaderVertices[1].y = (float)gHeight - 0.5f;
	shaderVertices[2].x = (float)gWidth - 0.5f;
	shaderVertices[3].y = (float)gHeight - 0.5f;
	shaderVertices[3].x = (float)gWidth - 0.5f;

	bool subtitleShow = (fo::var::subtitleList != nullptr);

	long offset;
	if (aviAspect > winAspect) {
		// scales height to aspect ratio and placement the movie surface to the centre of the window by the Y-axis
		aviAspect = (float)desc.Width / (float)gWidth;
		desc.Height = (int)(desc.Height / aviAspect);

		offset = (gHeight - desc.Height) / 2;

		shaderVertices[0].y += offset;
		shaderVertices[2].y += offset;
		shaderVertices[1].y -= offset;
		shaderVertices[3].y -= offset;

		subtitleShow = false;
	}
	else if (aviAspect < winAspect) {
		if (Graphics::AviMovieWidthFit || (hrpIsEnabled && *(DWORD*)HRPAddress(0x1006EC10) == 2)) {
			//desc.Width = gWidth; // scales the movie surface to the screen width size
		} else {
			// scales width to aspect ratio and placement the movie surface to the centre of the window by the X-axis
			aviAspect = (float)desc.Height / (float)gHeight;
			desc.Width = (int)(desc.Width / aviAspect);

			offset = (gWidth - desc.Width) / 2;

			shaderVertices[0].x += offset;
			shaderVertices[2].x -= offset;
			shaderVertices[3].x -= offset;
			shaderVertices[1].x += offset;
		}
	}
	if (subtitleShow) { // decrease size the surface to display the subtitle text of the lower layer of surfaces
		int offset = (int)(15.0f * ((float)gHeight / (float)ResHeight));
		shaderVertices[1].y -= offset;
		shaderVertices[3].y -= offset;
	}

	void* vertexPointer;
	movieBuffer->Lock(0, 0, &vertexPointer, 0);
	CopyMemory(vertexPointer, shaderVertices, sizeof(shaderVertices));
	movieBuffer->Unlock();

	PlayAviMovie = true;
}

void Graphics::ShowMovieFrame(IDirect3DTexture9* tex) {
	if (!tex || DeviceLost || !movieTex) return;

	d3d9Device->UpdateTexture(tex, movieTex);
	d3d9Device->SetRenderTarget(0, backBuffer);

	d3d9Device->BeginScene();
	if (!mainTexLock) {
		if (ScriptShaders::Count() && Graphics::GPUBlt) {
			d3d9Device->SetTexture(0, sTex2);
		} else {
			d3d9Device->SetTexture(0, mainTex);
		}
		d3d9Device->SetStreamSource(0, vBuffer2, 0, sizeof(VertexFormat));

		// for show subtitles
		if (Graphics::GPUBlt) {
			UINT passes;
			gpuBltEffect->Begin(&passes, 0);
			gpuBltEffect->BeginPass(0);
		}
		d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		if (Graphics::GPUBlt) {
			gpuBltEffect->EndPass();
			gpuBltEffect->End();
		}
	}
	// for avi movie
	d3d9Device->SetTexture(0, movieTex);
	d3d9Device->SetStreamSource(0, movieBuffer, 0, sizeof(VertexFormat));
	d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

	d3d9Device->EndScene();
	Present();
}

void Graphics::SetHighlightTexture(IDirect3DTexture9* htex, int xPos, int yPos) {
	gpuBltEffect->SetTexture(gpuBltHighlight, htex);

	float size[2];
	size[0] = 388.0f * rcpres[0];
	size[1] = 200.0f * rcpres[1];
	gpuBltEffect->SetFloatArray(gpuBltHighlightSize, size, 2);

	size[0] = (126.0f + xPos) * rcpres[0];
	size[1] = (16.0f + yPos) * rcpres[1];
	gpuBltEffect->SetFloatArray(gpuBltHighlightCorner, size, 2);
}

void Graphics::SetHeadTex(IDirect3DTexture9* tex, int width, int height, int xoff, int yoff, int showHighlight) {
	gpuBltEffect->SetInt(gpuBltShowHighlight, showHighlight);
	gpuBltEffect->SetTexture(gpuBltHead, tex);

	float size[2];
	size[0] = (float)width  * rcpres[0];
	size[1] = (float)height * rcpres[1];
	gpuBltEffect->SetFloatArray(gpuBltHeadSize, size, 2);

	size[0] = (126.0f + xoff + ((388 - width) / 2)) * rcpres[0];
	size[1] = (14.0f + yoff + ((200 - height) / 2)) * rcpres[1];
	gpuBltEffect->SetFloatArray(gpuBltHeadCorner, size, 2);

	SetHeadTechnique();
}

void Graphics::SetHeadTechnique() {
	gpuBltEffect->SetTechnique("T1");
}

void Graphics::SetDefaultTechnique() {
	gpuBltEffect->SetTechnique("T0");
}

class FakeDirectDrawSurface : IDirectDrawSurface {
private:
	ULONG Refs;
	bool isPrimary;
	BYTE* lockTarget = nullptr;

public:
	static bool IsPlayMovie;

	FakeDirectDrawSurface(bool primary) {
		Refs = 1;
		isPrimary = primary;
		if (primary && Graphics::GPUBlt) {
			// use the mainTex texture as a buffer
		} else {
			lockTarget = new BYTE[ResWidth * ResHeight];
		}
	}

	/* IUnknown methods */

	HRESULT __stdcall QueryInterface(REFIID, LPVOID *) { return E_NOINTERFACE; }

	ULONG __stdcall AddRef() { return ++Refs; }

	ULONG __stdcall Release() {
		if (!--Refs) {
			delete[] lockTarget;
			delete this;
			return 0;
		} else return Refs;
	}

	/* IDirectDrawSurface methods */

	HRESULT __stdcall AddAttachedSurface(LPDIRECTDRAWSURFACE) { UNUSEDFUNCTION; }
	HRESULT __stdcall AddOverlayDirtyRect(LPRECT) { UNUSEDFUNCTION; }

	HRESULT __stdcall Blt(LPRECT dsc, LPDIRECTDRAWSURFACE b, LPRECT scr, DWORD d, LPDDBLTFX e) { // called 0x4868DA movie_MVE_ShowFrame_ (used for game movies, only for w/o HRP)
		mveDesc.dwHeight = (dsc->bottom - dsc->top);
		yoffset = (ResHeight - mveDesc.dwHeight) / 2;
		mveDesc.lPitch = (dsc->right - dsc->left);
		///xoffset = (ResWidth - mveDesc.lPitch) / 2;

		//dlog_f("\nBlt: [mveDesc: w:%d h:%d]", DL_INIT, mveDesc.lPitch, mveDesc.dwHeight);

		IsPlayMovie = true;
		//mainTexLock = true;

		BYTE* lockTarget = ((FakeDirectDrawSurface*)b)->lockTarget;

		D3DLOCKED_RECT dRect;
		mainTex->LockRect(0, &dRect, dsc, 0);

		DWORD width = mveDesc.lPitch; // the current size of the width of the mve movie
		int pitch = dRect.Pitch;

		if (Graphics::GPUBlt) {
			fo::func::buf_to_buf(lockTarget, width, mveDesc.dwHeight, width, (BYTE*)dRect.pBits, pitch);
			///char* pBits = (char*)dRect.pBits;
			///for (DWORD y = 0; y < mveDesc.dwHeight; y++) {
			///	CopyMemory(&pBits[y * pitch], &lockTarget[y * width], width);
			///}
		} else {
			pitch /= 4;
			for (DWORD y = 0; y < mveDesc.dwHeight; y++) {
				int yp = y * pitch;
				int yw = y * width;
				for (DWORD x = 0; x < width; x++) {
					((DWORD*)dRect.pBits)[yp + x] = palette[lockTarget[yw + x]];
				}
			}
		}
		mainTex->UnlockRect(0);
		//mainTexLock = false;
		//if (Graphics::PlayAviMovie) return DD_OK; // Blt method is not executed during avi playback because the sfShowFrame_ function is blocked

		if (!DeviceLost) {
			d3d9Device->SetTexture(0, mainTex);

			d3d9Device->BeginScene();
			UINT passes;
			if (textureFilter && Graphics::GPUBlt) { // fixes color palette distortion of movie image when texture filtering and enabled GPUBlt
				d3d9Device->SetStreamSource(0, vBuffer, 0, sizeof(VertexFormat));
				d3d9Device->SetRenderTarget(0, sSurf1);

				gpuBltEffect->Begin(&passes, 0);
				gpuBltEffect->BeginPass(0);
				d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
				gpuBltEffect->EndPass();
				gpuBltEffect->End();

				d3d9Device->SetTexture(0, sTex1);
				d3d9Device->SetRenderTarget(0, backBuffer);
			}
			d3d9Device->SetStreamSource(0, vBuffer2, 0, sizeof(VertexFormat));

			if (!textureFilter && Graphics::GPUBlt) {
				gpuBltEffect->Begin(&passes, 0);
				gpuBltEffect->BeginPass(0);
			}
			d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
			if (!textureFilter && Graphics::GPUBlt) {
				gpuBltEffect->EndPass();
				gpuBltEffect->End();
			}
			d3d9Device->EndScene();
			Present();
		}
		return DD_OK;
	}

	HRESULT __stdcall BltBatch(LPDDBLTBATCH, DWORD, DWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall BltFast(DWORD,DWORD,LPDIRECTDRAWSURFACE, LPRECT,DWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall DeleteAttachedSurface(DWORD,LPDIRECTDRAWSURFACE) { UNUSEDFUNCTION; }
	HRESULT __stdcall EnumAttachedSurfaces(LPVOID,LPDDENUMSURFACESCALLBACK) { UNUSEDFUNCTION; }
	HRESULT __stdcall EnumOverlayZOrders(DWORD,LPVOID,LPDDENUMSURFACESCALLBACK) { UNUSEDFUNCTION; }
	HRESULT __stdcall Flip(LPDIRECTDRAWSURFACE, DWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetAttachedSurface(LPDDSCAPS, LPDIRECTDRAWSURFACE *) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetBltStatus(DWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetCaps(LPDDSCAPS) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetClipper(LPDIRECTDRAWCLIPPER *) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetColorKey(DWORD, LPDDCOLORKEY) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetDC(HDC *) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetFlipStatus(DWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetOverlayPosition(LPLONG, LPLONG) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetPalette(LPDIRECTDRAWPALETTE *) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetPixelFormat(LPDDPIXELFORMAT) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetSurfaceDesc(LPDDSURFACEDESC) { UNUSEDFUNCTION; }
	HRESULT __stdcall Initialize(LPDIRECTDRAW, LPDDSURFACEDESC) { UNUSEDFUNCTION; }
	HRESULT __stdcall IsLost() { UNUSEDFUNCTION; }

	/* Called from:
		0x4CB887 GNW95_ShowRect_      [c=1]
		0x48699D movieShowFrame_      [c=1]
		0x4CBBFA GNW95_zero_vid_mem_  [c=1] (clear surface)
		0x4F5E91/0x4F5EBB sub_4F5E60  [c=0] (from MVE_rmStepMovie_)
		0x486861 movie_MVE_ShowFrame_ [c=1] (capture, never call)
	*/
	HRESULT __stdcall Lock(LPRECT a, LPDDSURFACEDESC b, DWORD c, HANDLE d) {
		if (DeviceLost) return DDERR_SURFACELOST;
		if (isPrimary) {
			if (Graphics::GPUBlt) {
				D3DLOCKED_RECT buf;
				if (mainTex->LockRect(0, &buf, a, 0)) goto surface; // fail lock, use old method

				mainTexLock = true;

				b->lpSurface = buf.pBits;
				b->lPitch = buf.Pitch;
			} else {
		surface:
				*b = surfaceDesc;
				b->lpSurface = lockTarget;
			}
		} else {
			mveDesc.lPitch = *(DWORD*)FO_VAR_lastMovieW;
			mveDesc.dwHeight = *(DWORD*)FO_VAR_lastMovieH;
			//dlog_f("\nLock: [mveDesc: w:%d h:%d]", DL_INIT, mveDesc.lPitch, mveDesc.dwHeight);
			*b = mveDesc;
			b->lpSurface = lockTarget;
		}
		return DD_OK;
	}

	HRESULT __stdcall ReleaseDC(HDC) { UNUSEDFUNCTION; }

	HRESULT __stdcall Restore() { // called 0x4CB907 GNW95_ShowRect_
		if (!d3d9Device) return DD_FALSE;
		if (d3d9Device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
			ResetDevice(false);
			DeviceLost = false;
			fo::RefreshGNW();
		}
		return !DeviceLost;
	}

	HRESULT __stdcall SetClipper(LPDIRECTDRAWCLIPPER) { UNUSEDFUNCTION; }
	HRESULT __stdcall SetColorKey(DWORD, LPDDCOLORKEY) { UNUSEDFUNCTION; }
	HRESULT __stdcall SetOverlayPosition(LONG, LONG) { UNUSEDFUNCTION; }

	HRESULT __stdcall SetPalette(LPDIRECTDRAWPALETTE a) { // called 0x4CB198 GNW95_init_DirectDraw_
		if (DeviceLost || a) return DD_OK;                // prevents executing the function when called from outside of sfall
		//dlog("\nSetPalette", DL_INIT);
		mainTexLock = true;

		D3DLOCKED_RECT dRect;
		mainTex->LockRect(0, &dRect, 0, 0);

		DWORD* pBits = (DWORD*)dRect.pBits;
		int pitch = dRect.Pitch / 4;
		DWORD width = ResWidth;

		for (DWORD y = 0; y < ResHeight; y++) {
			int yp = y * pitch;
			int yw = y * width;
			for (DWORD x = 0; x < width; x++) {
				pBits[yp + x] = palette[lockTarget[yw + x]]; // takes the color index in the palette and copies the color value to the texture surface
			}
		}
		mainTex->UnlockRect(0);
		mainTexLock = false;
		return DD_OK;
	}

	/* Called from:
		0x4CB8F0 GNW95_ShowRect_      (common game, primary)
		0x486A87 movieShowFrame_
		0x4CBC5A GNW95_zero_vid_mem_  (clear surface)
		0x4F5ECC sub_4F5E60           (from MVE_rmStepMovie_)
		0x4868BA movie_MVE_ShowFrame_ (capture never call)
	*/
	HRESULT __stdcall Unlock(LPVOID) {
		//dlog("\nUnlock", DL_INIT);
		if ((DeviceLost && Restore() == DD_FALSE) || !isPrimary) return DD_OK;

		//dlog("\nUnlock -> primary", DL_INIT);

		if (Graphics::GPUBlt == 0) {
			mainTexLock = true;

			D3DLOCKED_RECT dRect;
			mainTex->LockRect(0, &dRect, 0, 0);

			int pitch = dRect.Pitch / 4;
			DWORD* pBits = (DWORD*)dRect.pBits;
			DWORD width = ResWidth;
			// slow copy method
			for (DWORD y = 0; y < ResHeight; y++) {
				int yp = y * pitch;
				int yw = y * width;
				for (DWORD x = 0; x < width; x++) {
					pBits[yp + x] = palette[lockTarget[yw + x]];
				}
			}
		}
		if (mainTexLock) mainTex->UnlockRect(0);
		mainTexLock = false;

		if (!IsPlayMovie && !Graphics::PlayAviMovie) {
			//dlog("\nUnlock -> RefreshGraphics", DL_INIT);
			Refresh();
		}
		IsPlayMovie = false;
		return DD_OK;
	}

	HRESULT __stdcall UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE,LPRECT,DWORD, LPDDOVERLAYFX) { UNUSEDFUNCTION; }
	HRESULT __stdcall UpdateOverlayDisplay(DWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE) { UNUSEDFUNCTION; }
};

bool FakeDirectDrawSurface::IsPlayMovie;

class FakeDirectDrawPalette : IDirectDrawPalette {
private:
	ULONG Refs;
public:
	FakeDirectDrawPalette() {
		Refs = 1;
	}

	/* IUnknown methods */

	HRESULT __stdcall QueryInterface(REFIID, LPVOID*) { return E_NOINTERFACE; }

	ULONG __stdcall AddRef() { return ++Refs; }

	ULONG __stdcall Release() {
		if (!--Refs) {
			delete this;
			return 0;
		} else return Refs;
	}

	/* IDirectDrawPalette methods */

	HRESULT __stdcall GetCaps(LPDWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetEntries(DWORD, DWORD, DWORD, LPPALETTEENTRY) { UNUSEDFUNCTION; }
	HRESULT __stdcall Initialize(LPDIRECTDRAW, DWORD, LPPALETTEENTRY) { UNUSEDFUNCTION; }

	/* Called from:
		0x4CB5C7 GNW95_SetPalette_
		0x4CB36B GNW95_SetPaletteEntries_
	*/
	HRESULT __stdcall SetEntries(DWORD a, DWORD b, DWORD c, LPPALETTEENTRY destPal) { // used to set palette for splash screen, fades, subtitles
		if (!windowInit || c == 0 || b + c > 256) return DDERR_INVALIDPARAMS;

		__movsd(&palette[b], (unsigned long*)destPal, c);

		if (Graphics::GPUBlt && gpuPalette) {
			D3DLOCKED_RECT rect;
			if (!FAILED(gpuPalette->LockRect(0, &rect, 0, D3DLOCK_DISCARD))) {
				CopyMemory(rect.pBits, palette, 256 * 4);
				gpuPalette->UnlockRect(0);
			}
		} else {
			// X8B8G8R8 format
			for (size_t i = b; i < b + c; i++) { // swap color B <> R
				BYTE clr = *(BYTE*)((long)&palette[i]); // B
				*(BYTE*)((long)&palette[i]) = *(BYTE*)((long)&palette[i] + 2); // R
				*(BYTE*)((long)&palette[i] + 2) = clr;
			}
			primaryDDSurface->SetPalette(0); // update
			if (FakeDirectDrawSurface::IsPlayMovie) return DD_OK; // prevents flickering movie at the beginning of playback (w/o HRP & GPUBlt=2)
		}
		if (!Graphics::PlayAviMovie) {
			//dlog("\nSetEntries -> RefreshGraphics", DL_INIT);
			Refresh();
		}
		return DD_OK;
	}
};

class FakeDirectDraw : IDirectDraw
{
private:
	ULONG Refs;
public:
	FakeDirectDraw() {
		Refs = 1;
	}

	/* IUnknown methods */

	HRESULT __stdcall QueryInterface(REFIID, LPVOID*) { return E_NOINTERFACE; }

	ULONG __stdcall AddRef()  { return ++Refs; }

	ULONG __stdcall Release() { // called from game on exit
		if (!--Refs) {
			ScriptShaders::Release();

			SAFERELEASE(backBuffer);
			SAFERELEASE(sSurf1);
			SAFERELEASE(sSurf2);
			SAFERELEASE(mainTex);
			SAFERELEASE(sTex1);
			SAFERELEASE(sTex2);
			SAFERELEASE(vBuffer);
			SAFERELEASE(vBuffer2);
			SAFERELEASE(d3d9Device);
			SAFERELEASE(d3d9);
			SAFERELEASE(gpuPalette);
			SAFERELEASE(gpuBltEffect);
			SAFERELEASE(movieBuffer);

			delete this;
			return 0;
		} else return Refs;
	}

	/* IDirectDraw methods */

	HRESULT __stdcall Compact() { UNUSEDFUNCTION; }
	HRESULT __stdcall CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*) { UNUSEDFUNCTION; }

	HRESULT __stdcall CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE* c, IUnknown*) { // called 0x4CB182 GNW95_init_DirectDraw_
		*c = (IDirectDrawPalette*)new FakeDirectDrawPalette();
		return DD_OK;
	}

	/*
		0x4CB094 GNW95_init_DirectDraw_ (primary surface)
		0x4F5DD4/0x4F5DF9 nfConfig_     (mve surface)
	*/
	HRESULT __stdcall CreateSurface(LPDDSURFACEDESC a, LPDIRECTDRAWSURFACE* b, IUnknown* c) {
		if (a->ddsCaps.dwCaps == DDSCAPS_PRIMARYSURFACE && a->dwFlags == DDSD_CAPS) {
			*b = primaryDDSurface = (IDirectDrawSurface*)new FakeDirectDrawSurface(true);
		} else {
			*b = (IDirectDrawSurface*)new FakeDirectDrawSurface(false);
		}
		return DD_OK;
	}

	HRESULT __stdcall DuplicateSurface(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE *) { UNUSEDFUNCTION; }
	HRESULT __stdcall EnumDisplayModes(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMMODESCALLBACK) { UNUSEDFUNCTION; }
	HRESULT __stdcall EnumSurfaces(DWORD, LPDDSURFACEDESC, LPVOID,LPDDENUMSURFACESCALLBACK) { UNUSEDFUNCTION; }
	HRESULT __stdcall FlipToGDISurface() { UNUSEDFUNCTION; }
	HRESULT __stdcall GetCaps(LPDDCAPS, LPDDCAPS b) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetDisplayMode(LPDDSURFACEDESC) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetFourCCCodes(LPDWORD,LPDWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetGDISurface(LPDIRECTDRAWSURFACE *) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetMonitorFrequency(LPDWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetScanLine(LPDWORD) { UNUSEDFUNCTION; }
	HRESULT __stdcall GetVerticalBlankStatus(LPBOOL) { UNUSEDFUNCTION; }
	HRESULT __stdcall Initialize(GUID *) { UNUSEDFUNCTION; }
	HRESULT __stdcall RestoreDisplayMode() { return DD_OK; }

	HRESULT __stdcall SetCooperativeLevel(HWND a, DWORD b) { // called 0x4CB005 GNW95_init_DirectDraw_
		window = a;

		if (!d3d9Device) {
			CoInitialize(0);
			ResetDevice(true); // create
		}
		dlog("Creating D3D9 Device window...", DL_MAIN);

		if (Graphics::mode >= 5) {
			SetWindowLong(a, GWL_STYLE, windowStyle);
			RECT r;
			r.left = 0;
			r.right = gWidth;
			r.top = 0;
			r.bottom = gHeight;
			AdjustWindowRect(&r, windowStyle, false);
			r.right -= r.left;
			r.bottom -= r.top;
			SetWindowPos(a, HWND_NOTOPMOST, windowLeft, windowTop, r.right, r.bottom, SWP_DRAWFRAME | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}

		dlogr(" Done", DL_MAIN);
		return DD_OK;
	}

	HRESULT __stdcall SetDisplayMode(DWORD, DWORD, DWORD) { return DD_OK; } // called 0x4CB01B GNW95_init_DirectDraw_
	HRESULT __stdcall WaitForVerticalBlank(DWORD, HANDLE) { UNUSEDFUNCTION; }
};

HRESULT __stdcall InitFakeDirectDrawCreate(void*, IDirectDraw** b, void*) {
	dlog("Initializing Direct3D...", DL_MAIN);

	// original resolution or HRP
	ResWidth = *(DWORD*)0x4CAD6B;  // 640
	ResHeight = *(DWORD*)0x4CAD66; // 480

	if (!d3d9) d3d9 = Direct3DCreate9(D3D_SDK_VERSION);

	ZeroMemory(&surfaceDesc, sizeof(DDSURFACEDESC));

	surfaceDesc.dwSize = sizeof(DDSURFACEDESC);
	surfaceDesc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH;
	surfaceDesc.dwWidth = ResWidth;
	surfaceDesc.dwHeight = ResHeight;
	surfaceDesc.ddpfPixelFormat.dwRGBBitCount = 16; // R5G6B5
	surfaceDesc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	surfaceDesc.ddpfPixelFormat.dwRBitMask = 0xF800;
	surfaceDesc.ddpfPixelFormat.dwGBitMask = 0x7E0;
	surfaceDesc.ddpfPixelFormat.dwBBitMask = 0x1F;
	surfaceDesc.ddpfPixelFormat.dwFlags = DDPF_RGB;
	surfaceDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
	surfaceDesc.lPitch = ResWidth;

	// set params for .mve surface
	mveDesc = surfaceDesc;
	mveDesc.lPitch = 640;
	mveDesc.dwWidth = 640;
	mveDesc.dwHeight = 480;

	if (Graphics::mode == 6) {
		D3DDISPLAYMODE dispMode;
		GetDisplayMode(dispMode);
		gWidth  = dispMode.Width;
		gHeight = dispMode.Height;
	} else {
		gWidth  = GetConfigInt("Graphics", "GraphicsWidth", 0);
		gHeight = GetConfigInt("Graphics", "GraphicsHeight", 0);
		if (!gWidth || !gHeight) {
			gWidth  = ResWidth;
			gHeight = ResHeight;
		}
	}

	Graphics::GPUBlt = GetConfigInt("Graphics", "GPUBlt", 0); // 0 - auto, 1 - GPU, 2 - CPU
	if (!Graphics::GPUBlt || Graphics::GPUBlt > 2)
		Graphics::GPUBlt = 2; // Swap them around to keep compatibility with old ddraw.ini
	else if (Graphics::GPUBlt == 2) Graphics::GPUBlt = 0; // Use CPU

	if (Graphics::mode == 5) {
		moveWindowKey[0] = GetConfigInt("Input", "WindowScrollKey", 0);
		if (moveWindowKey[0] < 0) {
			switch (moveWindowKey[0]) {
			case -1:
				moveWindowKey[0] = DIK_LCONTROL;
				moveWindowKey[1] = DIK_RCONTROL;
				break;
			case -2:
				moveWindowKey[0] = DIK_LMENU;
				moveWindowKey[1] = DIK_RMENU;
				break;
			case -3:
				moveWindowKey[0] = DIK_LSHIFT;
				moveWindowKey[1] = DIK_RSHIFT;
				break;
			default:
				moveWindowKey[0] = 0;
			}
		} else {
			moveWindowKey[0] &= 0xFF;
		}
		windowData = GetConfigInt("Graphics", "WindowData", 0);
		if (windowData > 0) {
			windowLeft = windowData >> 16;
			windowTop = windowData & 0xFFFF;
		} else {
			windowData = 0;
		}
	}

	rcpres[0] = 1.0f / (float)gWidth;
	rcpres[1] = 1.0f / (float)gHeight;

	if (textureFilter) {
		float wScale = (float)gWidth  / ResWidth;
		float hScale = (float)gHeight / ResHeight;
		if (wScale == 1.0f && hScale == 1.0f) textureFilter = 0; // disable texture filtering if the set resolutions are equal
		if (textureFilter == 1) {
			if (static_cast<int>(wScale) == wScale && static_cast<int>(hScale) == hScale) {
				textureFilter = 0; // disable for integer scales
			}
		}
	}

	*b = (IDirectDraw*)new FakeDirectDraw();

	dlogr(" Done", DL_MAIN);
	return DD_OK;
}

static __declspec(naked) void game_init_hook() {
	__asm {
		push ecx;
		call WindowInit;
		pop  ecx;
		jmp  fo::funcoffs::palette_init_;
	}
}

static double fadeMulti;

static __declspec(naked) void palette_fade_to_hook() {
	__asm {
		push ebx; // _fade_steps
		fild [esp];
		fmul fadeMulti;
		fistp [esp];
		pop  ebx;
		jmp  fo::funcoffs::fadeSystemPalette_;
	}
}

///////////////////////////////////////////////////////////////////////////////

static void __forceinline UpdateDDSurface(BYTE* surface, int width, int height, int widthFrom, RECT* rect) {
	DDSURFACEDESC desc;
	RECT lockRect = { rect->left, rect->top, rect->right + 1, rect->bottom + 1 };

	primaryDDSurface->Lock(&lockRect, &desc, 0, 0);

	fo::func::buf_to_buf(surface, width, height, widthFrom, (BYTE*)desc.lpSurface, desc.lPitch); ///+ (desc.lPitch * rect->top) + rect->left

	primaryDDSurface->Unlock(desc.lpSurface);
}

static BYTE* GetBuffer() {
	return (BYTE*)*(DWORD*)FO_VAR_screen_buffer;
}

static void __fastcall sf_GNW_win_refresh(fo::Window* win, RECT* updateRect, BYTE* toBuffer) {
	if (win->flags & fo::WinFlags::Hidden) return;
	fo::RectList* rects;

	if (win->flags & fo::WinFlags::Transparent && !*(DWORD*)FO_VAR_doing_refresh_all) {
		__asm {
			mov  eax, updateRect;
			mov  edx, ds:[FO_VAR_screen_buffer];
			call fo::funcoffs::refresh_all_;
		}
		int w = updateRect->right - updateRect->left + 1;

		if (fo::var::mouse_is_hidden || !fo::func::mouse_in(updateRect->left, updateRect->top, updateRect->right, updateRect->bottom)) {
			/*__asm {
				mov  eax, win;
				mov  edx, updateRect;
				call fo::funcoffs::GNW_button_refresh_;
			}*/
			if (!DeviceLost) {
				int h = (updateRect->bottom - updateRect->top) + 1;
				UpdateDDSurface(GetBuffer(), w, h, w, updateRect); // update the entire rectangle area
			}
		} else {
			fo::func::mouse_show(); // for update background cursor area
			RECT mouseRect;
			__asm {
				lea  eax, mouseRect;
				mov  edx, eax;
				call fo::funcoffs::mouse_get_rect_;
				mov  eax, updateRect;
				call fo::funcoffs::rect_clip_;
				mov  rects, eax;
			}
			while (rects) { // updates everything except the cursor area
				//__asm {
				//	mov  eax, win;
				//	mov  edx, rects;
				//	call fo::funcoffs::GNW_button_refresh_;
				//}
				if (!DeviceLost) {
					int wRect = (rects->wRect.right - rects->wRect.left) + 1;
					int hRect = (rects->wRect.bottom - rects->wRect.top) + 1;
					UpdateDDSurface(&GetBuffer()[rects->wRect.left - updateRect->left] + (rects->wRect.top - updateRect->top) * w, wRect, hRect, w, &rects->wRect);
				}
				fo::RectList* next = rects->nextRect;
				fo::sf_rect_free(rects);
				rects = next;
			}
		}
		return;
	}

	/* Allocates memory for 10 RectList (if no memory was allocated), returns the first Rect and removes it from the list */
	__asm {
		call fo::funcoffs::rect_malloc_;
		mov  rects, eax;
	}
	if (!rects) return;

	rects->rect = { updateRect->left, updateRect->top, updateRect->right, updateRect->bottom };
	rects->nextRect = nullptr;
	RECT &rect = rects->wRect;

	/*
		if the border of the updateRect rectangle is located outside the window, then assign to rects->rect the border of the window rectangle
		otherwise, rects->rect contains the borders from the update rectangle (updateRect)
	*/
	if (win->wRect.left >= rect.left) rect.left = win->wRect.left;
	if (win->wRect.top >= rect.top) rect.top = win->wRect.top;
	if (win->wRect.right <= rect.right) rect.right = win->wRect.right;
	if (win->wRect.bottom <= rect.bottom) rect.bottom = win->wRect.bottom;

	if (rect.right < rect.left || rect.bottom < rect.top) {
		fo::sf_rect_free(rects);
		return;
	}

	int widthFrom = win->width;
	int toWidth = (toBuffer) ? updateRect->right - updateRect->left + 1 : Graphics::GetGameWidthRes();

	fo::func::win_clip(win, &rects, toBuffer);

	fo::RectList* currRect = rects;
	while (currRect)
	{
		RECT &crect = currRect->wRect;
		int width = (crect.right - crect.left) + 1;   // for current rectangle
		int height = (crect.bottom - crect.top) + 1;; // for current rectangle

		BYTE* surface;
		if (win->wID) {
			__asm {
				mov  eax, win;
				mov  edx, currRect;
				call fo::funcoffs::GNW_button_refresh_;
			}
			surface = &win->surface[crect.left - win->rect.x] + ((crect.top - win->rect.y) * win->width);
		} else {
			surface = new BYTE[height * width](); // black background
			widthFrom = width; // replace to rectangle
		}

		auto drawFunc = (win->flags & fo::WinFlags::Transparent && win->wID) ? fo::func::trans_buf_to_buf : fo::func::buf_to_buf;
		if (toBuffer) {
			drawFunc(surface, width, height, widthFrom, &toBuffer[crect.left - updateRect->left] + ((crect.top - updateRect->top) * toWidth), toWidth);
		} else {
			// copy to buffer instead of DD surface (buffering)
			drawFunc(surface, width, height, widthFrom, &GetBuffer()[crect.left] + (crect.top * toWidth), toWidth);
			///if (!DeviceLost) UpdateDDSurface(surface, width, height, widthFrom, crect);
		}
		if (win->wID == 0) delete[] surface;

		currRect = currRect->nextRect;
	}

	while (rects)
	{   // copy all rectangles from the buffer to the DD surface (buffering)
		if (!toBuffer && !DeviceLost) {
			int width = (rects->rect.offx - rects->rect.x) + 1;
			int height = (rects->rect.offy - rects->rect.y) + 1;
			int widthFrom = toWidth;
			UpdateDDSurface(&GetBuffer()[rects->rect.x] + (rects->rect.y * widthFrom), width, height, widthFrom, &rects->wRect);
		}
		fo::RectList* next = rects->nextRect;
		fo::sf_rect_free(rects);
		rects = next;
	}

	if (!toBuffer && !*(DWORD*)FO_VAR_doing_refresh_all && !fo::var::mouse_is_hidden && fo::func::mouse_in(updateRect->left, updateRect->top, updateRect->right, updateRect->bottom)) {
		fo::func::mouse_show();
	}
}

static __declspec(naked) void GNW_win_refresh_hack() {
	__asm {
		push ebx; // toBuffer
		mov  ecx, eax;
		call sf_GNW_win_refresh;
		pop  ecx;
		retn;
	}
}

void Graphics::init() {
	Graphics::mode = GetConfigInt("Graphics", "Mode", 0);
	if (Graphics::mode == 6) {
		windowStyle = WS_OVERLAPPED;
	} else if (Graphics::mode != 4 && Graphics::mode != 5) {
		Graphics::mode = 0;
	}

	if (Graphics::mode) {
		dlog("Applying DX9 graphics patch.", DL_INIT);
#define _DLL_NAME "d3dx9_43.dll"
		HMODULE h = LoadLibraryEx(_DLL_NAME, 0, LOAD_LIBRARY_AS_DATAFILE);
		if (!h) {
			MessageBoxA(0, "You have selected DirectX graphics mode, but " _DLL_NAME " is missing.\n"
						   "Switch back to mode 0, or install an up to date version of DirectX.", "Error", MB_TASKMODAL | MB_ICONERROR);
#undef _DLL_NAME
			ExitProcess(-1);
		} else {
			FreeLibrary(h);
		}
		SafeWrite8(0x50FB6B, '2'); // Set call DirectDrawCreate2
		HookCall(0x44260C, game_init_hook);

		textureFilter = GetConfigInt("Graphics", "TextureFilter", 1);
		dlogr(" Done", DL_INIT);
	}

	fadeMulti = GetConfigInt("Graphics", "FadeMultiplier", 100);
	if (fadeMulti != 100) {
		dlog("Applying fade patch.", DL_INIT);
		HookCall(0x493B16, palette_fade_to_hook);
		fadeMulti = ((double)fadeMulti) / 100.0;
		dlogr(" Done", DL_INIT);
	}

	// Replacing the srcCopy_ function with an SSE implementation
	MakeJump(0x4D36D4, fo::func::buf_to_buf); // buf_to_buf_
	// Replacing the transSrcCopy_ function
	MakeJump(0x4D3704, fo::func::trans_buf_to_buf); // trans_buf_to_buf_

	// Enables support for transparent interface windows
	SafeWrite16(0x4D5D46, 0x9090); // win_init_ (create screen_buffer)
	if (Graphics::mode) {
		// custom implementation of the GNW_win_refresh function
		MakeJump(0x4D6FD9, GNW_win_refresh_hack, 1);
		SafeWrite16(0x4D75E6, 0x9090); // win_clip_ (remove _buffering checking)
	} else { // for default or HRP graphics mode
		SafeWrite8(0x4D5DAB, 0x1D); // ecx > ebx (_buffering enable)
		BlockCall(0x431076); // dialogMessage_
	}
}

void Graphics::exit() {
	if (Graphics::mode) {
		if (Graphics::mode == 5) {
			int data = windowTop | (windowLeft << 16);
			if (data >= 0 && data != windowData) SetConfigInt("Graphics", "WindowData", data);
		}
		CoUninitialize();
	}
}

}

// This should be in global namespace
HRESULT __stdcall FakeDirectDrawCreate2(void* a, IDirectDraw** b, void* c) {
	return sfall::InitFakeDirectDrawCreate(a, b, c);
}
