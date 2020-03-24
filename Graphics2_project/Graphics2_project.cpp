// Graphics2_project.cpp : Defines the entry point for the application.

#include <vector>
#include <string>
#include <fstream>

#include "framework.h"
#include "Graphics2_project.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include "assets/StoneHenge.h"
#include "Camera.h"
#include "Enums_globals.h"
#include "DDSTextureLoader.h"

#include "debug_vs.csh"
#include "debug_ps.csh"

#include "base_vs.csh"
#include "base_ps.csh"

#include "henge_vs.csh"

#include "XTime.h"
#include "Structures.h"

using namespace DirectX;

// Globals
// For initialization and utility
ID3D11Device* device;
IDXGISwapChain* swapchain;
ID3D11DeviceContext* deviceContext;

// States
ID3D11RasterizerState* rasterizerStateDefault;
ID3D11RasterizerState* rasterizerStateWireframe;

// For drawing
ID3D11RenderTargetView* renderTargetView;
D3D11_VIEWPORT viewport;
float aspectRatio = 1;

// Shader variables
ID3D11Buffer* constantBuffer;
ID3D11Buffer* lightBuffer;
// Shader variables

// Z buffer
ID3D11Texture2D* zBuffer;
ID3D11DepthStencilView* depthStencil;
// Z buffer

// Camera shit
FPSCamera camera;

bool g_wireframeEnabled = false;
float rotation_value = 0;

// Math globals
struct WorldViewProjection
{
	XMFLOAT4X4 WorldMatrix;
	XMFLOAT4X4 ViewMatrix;
	XMFLOAT4X4 ProjectionMatrix;
	XMFLOAT4 cameraPosition;
}WORLD;

// Debug shit
struct 
{
	std::vector<DebugLineVertex> positions;
	WorldViewProjection mat;

	ID3D11Buffer* vertexBuffer;

	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;

	ID3D11InputLayout* vertexBufferLayout;

	// Animation
	XTime xtime;
	float timer = 0;
	float key_timer = 0;
	int current_keyframe = 0;
} debug;

// Object shit
struct Model
{
	std::vector<AnimVertex> vertices;
	std::vector<int> indices;
	std::vector<XMMATRIX> joints;

	XMFLOAT3 position;

	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;

	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;

	ID3D11InputLayout* vertexBufferLayout;

	// Texture stuff
	ID3D11SamplerState* sampler;

	ID3D11ShaderResourceView* diffuse;
	ID3D11ShaderResourceView* specular;
	ID3D11ShaderResourceView* emissive;
	ID3D11ShaderResourceView* normal;

	// Animation
	animation_clip clip;
};

enum class LIGHTTYPE
{
	DIRECTIONAL = 0,
	POINT,
	SPOT,
	CAPSULE
};

struct Light
{
	XMFLOAT4 position, lightDirection;
	XMFLOAT4 ambientUp, ambientDown, diffuse, specular;
	unsigned int lightType;
	float lightRadius;
	float cosineInnerCone, cosineOuterCone;
	float ambientIntensityUp, ambientIntensityDown, diffuseIntensity, specularIntensity;
	float lightLength, p1, p2, p3;
};

struct BUFFERS
{
	ID3D11Buffer* c_bufer;
};

// Models
Model model;
BUFFERS buffers;
std::vector<Light> lights;

// Globals

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


// FUNCTION DECLARATION

void HandleInput();
void LoadWobjectMesh(const char* meshname, std::vector<AnimVertex>& vertices, std::vector<int>& indices);
void LoadAnimationWobjectMesh(const char* meshname, std::vector<AnimVertex>& vertices, std::vector<int>& indices);
void LoadTextures(Header& header, Model& _model);
void InitializeWModel(Model& _model, const char* modelname, XMFLOAT3 position, const BYTE* _vs, const BYTE* _ps, int vs_size, int ps_size);
void CleanupModel(Model& model);

void UpdateSkeleton(float time);
void Animate();

