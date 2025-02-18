#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "capture.h"

// Definitions taken from the Assetto Corsa custom shader pack OBS plugin
#define MAX_TEXTURES 63
#define NAME_LENGTH 48
#define DESCRIPTION_LENGTH 256

#define FLAG_TEXTURE_UNAVAILABLE (1 << 0)
#define FLAG_TEXTURE_TRANSPARENT (1 << 1)
#define FLAG_TEXTURE_SRGB (1 << 2)
#define FLAG_TEXTURE_MONOCHROME (1 << 3)
#define FLAG_TEXTURE_HDR (1 << 4)
#define FLAG_TEXTURE_USER_SIZE (1 << 7)
#define FLAG_TEXTURE_OVERRIDE_SIZE (1 << 8)

typedef struct
{
    uint32_t handle;
    uint32_t name_key;
    uint16_t width;
    uint16_t height;
    uint16_t needs_data;
    uint16_t flags;
    char name[NAME_LENGTH];
    char description[DESCRIPTION_LENGTH];
} accsp_source;

typedef struct
{
    uint32_t alive_counter;
    int32_t items_count;
    accsp_source items[MAX_TEXTURES];
} accsp_data;

static ID3D11Device *device = NULL;
static ID3D11DeviceContext *context = NULL;
static ID3D11Texture2D *sharedTex = NULL;
static ID3D11Texture2D *stagingTex = NULL;
static HANDLE hMap = NULL;
static accsp_data *accspHandle = NULL;
static accsp_source *accspSource = NULL;
static uint32_t lastHandle = NULL;

DLLEXPORT int initialize_capture(void)
{
    if (device || context || accspHandle || accspSource)
    {
        printf("Capture already initialized.\n");
        return -1;
    }

    HRESULT hr;

    // Create a basic D3D11 device.
    hr = D3D11CreateDevice(
        NULL,                        // use default adapter
        D3D_DRIVER_TYPE_HARDWARE,    // hardware device
        NULL,
        0,                           // optionally, set D3D11_CREATE_DEVICE_BGRA_SUPPORT
        NULL, 0,                     // default feature levels
        D3D11_SDK_VERSION,
        &device,
        NULL,
        &context);
    if (FAILED(hr))
    {
        printf("Failed to create D3D11 device (hr=0x%08X).\n", hr);
        return -2;
    }

    // Open the shared memory mapping created by the Assetto Corsa shader pack.
    hMap = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, L"Local\\AcTools.CSP.OBSTextures.v0");
    if (!hMap)
    {
        printf("Failed to open shared memory mapping.\n");
        return -3;
    }

    accspHandle = (accsp_data*) MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(accsp_data));
    if (!accspHandle)
    {
        printf("Failed to map shared memory.\n");
        CloseHandle(hMap);
        return -4;
    }

    accspHandle->alive_counter = 60; // Signal CSP that the interface is alive

    // Wait until at least one texture is available.
    printf("Waiting for a valid texture...\n");
    while (accspHandle->items_count <= 0)
    {
        Sleep(100);
        accspHandle->alive_counter = 60;
    }

    // Print all video sources
    for (int i = 0; i < accspHandle->items_count; ++i)
    {
        char isValid = accspHandle->items[i].handle != 0 && !(accspHandle->items[i].flags & FLAG_TEXTURE_UNAVAILABLE);
        printf("Source %d '%s' is %s\n", i, accspHandle->items[i].name, isValid ? "valid" : "invalid");
    }

    // Pick the first available texture (with nonzero handle and not flagged as unavailable).
    for (int i = 0; i < accspHandle->items_count; ++i)
    {
        if (accspHandle->items[i].handle != 0 && !(accspHandle->items[i].flags & FLAG_TEXTURE_UNAVAILABLE))
        {
            accspSource = &accspHandle->items[i];
            break;
        }
    }
    if (!accspSource) {
        printf("No valid texture found in shared memory.\n");
        UnmapViewOfFile(accspHandle);
        CloseHandle(hMap);
        return -5;
    }
    printf("Using texture '%s' (handle=0x%08X, size=%dx%d).\n",
           accspSource->name, accspSource->handle, accspSource->width, accspSource->height);
    
    accspSource->needs_data = 3; // Signal CSP that it should start filling the texture

    return 0;
}

