# capture_wrapper.pyx
from libc.stdlib cimport free
from PIL import Image

cdef extern from "capture.h":
    ctypedef struct Frame:
        unsigned char *data
        unsigned int width
        unsigned int height

    int initialize_capture()
    int grab_current_frame(Frame *frame)
    int wait_new_frame(Frame *frame)
    void shutdown_capture()


__is_capture_initialized = False

def shutdown() -> None:
    global __is_capture_initialized
    shutdown_capture()
    __is_capture_initialized = False


def get_frame(wait_for_new_frame: bool = False) -> Image:
    global __is_capture_initialized
    cdef Frame frame
    cdef int ret

    if not __is_capture_initialized:
        ret = initialize_capture()
        if ret != 0:
            raise RuntimeError("Failed to initialize capture")
        __is_capture_initialized = True

    if wait_for_new_frame:
        ret = wait_new_frame(&frame)
    else:
        ret = grab_current_frame(&frame)
    if ret != 0:
        shutdown_capture()
        raise RuntimeError("Failed to grab frame")

    # Copy the data into a Python bytes object
    cdef int size = frame.width * frame.height * 4 # 4 bytes per pixel
    py_bytes = bytes((<char*>frame.data)[:size])
    free(frame.data)

    return Image.frombuffer("RGBA", (frame.width, frame.height), py_bytes, "raw")