// Utility
float lerp(float current, float next, float f);
// Utility

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GRAPHICS2PROJECT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GRAPHICS2PROJECT));

    MSG msg;

    // Main message loop:
	debug.xtime.Restart();
	while (true)
	{
		// Timing
		debug.xtime.Signal();
		debug.timer += static_cast<float>(debug.xtime.Delta());
		debug.key_timer += static_cast<float>(debug.xtime.Delta());
		Animate();
		UpdateSkeleton(debug.timer);

		PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
			break;
		if (GetAsyncKeyState(VK_ESCAPE)) break;

		// Input shit
		HandleInput();

		// DONT FUCK WITH THIS
		// Rendering shit right here
		// Output merger
		ID3D11RenderTargetView* tempRTV[] = { renderTargetView };
		deviceContext->OMSetRenderTargets(1, tempRTV, depthStencil);

		float color[4] = { 0, 0, 0, 1 };
		deviceContext->ClearRenderTargetView(renderTargetView, color);
		deviceContext->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH, 1, 0);

		// Rasterizer
		deviceContext->RSSetViewports(1, &viewport);
		// DONT FUCK WITH THIS

		// TEAPOT
		// World matrix projection
		XMMATRIX temp = XMMatrixRotationY(XMConvertToRadians(180));
		temp = XMMatrixMultiply(temp, XMMatrixScaling(.2, .2, .2));
		temp = XMMatrixMultiply(temp, XMMatrixTranslation(model.position.x, model.position.y, model.position.z));
		XMStoreFloat4x4(&WORLD.WorldMatrix, temp);
		// View
		camera.GetViewMatrix(temp);
		XMStoreFloat4x4(&WORLD.ViewMatrix, temp);
		// Projection
		temp = XMMatrixPerspectiveFovLH(camera.GetFOV(), aspectRatio, 0.1f, 1000);
		XMStoreFloat4x4(&WORLD.ProjectionMatrix, temp);
		// Camera
		XMFLOAT3 POS = camera.GetPosition();
		WORLD.cameraPosition = XMFLOAT4(POS.x, POS.y, POS.z, 1);

		// Send the matrix to constant buffer
		D3D11_MAPPED_SUBRESOURCE gpuBuffer;
		HRESULT result = deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &gpuBuffer);
		memcpy(gpuBuffer.pData, &WORLD, sizeof(WORLD));
		deviceContext->Unmap(constantBuffer, 0);
		// Connect constant buffer to the pipeline
		ID3D11Buffer* teapotCBuffers[] = { constantBuffer };
		deviceContext->VSSetConstantBuffers(0, 1, teapotCBuffers);

		// sET THE PIPELINE
		UINT teapotstrides[] = { sizeof(AnimVertex) };
		UINT teapotoffsets[] = { 0 };
		ID3D11Buffer* teapotVertexBuffers[] = { model.vertexBuffer };
		deviceContext->IASetVertexBuffers(0, 1, teapotVertexBuffers, teapotstrides, teapotoffsets);
		deviceContext->IASetIndexBuffer(model.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		deviceContext->PSSetSamplers(0, 1, &model.sampler);

		// Send the lights to constant buffer
		D3D11_MAPPED_SUBRESOURCE lightSub;
		result = deviceContext->Map(lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &lightSub);
		ASSERT_HRESULT_SUCCESS(result);
		memcpy(lightSub.pData, lights.data(), sizeof(Light) * lights.size());
		deviceContext->Unmap(lightBuffer, 0);
		// Connect constant buffer to the pipeline
		ID3D11Buffer* lightCbuffers[] = { lightBuffer };
		deviceContext->PSSetConstantBuffers(0, 1, lightCbuffers);
		
		// Textures
		ID3D11ShaderResourceView** rs[] = {
			&model.diffuse,
			&model.specular,
			&model.emissive,
			&model.normal
		};
		deviceContext->PSSetShaderResources(0, 4, *rs);
		deviceContext->IASetInputLayout(model.vertexBufferLayout);
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		deviceContext->VSSetShader(model.vertexShader, 0, 0);
		deviceContext->PSSetShader(model.pixelShader, 0, 0);

		deviceContext->DrawIndexed(model.indices.size(), 0, 0);
		// TEAPOT

		// Debug
		// Set world matrix
		temp = XMMatrixScaling(.3, .3, .3);
		temp = XMMatrixMultiply(temp, XMMatrixRotationY(XMConvertToRadians(180)));
		XMStoreFloat4x4(&debug.mat.WorldMatrix, temp);
		// View mat
		debug.mat.ViewMatrix = WORLD.ViewMatrix;
		debug.mat.ProjectionMatrix = WORLD.ProjectionMatrix;
		debug.mat.cameraPosition = WORLD.cameraPosition;

		// Send the matrix to constant buffer
		D3D11_MAPPED_SUBRESOURCE debugbuffer;
		result = deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &debugbuffer);
		memcpy(debugbuffer.pData, &debug.mat, sizeof(WorldViewProjection));
		deviceContext->Unmap(constantBuffer, 0);
		// Connect constant buffer to the pipeline
		ID3D11Buffer* debug_b[] = { constantBuffer };
		deviceContext->VSSetConstantBuffers(0, 1, debug_b);

		deviceContext->UpdateSubresource(debug.vertexBuffer, 0, NULL, debug.positions.data(), 0, 0);

		// Set the pipeline
		unsigned int debug_strides[] = {sizeof(DebugLineVertex)};
		unsigned int debug_offsets[] = {0};
		ID3D11Buffer* debugvbuffer[] = { debug.vertexBuffer };
		deviceContext->IASetInputLayout(debug.vertexBufferLayout);
		deviceContext->IASetVertexBuffers(0, 1, debugvbuffer, debug_strides, debug_offsets);
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		deviceContext->VSSetShader(debug.vertexShader, 0, 0);
		deviceContext->PSSetShader(debug.pixelShader, 0, 0);

		deviceContext->Draw(debug.positions.size(), 0);
		// Debug

		swapchain->Present(0, 0);
    }

	// Release a million fucking things
	D3DSAFE_RELEASE(swapchain);
	D3DSAFE_RELEASE(deviceContext);
	D3DSAFE_RELEASE(device);
	D3DSAFE_RELEASE(renderTargetView);
	D3DSAFE_RELEASE(constantBuffer);
	D3DSAFE_RELEASE(lightBuffer);

	D3DSAFE_RELEASE(rasterizerStateDefault);
	D3DSAFE_RELEASE(rasterizerStateWireframe);

	D3DSAFE_RELEASE(zBuffer);
	D3DSAFE_RELEASE(depthStencil);

	// Debug shit
	D3DSAFE_RELEASE(debug.vertexBuffer);
	D3DSAFE_RELEASE(debug.vertexBufferLayout);
	D3DSAFE_RELEASE(debug.vertexShader);
	D3DSAFE_RELEASE(debug.pixelShader);

	CleanupModel(model);

    return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GRAPHICS2PROJECT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_GRAPHICS2PROJECT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(
	   szWindowClass, 
	   szTitle,
	   WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	   CW_USEDEFAULT, 
	   0, 
	   CW_USEDEFAULT, 
	   0, 
	   nullptr, 
	   nullptr, 
	   hInstance, 
	   nullptr);


   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   RECT rect;
   GetClientRect(hWnd, &rect);

   // Attach d3d to the window
   D3D_FEATURE_LEVEL DX11 = D3D_FEATURE_LEVEL_11_0;
   DXGI_SWAP_CHAIN_DESC swap;
   ZeroMemory(&swap, sizeof(DXGI_SWAP_CHAIN_DESC));
   swap.BufferCount = 1;
   swap.OutputWindow = hWnd;
   swap.Windowed = true;
   swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
   swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   swap.BufferDesc.Width = rect.right - rect.left;
   swap.BufferDesc.Height = rect.bottom - rect.top;
   swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   swap.SampleDesc.Count = 1;

   aspectRatio = swap.BufferDesc.Width / (float)swap.BufferDesc.Height;

   HRESULT result;

   result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, &DX11, 1, D3D11_SDK_VERSION, &swap, &swapchain, &device, 0, &deviceContext);


   ID3D11Resource* backbuffer;
   result = swapchain->GetBuffer(0, __uuidof(backbuffer), (void**)&backbuffer);
   result = device->CreateRenderTargetView(backbuffer, NULL, &renderTargetView);

   // Release the resource to decrement the counter by one
   // This is necessary to keep the thing from leaking memory
   backbuffer->Release();

   // Setup viewport
   viewport.Width = swap.BufferDesc.Width;
   viewport.Height = swap.BufferDesc.Height;
   viewport.TopLeftY = viewport.TopLeftX = 0;
   viewport.MinDepth = 0;
   viewport.MaxDepth = 1;

   // Rasterizer state
   D3D11_RASTERIZER_DESC rdesc;
   ZeroMemory(&rdesc, sizeof(D3D11_RASTERIZER_DESC));
   rdesc.FrontCounterClockwise = false;
   rdesc.DepthBiasClamp = 1;
   rdesc.DepthBias = rdesc.SlopeScaledDepthBias = 0;
   rdesc.DepthClipEnable = true;
   rdesc.FillMode = D3D11_FILL_SOLID;
   rdesc.CullMode = D3D11_CULL_BACK;
   rdesc.AntialiasedLineEnable = false;
   rdesc.MultisampleEnable = false;

   device->CreateRasterizerState(&rdesc, &rasterizerStateDefault);

   // Wire frame Rasterizer State
   ZeroMemory(&rdesc, sizeof(D3D11_RASTERIZER_DESC));
   rdesc.FillMode = D3D11_FILL_WIREFRAME;
   rdesc.CullMode = D3D11_CULL_NONE;
   rdesc.DepthClipEnable = true;

   device->CreateRasterizerState(&rdesc, &rasterizerStateWireframe);

   deviceContext->RSSetState(rasterizerStateDefault);

   // Initialize camera
   camera.SetPosition(XMFLOAT3(0, 0, -5));
   camera.SetFOV(30);

   // TEAPOT
   InitializeWModel(model, "assets/models/Run.wobj", XMFLOAT3(0, -1, 0), base_vs, base_ps, sizeof(base_vs), sizeof(base_ps));
   // TEAPOT

   // Lights go here
   Light light;
   ZeroMemory(&light, sizeof(Light));
   light.lightType = (int)LIGHTTYPE::POINT;
   light.ambientUp = XMFLOAT4(0, 0, 1, 1);
   light.ambientDown = XMFLOAT4(0, 1, 0, 1);
   light.ambientIntensityDown = .05;
   light.ambientIntensityUp = .1;
   light.diffuse = light.specular = XMFLOAT4(1, 1, 1, 1);
   light.diffuseIntensity = 1;
   light.specularIntensity = .4;
   light.position = XMFLOAT4(2, 3, -5, 1);
   light.lightRadius = 100;
   lights.push_back(light);
   // Lights go here

   deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   // Create constant buffer
   D3D11_BUFFER_DESC bDesc;
   D3D11_SUBRESOURCE_DATA subdata;
   ZeroMemory(&bDesc, sizeof(D3D11_BUFFER_DESC));
   ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));

   bDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   bDesc.ByteWidth = sizeof(WorldViewProjection);
   bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
   bDesc.MiscFlags = 0;
   bDesc.StructureByteStride = 0;
   bDesc.Usage = D3D11_USAGE_DYNAMIC;

   result = device->CreateBuffer(&bDesc, nullptr, &constantBuffer);
   ASSERT_HRESULT_SUCCESS(result);

   // Create light buffer
   bDesc.ByteWidth = lights.size() * sizeof(Light);

   result = device->CreateBuffer(&bDesc, nullptr, &lightBuffer);
   ASSERT_HRESULT_SUCCESS(result);

   // Z buffer 
   D3D11_TEXTURE2D_DESC zDesc;
   ZeroMemory(&zDesc, sizeof(zDesc));
   zDesc.ArraySize = 1;
   zDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
   zDesc.Width = swap.BufferDesc.Width;
   zDesc.Height = swap.BufferDesc.Height;
   zDesc.Usage = D3D11_USAGE_DEFAULT;
   zDesc.Format = DXGI_FORMAT_D32_FLOAT;
   zDesc.MipLevels = 1;
   zDesc.SampleDesc.Count = 1;

   result = device->CreateTexture2D(&zDesc, nullptr, &zBuffer);

   D3D11_DEPTH_STENCIL_DESC zViewDesc;
   ZeroMemory(&zViewDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

   result = device->CreateDepthStencilView(zBuffer, nullptr, &depthStencil);

   assert(!FAILED(result));
   return TRUE;
}

