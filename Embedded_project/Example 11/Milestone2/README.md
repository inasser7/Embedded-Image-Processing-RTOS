# Milestone 2 - Dynamic Pipeline Optimization

## Overview
Advanced pipeline implementation with dynamic task ordering, priority management, and automatic optimization.

## Key Features
- **User-Configurable Pipeline**: Enter desired task execution order at runtime
- **Dynamic Priority Management**: Automatic priority adjustment based on task position
- **Performance Monitoring**: Real-time execution time tracking for each task
- **Automatic Optimization**: System detects when execution exceeds 100ms and optimizes

## Task Characteristics

### Invert Task
- **Random Delay**: 0-100ms (changes each run)
- **Priority**: Low (2) to allow blocking when at first position
- **Behavior**: Can be blocked by higher priority tasks

### Constant Tasks (Gray, Blur, Edge)
- **First Run**: Initial 10ms delay for measurement
- **Subsequent Runs**: Use measured constant delay from first run
- **Priority**: Adjusted dynamically based on position relative to Invert

## Optimization Strategy

### When Total Time > 100ms:
1. **Task Swapping**: Moves Invert to first position
2. **Priority Adjustment**: 
   - Invert: Priority 2 (low, will be blocked)
   - Constant tasks: Priority 3 or 4 (higher, execute first)
3. **Blocking Mechanism**: Invert starts but is preempted by higher priority constant tasks
4. **Result**: Constant tasks complete in parallel while Invert is blocked
5. **Total Time**: Reduced to sum of constant task times only

### Priority Scheme:
- **Edge Task**: Always priority 1 (lowest)
- **Invert Task**: Priority 2 when at first position
- **Tasks Before Invert**: Priority 4 (high)
- **Tasks After Invert**: Priority 3 (normal)

## User Interaction

### Pipeline Configuration:
```
Step 1: Invert
Step 2: Gray
Step 3: Blur
Step 4: Edge
```

The system validates and configures the pipeline based on user input.

## Performance Reporting

After each run, the system displays:
- Individual task execution times
- Total pipeline time
- Optimization status (if swapping occurred)
- Before/after comparison (if optimized)
- Success/failure relative to 100ms target

### Example Output:
```
=========================================
   PIPELINE COMPLETE - TIMING REPORT
=========================================
ORIGINAL TIME (before swap): 125ms (exceeded 100ms target)

[AFTER SWAPPING & BLOCKING INVERT]
Invert: 75ms (blocked) | Gray: 10ms | Blur: 10ms | Edge: 10ms
TOTAL TIME AFTER OPTIMIZATION: 30ms
STATUS: SUCCESS - Time reduced from 125ms to 30ms (within 100ms target)!
        Invert blocked at first position, constant tasks executed in parallel
=========================================
```

## Technical Implementation

### Queue Structure:
- 5 slot queues for token passing between tasks
- 1 control queue for manager-camera communication
- Dynamic queue assignment based on user-configured order

### Timing Measurement:
- Uses FreeRTOS tick count for accurate timing
- Measures actual execution time including delays
- Stores constant delays after first run

### Task Handles:
All task handles stored for dynamic priority adjustment:
- xInvertHandle
- xGrayHandle
- xBlurHandle
- xEdgeHandle

## Building and Running

1. Copy `main.c` to MSVC project directory
2. Ensure `image_bridge.h` and OpenCV are properly configured
3. Create `Images2/` directory for output files
4. Build and run in Visual Studio
5. Follow prompts to configure pipeline order
6. Observe automatic optimization if time exceeds 100ms

## Notes
- Camera capture time is NOT counted in the 100ms delay budget
- Constant task delays are locked after first successful run
- Invert delay randomizes each run to simulate variable workload
- System can restart pipeline with different task order
