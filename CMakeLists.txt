cmake_minimum_required(VERSION 3.16)
project(Project3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Find OpenCV (needs dnn module for CNN embedding) ───────────────────────
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio imgcodecs dnn)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")

# ─── Main executable ───────────────────────────────────────────────────────
add_executable(Project3
        main.cpp
        thresholding.cpp
        morphological.cpp
        segmentation.cpp
        features.cpp
        classifier.cpp
        eigenspace.cpp
        utilities.cpp
)

target_include_directories(Project3 PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(Project3 PRIVATE ${OpenCV_LIBS})