void Animate()
{
	float time = debug.timer;

	if (time > model.clip.keyframes[debug.current_keyframe].time)
	{
		debug.current_keyframe++;

		if (debug.current_keyframe > model.clip.keyframes.size() - 1)
		{
			debug.timer = 0;
			debug.current_keyframe = 0;
		}
	}
}

void UpdateSkeleton(float time)
{
	debug.positions.clear();

	// Populate the lines
	FLOAT3 red{ 1, 0, 0 };
	FLOAT3 green{ 0, 1, 0 };
	FLOAT3 blue{ 0, 0, 1 };

	DebugLineVertex vert;
	XMMATRIX matrix;

	for (int i = 0; i < model.clip.keyframes[debug.current_keyframe].joints.size(); i++)
	{
		// Lerp with next frame
		// Compute blend ratio
		float elapsed_time = debug.timer;
		float lerp_ratio = 0.0f;
		float key_time = model.clip.keyframes[debug.current_keyframe].time;

		lerp_ratio = (key_time) / elapsed_time;
		if (debug.current_keyframe == 0)
			lerp_ratio = 0;

		int next_frame = debug.current_keyframe + 1;
		if (next_frame > model.clip.keyframes.size() - 1)
			next_frame = 0;

		// For each joint in the current keyframe, create the matrix data
		// X
		vert.position = {
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[12], model.clip.keyframes[next_frame].joints[i].transform.data[12], lerp_ratio),
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[13], model.clip.keyframes[next_frame].joints[i].transform.data[13], lerp_ratio),
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[14], model.clip.keyframes[next_frame].joints[i].transform.data[14], lerp_ratio),
		};
		memcpy(&vert.color, &red, sizeof(FLOAT3));
		debug.positions.push_back(vert);
		vert.position.x += .2;
		debug.positions.push_back(vert);
		// Y
		vert.position = {
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[12], model.clip.keyframes[next_frame].joints[i].transform.data[12], lerp_ratio),
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[13], model.clip.keyframes[next_frame].joints[i].transform.data[13], lerp_ratio),
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[14], model.clip.keyframes[next_frame].joints[i].transform.data[14], lerp_ratio),
		};
		memcpy(&vert.color, &green, sizeof(FLOAT3));
		debug.positions.push_back(vert);
		vert.position.y += .2;
		debug.positions.push_back(vert);
		// Z
		vert.position = {
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[12], model.clip.keyframes[next_frame].joints[i].transform.data[12], lerp_ratio),
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[13], model.clip.keyframes[next_frame].joints[i].transform.data[13], lerp_ratio),
			lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[14], model.clip.keyframes[next_frame].joints[i].transform.data[14], lerp_ratio),
		};
		memcpy(&vert.color, &blue, sizeof(FLOAT3));
		debug.positions.push_back(vert);
		vert.position.z += .2;
		debug.positions.push_back(vert);
	}

	// For each bone
	FLOAT3 white{ 1, 1, 1 };
	memcpy(&vert.color, &white, sizeof(FLOAT3));
	for (int i = model.clip.keyframes[debug.current_keyframe].joints.size() - 1; i > 0; i--)
	{
		int parent_index = model.clip.keyframes[debug.current_keyframe].joints[i].parent_index;

		// Compute blend ratio
		float elapsed_time = debug.timer;
		float lerp_ratio = 0.0f;
		float key_time = model.clip.keyframes[debug.current_keyframe].time;

		lerp_ratio = (key_time) / elapsed_time;
		if (debug.current_keyframe == 0)
			lerp_ratio = 0;

		int next_frame = debug.current_keyframe + 1;
		if (next_frame > model.clip.keyframes.size() - 1)
			next_frame = 0;

		if (parent_index != -1)
		{
			vert.position = {
				lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[12], model.clip.keyframes[next_frame].joints[i].transform.data[12], lerp_ratio),
				lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[13], model.clip.keyframes[next_frame].joints[i].transform.data[13], lerp_ratio),
				lerp(model.clip.keyframes[debug.current_keyframe].joints[i].transform.data[14], model.clip.keyframes[next_frame].joints[i].transform.data[14], lerp_ratio),
			};
			debug.positions.push_back(vert);

			vert.position = {
				lerp(model.clip.keyframes[debug.current_keyframe].joints[parent_index].transform.data[12], model.clip.keyframes[next_frame].joints[parent_index].transform.data[12], lerp_ratio),
				lerp(model.clip.keyframes[debug.current_keyframe].joints[parent_index].transform.data[13], model.clip.keyframes[next_frame].joints[parent_index].transform.data[13], lerp_ratio),
				lerp(model.clip.keyframes[debug.current_keyframe].joints[parent_index].transform.data[14], model.clip.keyframes[next_frame].joints[parent_index].transform.data[14], lerp_ratio),
			};
			debug.positions.push_back(vert);
		}
	}
}

