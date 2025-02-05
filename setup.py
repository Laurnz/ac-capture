# setup.py
from skbuild import setup
from setuptools import Extension
from Cython.Build import cythonize
import os

# Define the extension module that will link against your C library.
extensions = cythonize([
    Extension(
        "capture_wrapper",
        sources=["capture_wrapper.pyx", "capture.c"],
        include_dirs=[os.getcwd()],
        libraries=["accapture"],  # link with the capture library built by CMake
        extra_compile_args=[],
    )
])

setup(
    name="accapture",
    ext_modules=extensions,
    version="0.1",
    description="A Python wrapper for capturing frames from Assetto Corsa textures",
    author="Laurenz Fussenegger",
    author_email="fussenegger@tugraz.at",
)
