# Milestone 1 - Basic Pipeline Implementation

## Overview
This milestone implements a basic image processing pipeline using FreeRTOS tasks and queues.

## Features
- Sequential pipeline mode (PIPELINE_MODE = 1)
- Parallel processing mode (PIPELINE_MODE = 2)
- Queue-based task synchronization
- Five processing stages: Capture, Invert, Grayscale, Blur, Edge Detection

## Implementation Details

### Sequential Mode (PIPELINE_MODE = 1)
Tasks execute in sequence:
1. Camera captures image
2. Invert processes captured image
3. Grayscale processes inverted image
4. Blur processes grayscale image
5. Edge detection processes blurred image

Each task waits for signal from previous task via queue.

### Parallel Mode (PIPELINE_MODE = 2)
All processing tasks receive signal simultaneously from camera task and process the captured image in parallel.

## Configuration
- Set `PIPELINE_MODE` to 1 or 2 in main.c
- Configure image paths for input/output
- Adjust task priorities and stack sizes as needed

## Results
Images are saved at each processing stage for visual verification of the pipeline.
