#ifndef CAPTURE_H
#define CAPTURE_H

#define DLLEXPORT __declspec(dllexport)

#ifdef __cplusplus
extern "C" {
#endif

// A simple struct to return image information.
typedef struct
{
    unsigned char *data; // pointer to pixel data in BGRA format
    unsigned int width;
    unsigned int height;
} Frame;

DLLEXPORT int initialize_capture(); // call once to setup D3D11 device etc.
DLLEXPORT void shutdown_capture();

DLLEXPORT int grab_current_frame(Frame *outFrame);
DLLEXPORT int wait_new_frame(Frame *outFrame);

#ifdef __cplusplus
}
#endif

#endif  // CAPTURE_H