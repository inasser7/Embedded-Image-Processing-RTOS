# FreeRTOS Embedded Image Processing Pipeline

This project demonstrates real-time image processing using FreeRTOS with dynamic task scheduling and priority management.

## Project Overview

An embedded system project that implements image processing pipelines using FreeRTOS tasks. The system captures images and processes them through multiple stages including inversion, grayscale conversion, blurring, and edge detection.

## Milestones

### Milestone 1: Basic Pipeline Implementation
- **Location**: `Embedded_project/Example 11/Milestone 1/`
- **Features**:
  - Sequential image processing pipeline
  - Parallel image processing mode
  - FreeRTOS queue-based communication
  - Image processing tasks: Camera Capture, Invert, Grayscale, Blur, Edge Detection

**Pipeline Modes:**
- Mode 1 (Sequential): Tasks execute in sequence with queue signaling
- Mode 2 (Parallel): All processing tasks receive signal simultaneously

### Milestone 2: Dynamic Pipeline Optimization
- **Location**: `Embedded_project/Example 11/Milestone2/`
- **Features**:
  - User-configurable task execution order
  - Dynamic priority adjustment
  - Automatic pipeline optimization
  - Performance monitoring and reporting
  - Adaptive task swapping when execution exceeds 100ms target

**Key Features:**
- **Random Invert Delay**: Invert task has random delay (0-100ms) each run
- **Constant Task Delays**: Gray, Blur, and Edge tasks maintain constant delays after first run
- **Automatic Optimization**: System automatically swaps task order and adjusts priorities when total time exceeds 100ms
- **Priority-Based Blocking**: Invert task can be blocked at first position to allow constant tasks to execute in parallel

## System Requirements

- Windows OS
- Visual Studio (MSVC compiler)
- FreeRTOS library
- OpenCV library (for image processing)

## Project Structure

```
Embedded_project/
├── README.md
├── Example 11/
│   ├── Milestone 1/
│   │   └── main.c          # Basic pipeline implementation
│   ├── Milestone2/
│   │   └── main.c          # Dynamic pipeline with optimization
│   ├── FreeRTOS_Source/    # FreeRTOS kernel source
│   ├── Supporting_Functions/
│   └── Examples/
│       └── Example011/
│           └── MSVC/       # Visual Studio project files
└── opencv/                 # OpenCV library
```

## How to Build and Run

### Milestone 1:
1. Open `RTOSDemo.sln` in Visual Studio
2. Set `PIPELINE_MODE` to 1 (sequential) or 2 (parallel) in main.c
3. Build and run the project
4. Images will be saved in the configured output paths

### Milestone 2:
1. Copy `main.c` from Milestone2 folder to the MSVC project
2. Build and run in Visual Studio
3. Enter desired task execution order when prompted
4. System will automatically optimize if execution time exceeds 100ms
5. View performance reports after each pipeline run

## Task Descriptions

- **Camera Task**: Captures images from camera/file
- **Invert Task**: Inverts image colors (random delay 0-100ms in Milestone 2)
- **Grayscale Task**: Converts image to grayscale
- **Blur Task**: Applies blur filter to image
- **Edge Detection Task**: Detects edges in the image

## Performance Optimization (Milestone 2)

The system implements intelligent optimization:
1. Measures execution time of each task
2. If total time > 100ms, automatically swaps task order
3. Moves Invert task to first position with low priority
4. Allows constant-delay tasks to execute at higher priority
5. Blocks Invert task execution while constant tasks complete
6. Reports performance before and after optimization

## Authors

Embedded Systems Project - ENAU501

## License

See LICENSE file for details.
