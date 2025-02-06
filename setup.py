# setup.py
from skbuild import setup
from setuptools import Extension
from Cython.Build import cythonize
import os

project_root = os.path.abspath(os.path.dirname(__file__))
lib_dir = os.path.join(project_root, "_skbuild", "win-amd64-3.11", "cmake-install", "lib")
include_dir = os.path.join(project_root)

# Define the extension module that will link against your C library.
extensions = cythonize([
   Extension(
       "capture_wrapper",
       sources=["capture_wrapper.pyx"],
       include_dirs=[include_dir],
       libraries=["accapture"],  # link with the capture library built by CMake
       library_dirs=[lib_dir],
   )
])

setup(
    name="accapture",
    ext_modules=extensions,
    version="0.1",
    description="A Python wrapper for capturing frames from Assetto Corsa textures",
    author="Laurenz Fussenegger",
    author_email="fussenegger@student.tugraz.at",
    cmake_install_dir=".",
)