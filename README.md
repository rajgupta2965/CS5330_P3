# CS 5330 — Project 3: Real-time 2-D Object Recognition

## Author

## Overview
This project implements a complete 2-D object recognition pipeline that can
identify objects placed on a white surface in a translation-, scale-, and
rotation-invariant manner.  All four core stages are written **from scratch**
(no OpenCV built-in thresholding, morphological ops, connected components, or
moment functions).

## OS / IDE
- OS: [macOS / Windows / Linux]
- IDE: CLion (CMake-based build)
- Language: C++ 17

## Dependencies
- OpenCV 4.x (with `dnn` module for CNN embedding)

## Building
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)       # Linux / macOS
```
Or simply open the project folder in CLion and build.

## Running
```bash
# Webcam mode (default)
./object_recognition

# Single image
./object_recognition -i path/to/image.png

# Directory of images
./object_recognition -d Proj03Examples/

# Video file
./object_recognition -v recording.mp4
```

## Key Bindings
| Key | Action |
|-----|--------|
| `q` | Quit |
| `n` | **Train** — save hand-crafted features with a label |
| `c` | Toggle continuous classification ON/OFF |
| `1` | Classify using hand-crafted features (default) |
| `2` | Classify using eigenspace (PCA) embedding |
| `3` | Classify using CNN (ResNet18) embedding |
| `g` | Ground-truth evaluation mode (for confusion matrix) |
| `p` | Print confusion matrix |
| `r` | Reset confusion matrix |
| `w` | Save preprocessed ROI for eigenspace training |
| `b` | Build eigenspace from saved ROIs |
| `e` | Save CNN embedding for current object |
| `s` | Save screenshot |
| `+`/`-` | Adjust threshold bias |
| `space` | Pause / resume |
| `.`/`,` | Next / previous image (directory mode) |

## Workflow

### Hand-crafted features (Tasks 1–7)
1. Run with webcam or images.
2. Place an object; verify threshold / morphology / regions look good.
3. Press `n` to label and save the feature vector.
4. Repeat for all objects (≥ 5 categories, multiple orientations/positions).
5. New objects are automatically classified in real time.
6. Press `g` to record ground-truth for confusion matrix; press `p` to print it.

### Eigenspace embedding (Task 9 — PCA option)
1. Press `w` to save ROI images to `roi_training/` (repeat for each object).
2. Press `b` to build the eigenspace from saved ROIs.
3. Press `2` to switch to eigenspace classification.

### CNN embedding (Task 9 — ResNet18 option)
1. Place `or2d-normmodel-007.onnx` in the working directory.
2. Press `e` to save a CNN embedding for the current object.
3. Press `3` to switch to CNN classification.

## Files
| File | Description |
|------|-------------|
| `main.cpp` | Main loop, key handling, display |
| `thresholding.cpp/h` | **From scratch**: ISODATA adaptive thresholding |
| `morphological.cpp/h` | **From scratch**: erosion, dilation, opening, closing |
| `segmentation.cpp/h` | **From scratch**: two-pass connected components |
| `features.cpp/h` | **From scratch**: moments, orientation, Hu invariants |
| `classifier.cpp/h` | Scaled Euclidean nearest-neighbour classifier |
| `eigenspace.cpp/h` | PCA eigenspace build + classify |
| `utilities.cpp` | CNN embedding (ResNet18) — provided by instructor |
| `vision.h` | Common structs |

## Time Travel Days
[0 / specify if using any]

## Video Demo
[Insert link to demo video]