DLLEXPORT int wait_new_frame(Frame *outFrame)
{
    if (!accspSource)
        return -1;

    accspSource->needs_data = 3; // Signal CSP that we want to get a new image

    // Wait until the texture is ready.
    while (accspSource->needs_data == 3)
    {
        Sleep(2);
    }

    return grab_current_frame(outFrame);
}

DLLEXPORT int grab_current_frame(Frame *outFrame)
{
    if (!device || !context || !accspHandle || !accspSource)
        return -1;
    
    HRESULT hr;

    accspHandle->alive_counter = 60; // Still alive, not sure if needed
    accspSource->needs_data = 3; // Signal CSP that we want to get a new image

    // Open the shared D3D11 texture resource.
    if (accspSource->handle != lastHandle)
    {
        lastHandle = accspSource->handle;
        if (sharedTex)
            sharedTex->lpVtbl->Release(sharedTex);

        hr = device->lpVtbl->OpenSharedResource(device,
                (HANDLE)(uintptr_t)lastHandle,
                &IID_ID3D11Texture2D,
                (void**)&sharedTex);
        if (FAILED(hr))
        {
            printf("Failed to open shared texture resource (hr=0x%08X).\n", hr);
            UnmapViewOfFile(accspHandle);
            CloseHandle(hMap);
            return -6;
        }
    }

    // Create staging texture for CPU access
    if (!stagingTex)
    {
        // Get the description of the shared texture.
        D3D11_TEXTURE2D_DESC texDesc;
        sharedTex->lpVtbl->GetDesc(sharedTex, &texDesc);
        printf("Shared texture format=0x%08X, width=%u, height=%u\n",
            texDesc.Format, texDesc.Width, texDesc.Height);

        // Create a staging texture with CPU read access.
        D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;

        hr = device->lpVtbl->CreateTexture2D(device, &stagingDesc, NULL, &stagingTex);
        if (FAILED(hr))
        {
            printf("Failed to create staging texture (hr=0x%08X).\n", hr);
            sharedTex->lpVtbl->Release(sharedTex);
            UnmapViewOfFile(accspHandle);
            CloseHandle(hMap);
            return -1;
        }
    }    
    
    // Copy the shared texture to the staging texture.
    context->lpVtbl->CopyResource(context, (ID3D11Resource*)stagingTex, (ID3D11Resource*)sharedTex);

    // Map the texture for reading.
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->lpVtbl->Map(context, (ID3D11Resource*)stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
        return -2;

    // Allocate a contiguous buffer for the frame. (Alternatively, you could
    // have a pre-allocated buffer and reuse it.)
    unsigned int width = outFrame->width = accspSource->width;
    unsigned int height = outFrame->height = accspSource->height;
    unsigned int bytesPerPixel = 4;
    unsigned int size = width * height * bytesPerPixel;
    unsigned char *buffer = (unsigned char*)malloc(size);
    if (!buffer)
    {
        context->lpVtbl->Unmap(context, (ID3D11Resource*)stagingTex, 0);
        return -3;
    }

    // Copy the data row by row (if the rowPitch != width * bytesPerPixel).
    if (mapped.RowPitch == width * 4)
    {
        memcpy(buffer, mapped.pData, size);
    }
    else
    {
        for (unsigned int y = 0; y < height; y++)
        {
            memcpy(buffer + y * width * bytesPerPixel,
                (unsigned char*)mapped.pData + y * mapped.RowPitch,
                width * bytesPerPixel);
        }
    }    

    // Unmap the texture.
    context->lpVtbl->Unmap(context, (ID3D11Resource*)stagingTex, 0);

    // Return the buffer in the outFrame.
    outFrame->data = buffer;
    return 0;
}

DLLEXPORT void shutdown_capture(void)
{
    // Release D3D11 objects, unmap shared memory, etc.
    if (sharedTex)
    {
        sharedTex->lpVtbl->Release(sharedTex);
        sharedTex = NULL;
    }
    if (stagingTex)
    {
        stagingTex->lpVtbl->Release(stagingTex);
        stagingTex = NULL;
    }
    if (accspSource)
    {
        accspSource->needs_data = 0;
        accspSource = NULL;
    }
    if (context)
    {
        context->lpVtbl->Release(context);
        context = NULL;
    }
    if (device)
    {
        device->lpVtbl->Release(device);
        device = NULL;
    }
    if (accspHandle)
    {
        UnmapViewOfFile(accspHandle);
        accspHandle = NULL;
    }
    if (hMap)
    {
        CloseHandle(hMap);
        hMap = NULL;
    }
}