void InitializeWModel(Model& _model, const char* modelname, XMFLOAT3 position, const BYTE* _vs, const BYTE* _ps, int vs_size, int ps_size)
{
	// TEAPOT
	// Set position
	_model.position = position;
	// Load mesh data
	D3D11_BUFFER_DESC bufferDesc;
	D3D11_SUBRESOURCE_DATA subdata;
	ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
	ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));
	// Actually load data
	LoadAnimationWobjectMesh(modelname, _model.vertices, _model.indices);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(AnimVertex) * _model.vertices.size();
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	subdata.pSysMem = _model.vertices.data();

	HRESULT result = device->CreateBuffer(&bufferDesc, &subdata, &_model.vertexBuffer);
	ASSERT_HRESULT_SUCCESS(result);

	// Index Buffer
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(int) * _model.indices.size();

	subdata.pSysMem = _model.indices.data();
	result = device->CreateBuffer(&bufferDesc, &subdata, &_model.indexBuffer);
	ASSERT_HRESULT_SUCCESS(result);

	// Load shaders
	result = device->CreateVertexShader(_vs, vs_size, nullptr, &_model.vertexShader);
	ASSERT_HRESULT_SUCCESS(result);
	result = device->CreatePixelShader(_ps, ps_size, nullptr, &_model.pixelShader);
	ASSERT_HRESULT_SUCCESS(result);

	// Make input layout for vertex buffer
	D3D11_INPUT_ELEMENT_DESC tempInputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	result = device->CreateInputLayout(tempInputElementDesc, ARRAYSIZE(tempInputElementDesc), _vs, vs_size, &_model.vertexBufferLayout);
	ASSERT_HRESULT_SUCCESS(result);

	// Make the necessary things for the debug render
	UpdateSkeleton(debug.timer);
	// Create the shaders
	D3D11_BUFFER_DESC debugBuffer;
	D3D11_SUBRESOURCE_DATA debugsub;
	ZeroMemory(&debugBuffer, sizeof(D3D11_BUFFER_DESC));
	ZeroMemory(&debugsub, sizeof(D3D11_SUBRESOURCE_DATA));
	debugBuffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	debugBuffer.ByteWidth = sizeof(DebugLineVertex) * debug.positions.size();
	debugBuffer.CPUAccessFlags = 0;
	debugBuffer.MiscFlags = 0;
	debugBuffer.StructureByteStride = 0;
	debugBuffer.Usage = D3D11_USAGE_DEFAULT;
	debugsub.pSysMem = debug.positions.data();

	result = device->CreateBuffer(&debugBuffer, &debugsub, &debug.vertexBuffer);
	ASSERT_HRESULT_SUCCESS(result);

	// Load shaders
	result = device->CreateVertexShader(debug_vs, sizeof(debug_vs), nullptr, &debug.vertexShader);
	ASSERT_HRESULT_SUCCESS(result);
	result = device->CreatePixelShader(debug_ps, sizeof(debug_ps), nullptr, &debug.pixelShader);
	ASSERT_HRESULT_SUCCESS(result);

	// Make input layout for vertex buffer
	D3D11_INPUT_ELEMENT_DESC debugia[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	result = device->CreateInputLayout(debugia, ARRAYSIZE(debugia), debug_vs, sizeof(debug_vs), &debug.vertexBufferLayout);
	ASSERT_HRESULT_SUCCESS(result);
}

