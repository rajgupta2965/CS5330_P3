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

# ─── Qt GUI executable (optional — comment out if Qt not installed) ────────
find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets QUIET)
endif()

if(Qt6_FOUND OR Qt5_FOUND)
    set(CMAKE_AUTOMOC ON)
    message(STATUS "Qt found — building GUI target")

    add_executable(Project3_GUI
            main_gui.cpp
            gui.cpp
            thresholding.cpp
            morphological.cpp
            segmentation.cpp
            features.cpp
            classifier.cpp
            eigenspace.cpp
            utilities.cpp
    )

    target_include_directories(Project3_GUI PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Project3_GUI PRIVATE ${OpenCV_LIBS})

    if(Qt6_FOUND)
        target_link_libraries(Project3_GUI PRIVATE Qt6::Widgets)
    else()
        target_link_libraries(Project3_GUI PRIVATE Qt5::Widgets)
    endif()
else()
    message(STATUS "Qt not found — GUI target skipped. Install with: brew install qt")
endif()