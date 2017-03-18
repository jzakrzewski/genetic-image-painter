# Genetic Image Painter

Tries to replicate a given image using a genetic algorithm.
Project is for fun and demonstration.

## Image preparation
* Convert to grayscale
* Save as raw pixels (*.data in GIMP, *.raw in IrfanView)

## Usage
```monalisa <width> <height> path/to/image.data```

## Prerequisites
* [SFML 2 Libraries](https://www.sfml-dev.org/)

# Build
Tested only on linux with standard SFML 2 installation and GCC 5.4
```
mkdir build
cd build
cmake path/to/sources
cmake --build .
```

## To do
* improve performance
* ensure it builds at least with MSVC, GCC and Clang
* if filesystem not available, fallback to c++11/14 and use boost
* if no boost, disable additional checks and let it crash ;)
* multithreading
* load other kinds of images (jpg, bmp)
* multicolor images