void CleanupModel(Model& model)
{
	// teapot cleanup
	D3DSAFE_RELEASE(model.indexBuffer);
	D3DSAFE_RELEASE(model.pixelShader);
	D3DSAFE_RELEASE(model.vertexBuffer);
	D3DSAFE_RELEASE(model.vertexBufferLayout);
	D3DSAFE_RELEASE(model.vertexShader);
	// teapot cleanup

	// texture
	D3DSAFE_RELEASE(model.sampler);
	D3DSAFE_RELEASE(model.diffuse);
	D3DSAFE_RELEASE(model.specular);
	D3DSAFE_RELEASE(model.emissive);
	D3DSAFE_RELEASE(model.normal);
	// texture
}

void LoadWobjectMesh(const char* meshname, std::vector<AnimVertex>& vertices, std::vector<int>& indices)
{
	MeshHeader header;

	std::string smeshname = meshname;

	assert(smeshname.find(".fbx") == std::string::npos);

	std::ifstream file;
	file.open(meshname, std::ios::binary | std::ios::in);
	assert(file.is_open());

	// Read in the header
	file.read((char*)&header, sizeof(MeshHeader));
	std::string s = header.texturename;

	// Create a buffer to hold the data
	char* buffer = new char[header.vertexcount * (size_t)sizeof(AnimVertex)];

	// Read in the indices
	file.read((char*)buffer, header.indexcount * (size_t)sizeof(int));
	indices.resize(header.indexcount);
	memcpy(indices.data(), buffer, sizeof(int) * header.indexcount);

	// Read in the vertices
	file.read((char*)buffer, header.vertexcount * (size_t)sizeof(AnimVertex));
	vertices.resize(header.vertexcount);
	memcpy(vertices.data(), buffer, sizeof(AnimVertex) * header.vertexcount);

	// Free memory
	delete[] buffer;

	file.close();
}

