/*
    Save each frame from the Assetto Corsa shared texture to disk.
    This code:
      - Opens the shared memory mapping "Local\\AcTools.CSP.OBSTextures.v0"
      - Picks a valid texture (the first one found that isn’t flagged as unavailable)
      - Creates a D3D11 device and opens the shared texture via its handle
      - Every frame, copies the GPU texture to a CPU–readable staging texture
      - Saves the resulting image as a BMP file (named frame_00000.bmp, frame_00001.bmp, …)
*/

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// --- Shared memory definitions (same as in the OBS plugin) ---

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

struct accsp_source
{
    uint32_t handle;
    uint32_t name_key;
    uint16_t width;
    uint16_t height;
    uint16_t needs_data;
    uint16_t flags;
    char name[NAME_LENGTH];
    char description[DESCRIPTION_LENGTH];
};

struct accsp_data
{
    uint32_t alive_counter;
    int32_t items_count;
    struct accsp_source items[MAX_TEXTURES];
};

// --- Helper function to save a BMP file ---
// // This function supports both DXGI_FORMAT_B8G8R8A8_UNORM and DXGI_FORMAT_R8G8B8A8_UNORM.
// For R8G8B8A8, it converts the pixel order from RGBA to BGRA.
void SaveBMP(const char *filename, void *pData, UINT width, UINT height, UINT rowPitch, DXGI_FORMAT format)
{
    // Determine which conversion (if any) is needed.
    // We'll prepare a contiguous buffer in BGRA order.
    unsigned char *buffer = NULL;
    int needsFree = 0;

    if (format == DXGI_FORMAT_B8G8R8A8_UNORM) 
    {
        // The data is already in BGRA order.
        // If the row pitch exactly equals width*4, we can use it directly.
        if (rowPitch == width * 4)
        {
            buffer = pData;
        } 
        else
        {
            // Otherwise, copy row by row into a contiguous buffer.
            buffer = (unsigned char*)malloc(width * height * 4);
            if (!buffer)
            {
                printf("Failed to allocate memory for BMP conversion.\n");
                return;
            }
            needsFree = 1;
            for (UINT y = 0; y < height; y++)
            {
                memcpy(buffer + y * width * 4, (unsigned char*)pData + y * rowPitch, width * 4);
            }
        }
    }
    else if (format == DXGI_FORMAT_R8G8B8A8_UNORM) 
    {
        // The data is in RGBA order. We need to swap red and blue for BMP (BGRA).
        buffer = (unsigned char*)malloc(width * height * 4);
        if (!buffer)
        {
            printf("Failed to allocate memory for conversion.\n");
            return;
        }
        needsFree = 1;
        for (UINT y = 0; y < height; y++)
        {
            unsigned char *src = (unsigned char*)pData + y * rowPitch;
            unsigned char *dst = buffer + y * width * 4;
            for (UINT x = 0; x < width; x++)
            {
                // In source: bytes are [R, G, B, A]
                // In destination we want: [B, G, R, A]
                dst[4*x + 0] = src[4*x + 2]; // blue
                dst[4*x + 1] = src[4*x + 1]; // green
                dst[4*x + 2] = src[4*x + 0]; // red
                dst[4*x + 3] = src[4*x + 3]; // alpha
            }
        }
    }
    else
    {
        printf("Unsupported texture format for BMP save: 0x%08X\n", format);
        return;
    }

    // Prepare BMP file header structures.
#pragma pack(push, 1)
    typedef struct tagBITMAPFILEHEADER
    {
        WORD  bfType;
        DWORD bfSize;
        WORD  bfReserved1;
        WORD  bfReserved2;
        DWORD bfOffBits;
    } BITMAPFILEHEADER;
#pragma pack(pop)

    BITMAPFILEHEADER bfh;
    memset(&bfh, 0, sizeof(bfh));
    bfh.bfType = 0x4D42; // 'BM'

    BITMAPINFOHEADER bih;
    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = width;
    bih.biHeight = -(LONG)height;  // negative height for a top–down bitmap
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = width * height * 4; // 4 bytes per pixel

    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + bih.biSizeImage;

    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        printf("Failed to open file '%s' for writing.\n", filename);
        if (needsFree)
            free(buffer);
        return;
    }

    fwrite(&bfh, sizeof(bfh), 1, fp);
    fwrite(&bih, sizeof(bih), 1, fp);
    fwrite(buffer, 1, bih.biSizeImage, fp);
    fclose(fp);

    printf("Saved frame to %s\n", filename);
    if (needsFree)
        free(buffer);
}

