# capture_wrapper.pyx
from libc.stdlib cimport free

cdef extern from "capture.h":
    ctypedef struct Frame:
        unsigned char *data
        unsigned int width
        unsigned int height
        unsigned int rowPitch

    int initialize_capture()
    int grab_frame(Frame *frame)
    void shutdown_capture()


def get_frame():
    cdef Frame frame
    cdef int ret

    ret = initialize_capture()
    if ret != 0:
        raise RuntimeError("Failed to initialize capture")

    ret = grab_frame(&frame)
    if ret != 0:
        shutdown_capture()
        raise RuntimeError("Failed to grab frame")

    # Calculate number of bytes (assuming 4 bytes per pixel)
    cdef int size = frame.width * frame.height * 4

    # Copy the data into a Python bytes object
    py_bytes = bytes((<char*>frame.data)[:size])

    # Free the memory allocated in C if needed:
    # For example, if you expose a free_frame_data() in your C code.
    # Otherwise, if you allocated with malloc in the same runtime, you can:
    # from libc.stdlib cimport free
    free(frame.data)
    
    shutdown_capture()
    return frame.width, frame.height, py_bytes