void LoadAnimationWobjectMesh(const char* meshname, std::vector<AnimVertex>& vertices, std::vector<int>& indices)
{
	Header header;

	std::string smeshname = meshname;

	// Break if fbx
	assert(smeshname.find(".fbx") == std::string::npos);
	// Break if not wobj
	assert(smeshname.find(".wobj") != std::string::npos);

	std::ifstream file;
	file.open(meshname, std::ios::binary | std::ios::in);
	assert(file.is_open());

	// Read in the header
	file.read((char*)&header, sizeof(Header));
	std::string s = header.t_diffuse;

	// Create a buffer to hold the data
	int keyframe_size = (header.joint_count * sizeof(joint)) + sizeof(float);
	int animation_clip_size = (keyframe_size * header.keyframe_count) + sizeof(float);
	int vertex_buffer_size = header.vertexcount * (size_t)sizeof(AnimVertex);

	int max_buffer_size = animation_clip_size > vertex_buffer_size ? animation_clip_size : vertex_buffer_size;

	char* buffer = new char[max_buffer_size];

	// Read in the indices
	file.read((char*)buffer, header.indexcount * (size_t)sizeof(int));
	indices.resize(header.indexcount);
	memcpy(indices.data(), buffer, sizeof(int) * header.indexcount);

	// Read in the vertices
	file.read((char*)buffer, header.vertexcount * (size_t)sizeof(AnimVertex));
	vertices.resize(header.vertexcount);
	memcpy(vertices.data(), buffer, sizeof(AnimVertex) * header.vertexcount);

	// Read in the animation data
	file.read((char*)buffer, animation_clip_size);
	file.close();

	// Resize the thing to hold the data
	model.clip.keyframes.resize(header.keyframe_count);
	for (int i = 0; i < model.clip.keyframes.size(); i++)
	{
		model.clip.keyframes[i].joints.resize(header.joint_count);
		model.clip.keyframes[i].xmjoints.resize(header.joint_count);
	}

	// Read in duration
	int dataptr = 0;
	memcpy((char*)&model.clip.duration, buffer, sizeof(float));
	dataptr += sizeof(float);

	// Read in each keyframe
	int joint_transform_bytes_per_keyframe = sizeof(joint) * header.joint_count;
	for (int i = 0; i < header.keyframe_count; i++)
	{
		// Copy the duration
		memcpy(&model.clip.keyframes[i].time, &buffer[dataptr], sizeof(float));
		dataptr += sizeof(float);
		// Copy the "joints"
		memcpy(model.clip.keyframes[i].joints.data(), &buffer[dataptr], joint_transform_bytes_per_keyframe);
		dataptr += joint_transform_bytes_per_keyframe;
	}

	XMMATRIX temp;
	for (int i = 0; i < header.keyframe_count; i++)
	{
		for (int j = 0; j < header.joint_count; j++)
		{
			memcpy(&temp.r[0].m128_f32, &model.clip.keyframes[i].joints[j].transform.data[ 0], sizeof(float) * 4);
			memcpy(&temp.r[1].m128_f32, &model.clip.keyframes[i].joints[j].transform.data[ 4], sizeof(float) * 4);
			memcpy(&temp.r[2].m128_f32, &model.clip.keyframes[i].joints[j].transform.data[ 8], sizeof(float) * 4);
			memcpy(&temp.r[3].m128_f32, &model.clip.keyframes[i].joints[j].transform.data[12], sizeof(float) * 4);

			model.clip.keyframes[i].xmjoints[j] = temp;
		}
	}

	// Free memory
	delete[] buffer;

	// Load textures
	LoadTextures(header, model);
}