// --- Main function ---
int main(void)
{
    HRESULT hr;
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;

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
        return -1;
    }

    // Open the shared memory mapping created by the Assetto Corsa shader pack.
    HANDLE hMap = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, L"Local\\AcTools.CSP.OBSTextures.v0");
    if (!hMap)
    {
        printf("Failed to open shared memory mapping.\n");
        return -1;
    }

    struct accsp_data *shared = (struct accsp_data*) MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(struct accsp_data));
    if (!shared)
    {
        printf("Failed to map shared memory.\n");
        CloseHandle(hMap);
        return -1;
    }

    shared->alive_counter = 60; // Signal CSP that we want to receive textures

    // Wait until at least one texture is available.
    printf("Waiting for a valid texture...\n");
    while (shared->items_count <= 0) {
        Sleep(100);
        shared->alive_counter = 60;
    }

    // Pick the first available texture (with nonzero handle and not flagged as unavailable).
    struct accsp_source *chosen = NULL;
    for (int i = 0; i < shared->items_count; ++i) {
        if (shared->items[i].handle != 0 && !(shared->items[i].flags & FLAG_TEXTURE_UNAVAILABLE)) {
            chosen = &shared->items[i];
            break;
        }
    }
    if (!chosen) {
        printf("No valid texture found in shared memory.\n");
        UnmapViewOfFile(shared);
        CloseHandle(hMap);
        return -1;
    }
    printf("Using texture '%s' (handle=0x%08X, size=%dx%d).\n",
           chosen->name, chosen->handle, chosen->width, chosen->height);

    // Open the shared D3D11 texture resource.
    // (The shader pack sends the texture handle as a uint32_t; cast it to HANDLE.)
    ID3D11Texture2D *sharedTex = NULL;
    hr = device->lpVtbl->OpenSharedResource(device,
            (HANDLE)(uintptr_t)chosen->handle,
            &IID_ID3D11Texture2D,
            (void**)&sharedTex);
    if (FAILED(hr)) {
        printf("Failed to open shared texture resource (hr=0x%08X).\n", hr);
        UnmapViewOfFile(shared);
        CloseHandle(hMap);
        return -1;
    }

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

    ID3D11Texture2D *stagingTex = NULL;
    hr = device->lpVtbl->CreateTexture2D(device, &stagingDesc, NULL, &stagingTex);
    if (FAILED(hr))
    {
        printf("Failed to create staging texture (hr=0x%08X).\n", hr);
        sharedTex->lpVtbl->Release(sharedTex);
        UnmapViewOfFile(shared);
        CloseHandle(hMap);
        return -1;
    }

    // Main loop: each iteration copies the texture and saves it as a BMP.
    int frame = 0;
    while (1)
    {        
        // Instruct the game to update the texture.
        chosen->needs_data = 3;
        shared->alive_counter = 60;  // Also keep signaling that we are alive

        // Copy the shared texture to the staging texture.
        context->lpVtbl->CopyResource(context, (ID3D11Resource*)stagingTex, (ID3D11Resource*)sharedTex);

        // Map the staging texture for reading.
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = context->lpVtbl->Map(context, (ID3D11Resource*)stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr))
        {
            printf("Failed to map staging texture (hr=0x%08X).\n", hr);
            break;
        }

        // Build a filename for this frame.
        char filename[256];
        sprintf(filename, "frame_%05d.bmp", frame++);

        // Save the image. (Assumes 32-bits per pixel, no pitch adjustment is needed if rowPitch == width*4.)
        // If rowPitch is larger than width*4, you might need to copy row-by-row.
        SaveBMP(filename, mapped.pData, texDesc.Width, texDesc.Height, mapped.RowPitch, texDesc.Format);

        context->lpVtbl->Unmap(context, (ID3D11Resource*)stagingTex, 0);

        // Wait a short time before capturing the next frame (e.g. ~30fps).
        Sleep(33);
    }

    // Clean up.
    stagingTex->lpVtbl->Release(stagingTex);
    sharedTex->lpVtbl->Release(sharedTex);
    context->lpVtbl->Release(context);
    device->lpVtbl->Release(device);
    UnmapViewOfFile(shared);
    CloseHandle(hMap);

    return 0;
}
