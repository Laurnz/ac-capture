from setuptools import Extension, setup
from Cython.Build import cythonize
import os

extensions = [
    Extension(
        "capture_wrapper",
        ["capture_wrapper.pyx", "capture.c"],
        include_dirs=[os.getcwd()],
        libraries=["d3d11"]
    )
]

setup(
    name="accapture",
    ext_modules=cythonize(extensions),
    version="0.2",
    description="A Python wrapper for capturing frames from Assetto Corsa textures",
    author="Laurenz Fussenegger",
    author_email="fussenegger@student.tugraz.at",
)