float lerp(float current, float next, float f)
{
	return current + f * (next - current);
}

void LoadTextures(Header& header, Model& _model)
{
	HRESULT result;

	// Construct wide string with filename
	std::string spath = header.t_diffuse;
	spath = std::string("assets/textures/").append(spath);
	std::wstring wpath = std::wstring(spath.begin(), spath.end());

	// Load the albedo texture
	result = CreateDDSTextureFromFile(device, wpath.c_str(), nullptr, &_model.diffuse);
	assert(!FAILED(result));

	// Metallic
	spath = header.t_specular;
	spath = std::string("assets/textures/").append(spath);
	wpath = std::wstring(spath.begin(), spath.end());
	result = CreateDDSTextureFromFile(device, wpath.c_str(), nullptr, &_model.specular);
	assert(!FAILED(result));

	// Emissive
	spath = header.t_emissive;
	spath = std::string("assets/textures/").append(spath);
	wpath = std::wstring(spath.begin(), spath.end());
	result = CreateDDSTextureFromFile(device, wpath.c_str(), nullptr, &_model.emissive);
	assert(!FAILED(result));
	
	// Normal
	spath = header.t_normal;
	spath = std::string("assets/textures/").append(spath);
	wpath = std::wstring(spath.begin(), spath.end());
	result = CreateDDSTextureFromFile(device, wpath.c_str(), nullptr, &_model.normal);
	assert(!FAILED(result));

	// Create texture sample state
	D3D11_SAMPLER_DESC sdesc;
	ZeroMemory(&sdesc, sizeof(D3D11_SAMPLER_DESC));
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sdesc.MinLOD = 0;
	sdesc.MaxLOD = D3D11_FLOAT32_MAX;
	// Create the sampler state
	result = device->CreateSamplerState(&sdesc, &_model.sampler);
	assert(!FAILED(result));
}

void HandleInput()
{
	static float move_thresh = .01;
	static float look_thresh = .03;
	XMFLOAT3 direction;

	if (GetAsyncKeyState((int)BUTTONS::NUM_ONE))
	{
		deviceContext->RSSetState(rasterizerStateWireframe);
	}
	else if (GetAsyncKeyState((int)BUTTONS::NUM_TWO))
	{
		deviceContext->RSSetState(rasterizerStateDefault);
	}
	// Movement
	if (GetAsyncKeyState((int)BUTTONS::LETTER_W))
	{
		direction = camera.GetLook();
		direction.x *= move_thresh;
		direction.y *= move_thresh;
		direction.z *= move_thresh;
		camera.Move(direction);
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_S))
	{
		direction = camera.GetLook();
		direction.x *= -move_thresh;
		direction.y *= -move_thresh;
		direction.z *= -move_thresh;
		camera.Move(direction);
	} 
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_A))
	{
		direction = camera.GetRight();
		direction.x *= move_thresh;
		direction.y *= move_thresh;
		direction.z *= move_thresh;
		camera.Move(direction);
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_D))
	{
		direction = camera.GetRight();
		direction.x *= -move_thresh;
		direction.y *= -move_thresh;
		direction.z *= -move_thresh;
		camera.Move(direction);
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_Z))
	{
		camera.Move(XMFLOAT3(0, move_thresh, 0));
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_X))
	{
		camera.Move(XMFLOAT3(0, -move_thresh, 0));
	}

	// Looking
	if (GetAsyncKeyState(VK_UP))
	{
		camera.Rotate(0, look_thresh);
	}
	else if (GetAsyncKeyState(VK_DOWN))
	{
		camera.Rotate(0, -look_thresh);
	}
	else if (GetAsyncKeyState(VK_LEFT))
	{
		camera.Rotate(look_thresh, 0);
	}
	else if (GetAsyncKeyState(VK_RIGHT))
	{
		camera.Rotate(-look_thresh, 0);
	}

	// Ayye
	if (GetAsyncKeyState(VK_SPACE))
	{
		if (debug.key_timer > .1)
		{
			debug.key_timer = 0;
			debug.current_keyframe++;

			if (debug.current_keyframe > model.clip.keyframes.size() - 1)
			{
       				debug.current_keyframe = 0;
			}
		}
	}
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    //case WM_PAINT:
    //    {
    //        PAINTSTRUCT ps;
    //        HDC hdc = BeginPaint(hWnd, &ps);
    //        // TODO: Add any drawing code that uses hdc here...
    //        EndPaint(hWnd, &ps);
    //    }
    //    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}