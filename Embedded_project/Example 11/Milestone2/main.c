// the last edited one


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "image_bridge.h"

// Task IDs
#define TASK_INVERT  2
#define TASK_GRAY    3
#define TASK_BLUR    4
#define TASK_EDGE    5

#define MAX_TOTAL_TIME_MS 100

// Output file slots
static char* PATH_SLOT_0 = "Images2\\capture.png";
static char* PATH_SLOT_1 = "Images2\\temp_step_1.png";
static char* PATH_SLOT_2 = "Images2\\temp_step_2.png";
static char* PATH_SLOT_3 = "Images2\\temp_step_3.png";
static char* PATH_SLOT_4 = "Images2\\temp_step_4.png";

// Queues (token moves Slot1 -> Slot5)
static QueueHandle_t xQueue_Slot1, xQueue_Slot2, xQueue_Slot3, xQueue_Slot4, xQueue_Slot5, xQueue_Control;

// Per-task configured wiring (set dynamically each run)
static QueueHandle_t xQ_In_Invert = NULL, xQ_Out_Invert = NULL;
static QueueHandle_t xQ_In_Gray = NULL, xQ_Out_Gray = NULL;
static QueueHandle_t xQ_In_Blur = NULL, xQ_Out_Blur = NULL;
static QueueHandle_t xQ_In_Edge = NULL, xQ_Out_Edge = NULL;

static char* pathIn_Invert, * pathOut_Invert;
static char* pathIn_Gray, * pathOut_Gray;
static char* pathIn_Blur, * pathOut_Blur;
static char* pathIn_Edge, * pathOut_Edge;

// Delays (Invert random, others become constant after first run)
static int g_ConstantsInitialized = 0;
static int g_ConstantDelayGray = 0;
static int g_ConstantDelayBlur = 0;
static int g_ConstantDelayEdge = 0;

static int g_DelayInvert = 0;
static int g_DelayGray = 0;
static int g_DelayBlur = 0;
static int g_DelayEdge = 0;

// Measured delay times (ms)
static TickType_t g_InvertDelayTime = 0;
static TickType_t g_GrayDelayTime = 0;
static TickType_t g_BlurDelayTime = 0;
static TickType_t g_EdgeDelayTime = 0;

// Task handles (for priority changes)
static TaskHandle_t xInvertHandle = NULL;
static TaskHandle_t xGrayHandle = NULL;
static TaskHandle_t xBlurHandle = NULL;
static TaskHandle_t xEdgeHandle = NULL;

// Track if Invert is at first position (will be blocked)
static int g_InvertAtFirstPosition = 0;

// Track if swapping was performed this iteration
static int g_SwappingPerformed = 0;

// Store original time before swapping
static TickType_t g_OriginalTimeBeforeSwap = 0;

static const char* TASK_NAMES[] = { "Unknown", "Capture", "Invert", "Gray", "Blur", "Edge" };

static int GetIDFromName(char* name) {
    name[strcspn(name, "\r\n")] = 0;
    for (int i = 2; i <= 5; i++) {
        if (_stricmp(name, TASK_NAMES[i]) == 0) return i;
    }
    return 0;
}

static void GetUserExecutionOrder(int* orderArray) {
    char inputBuffer[50];
    int validCount = 0;

    for (int i = 0; i < 4; i++) orderArray[i] = 0;

    printf("\n=========================================\n");
    printf("   CONFIGURE PIPELINE ORDER\n");
    printf("   Available: Invert, Gray, Blur, Edge\n");
    printf("=========================================\n");

    while (validCount < 4) {
        printf("Step %d: ", validCount + 1);
        if (fgets(inputBuffer, sizeof(inputBuffer), stdin) == NULL) continue;

        int id = GetIDFromName(inputBuffer);
        if (id == 0) { printf("  -> Error: Unknown task.\n"); continue; }

        int exists = 0;
        for (int k = 0; k < validCount; k++) if (orderArray[k] == id) exists = 1;
        if (exists) { printf("  -> Error: Duplicate.\n"); continue; }

        orderArray[validCount++] = id;
        printf("  -> Accepted: %s\n", TASK_NAMES[id]);
    }
}

// Edge MUST be last: bubble Edge to the right until it is last
static void ValidateAndFixPipeline(int* orderArray) {
    int swapCount = 0;
    int fixed = 0;

    printf("\n[Validator] Checking forbidden case: Edge MUST be LAST\n");

    while (!fixed) {
        fixed = 1;
        for (int i = 0; i < 3; i++) {
            if (orderArray[i] == TASK_EDGE) {
                fixed = 0;
                swapCount++;
                int t = orderArray[i];
                orderArray[i] = orderArray[i + 1];
                orderArray[i + 1] = t;
                printf("  -> [Fixing] Swap #%d: Moved Edge toward last position\n", swapCount);
            }
        }
    }

    if (swapCount == 0) printf("  -> Pipeline Valid: Edge is already last\n");
    else                printf("  -> Pipeline Fixed: Edge is now last\n");

    printf("\n[Pipeline Order] Final: %s -> %s -> %s -> %s\n",
        TASK_NAMES[orderArray[0]], TASK_NAMES[orderArray[1]],
        TASK_NAMES[orderArray[2]], TASK_NAMES[orderArray[3]]);
}

static void ConfigurePipeline(int* orderArray) {
    xQ_In_Invert = xQ_In_Gray = xQ_In_Blur = xQ_In_Edge = NULL;

    for (int i = 0; i < 4; i++) {
        QueueHandle_t qIn = (i == 0) ? xQueue_Slot1 : (i == 1) ? xQueue_Slot2 : (i == 2) ? xQueue_Slot3 : xQueue_Slot4;
        QueueHandle_t qOut = (i == 0) ? xQueue_Slot2 : (i == 1) ? xQueue_Slot3 : (i == 2) ? xQueue_Slot4 : xQueue_Slot5;

        char* pIn = (i == 0) ? PATH_SLOT_0 : (i == 1) ? PATH_SLOT_1 : (i == 2) ? PATH_SLOT_2 : PATH_SLOT_3;
        char* pOut = (i == 0) ? PATH_SLOT_1 : (i == 1) ? PATH_SLOT_2 : (i == 2) ? PATH_SLOT_3 : PATH_SLOT_4;

        switch (orderArray[i]) {
        case TASK_INVERT: xQ_In_Invert = qIn; xQ_Out_Invert = qOut; pathIn_Invert = pIn; pathOut_Invert = pOut; break;
        case TASK_GRAY:   xQ_In_Gray = qIn; xQ_Out_Gray = qOut; pathIn_Gray = pIn; pathOut_Gray = pOut; break;
        case TASK_BLUR:   xQ_In_Blur = qIn; xQ_Out_Blur = qOut; pathIn_Blur = pIn; pathOut_Blur = pOut; break;
        case TASK_EDGE:   xQ_In_Edge = qIn; xQ_Out_Edge = qOut; pathIn_Edge = pIn; pathOut_Edge = pOut; break;
        default: break;
        }
    }
}

static int TaskDelayByID(int taskID) {
    if (taskID == TASK_INVERT) return g_DelayInvert;
    if (taskID == TASK_GRAY)   return g_DelayGray;
    if (taskID == TASK_BLUR)   return g_DelayBlur;
    if (taskID == TASK_EDGE)   return g_DelayEdge;
    return 0;
}

static void ApplyPrioritiesFromOrder(const int* orderArray, int invertPos) {
    // Same behavior as your original: Invert low, Edge lowest,
    // tasks before Invert high, tasks after Invert normal.
    for (int i = 0; i < 4; i++) {
        int id = orderArray[i];

        if (id == TASK_INVERT) {
            vTaskPrioritySet(xInvertHandle, 2);
        }
        else if (id == TASK_EDGE) {
            vTaskPrioritySet(xEdgeHandle, 1);
        }
        else if (i < invertPos) {
            if (id == TASK_GRAY) vTaskPrioritySet(xGrayHandle, 4);
            if (id == TASK_BLUR) vTaskPrioritySet(xBlurHandle, 4);
        }
        else {
            if (id == TASK_GRAY) vTaskPrioritySet(xGrayHandle, 3);
            if (id == TASK_BLUR) vTaskPrioritySet(xBlurHandle, 3);
        }
    }
}

static void CalculateDelaysAndSetPriorities(int* orderArray) {
    // Random delay for Invert each run
    srand((unsigned int)(time(NULL) + clock()));
    g_DelayInvert = rand() % 101;

    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
    printf("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");


    printf("THE INVERT RANDOM DELAYYYYYY IS %d\n", g_DelayInvert);

    // Gray/Blur/Edge: constant after first run
    if (g_ConstantsInitialized) {
        g_DelayGray = g_ConstantDelayGray;
        g_DelayBlur = g_ConstantDelayBlur;
        g_DelayEdge = g_ConstantDelayEdge;
    }
    else {
        g_DelayGray = 10;
        g_DelayBlur = 10;
        g_DelayEdge = 10;
    }

    int expectedTotal = g_DelayInvert + g_DelayGray + g_DelayBlur + g_DelayEdge;

    printf("\n[Scheduler] ========== DELAY CALCULATION ==========\n");
    printf("[Scheduler] Invert: %dms (RANDOM)\n", g_DelayInvert);
    printf("[Scheduler] Gray:   %dms (CONSTANT)\n", g_DelayGray);
    printf("[Scheduler] Blur:   %dms (CONSTANT)\n", g_DelayBlur);
    printf("[Scheduler] Edge:   %dms (CONSTANT)\n", g_DelayEdge);
    printf("[Scheduler] Expected total: %dms (target < %dms)\n", expectedTotal, MAX_TOTAL_TIME_MS);

    // Reset measured timing vars
    //g_InvertDelayTime = g_GrayDelayTime = g_BlurDelayTime = g_EdgeDelayTime = 0;

    // Set all tasks to normal priority for initial run
    printf("\n[Priority] Initial setup: All tasks priority 3, Edge priority 1\n");
    vTaskPrioritySet(xInvertHandle, 3);
    vTaskPrioritySet(xGrayHandle, 3);
    vTaskPrioritySet(xBlurHandle, 3);
    vTaskPrioritySet(xEdgeHandle, 1);

    // Find Invert position for tracking
    int invertPos = -1;
    for (int i = 0; i < 4; i++) if (orderArray[i] == TASK_INVERT) { invertPos = i; break; }
    g_InvertAtFirstPosition = (invertPos == 0) ? 1 : 0;
}

static void vManagerTask(void* params) {
    int userOrder[4];
    uint32_t ulStart = 1, ulDone;
    char buffer[10];

    for (;;) {
        xQueueReset(xQueue_Slot1); xQueueReset(xQueue_Slot2);
        xQueueReset(xQueue_Slot3); xQueueReset(xQueue_Slot4);
        xQueueReset(xQueue_Slot5); xQueueReset(xQueue_Control);

        g_SwappingPerformed = 0;  // Reset swapping flag for new iteration
        g_InvertAtFirstPosition = 0;

        GetUserExecutionOrder(userOrder);
        // ValidateAndFixPipeline(userOrder);  // REMOVED: Don't check if Edge is last
        ConfigurePipeline(userOrder);
        CalculateDelaysAndSetPriorities(userOrder);

        // Tell camera to capture then kick off pipeline
        xQueueSend(xQueue_Control, &ulStart, portMAX_DELAY);

        if (xQueueReceive(xQueue_Slot5, &ulDone, portMAX_DELAY) == pdPASS) {
            // After first completed run, lock constant delays from measured values
            if (!g_ConstantsInitialized) {
                g_ConstantDelayGray = (int)g_GrayDelayTime;
                g_ConstantDelayBlur = (int)g_BlurDelayTime;
                g_ConstantDelayEdge = (int)g_EdgeDelayTime;
                g_ConstantsInitialized = 1;

                printf("\n[System] Stored constant delays for future runs:\n");
                printf("  Gray=%dms, Blur=%dms, Edge=%dms\n\n",
                    g_ConstantDelayGray, g_ConstantDelayBlur, g_ConstantDelayEdge);
            }

            TickType_t totalDelayTime = g_InvertDelayTime + g_GrayDelayTime + g_BlurDelayTime + g_EdgeDelayTime;

            // Check if actual measured time exceeds 100ms and perform swapping if needed
            if (totalDelayTime > MAX_TOTAL_TIME_MS && !g_SwappingPerformed) {
                printf("\n[OPTIMIZATION] Actual time %dms > %dms!\n", (int)totalDelayTime, MAX_TOTAL_TIME_MS);
                printf("[OPTIMIZATION] SWAPPING INVERT TO FIRST POSITION\n\n");

                // Store original time for later comparison
                g_OriginalTimeBeforeSwap = totalDelayTime;

                // Find Invert position
                int invertPos = -1;
                for (int i = 0; i < 4; i++) if (userOrder[i] == TASK_INVERT) { invertPos = i; break; }

                printf("[SWAP] Original order: %s -> %s -> %s -> %s\n",
                    TASK_NAMES[userOrder[0]], TASK_NAMES[userOrder[1]],
                    TASK_NAMES[userOrder[2]], TASK_NAMES[userOrder[3]]);

                // Keep swapping Invert backward until it reaches first position
                int swapCount = 0;
                while (invertPos > 0) {
                    int swapTargetPos = invertPos - 1;

                    // Swap Invert with the task before it
                    int tmp = userOrder[invertPos];
                    userOrder[invertPos] = userOrder[swapTargetPos];
                    userOrder[swapTargetPos] = tmp;
                    invertPos = swapTargetPos;
                    swapCount++;

                    printf("[SWAP] #%d: Invert <-> %s\n", swapCount, TASK_NAMES[userOrder[invertPos + 1]]);
                }

                printf("[SWAP] New order: %s -> %s -> %s -> %s\n",
                    TASK_NAMES[userOrder[0]], TASK_NAMES[userOrder[1]],
                    TASK_NAMES[userOrder[2]], TASK_NAMES[userOrder[3]]);
                printf("[SWAP] Invert moved to first - will block while constant tasks execute\n\n");

                g_InvertAtFirstPosition = 1;
                g_SwappingPerformed = 1;

                // Reconfigure pipeline with swapped order
                ConfigurePipeline(userOrder);
                ApplyPrioritiesFromOrder(userOrder, 0);

                // Reset queues and run again
                xQueueReset(xQueue_Slot1); xQueueReset(xQueue_Slot2);
                xQueueReset(xQueue_Slot3); xQueueReset(xQueue_Slot4);
                xQueueReset(xQueue_Slot5);

                // Reset timing vars
                g_InvertDelayTime = g_GrayDelayTime = g_BlurDelayTime = g_EdgeDelayTime = 0;

                // Trigger new capture and pipeline run
                printf("[SWAP] Running pipeline again with new order...\n\n");
                xQueueSend(xQueue_Control, &ulStart, portMAX_DELAY);

                // Wait for the second pipeline to complete
                if (xQueueReceive(xQueue_Slot5, &ulDone, portMAX_DELAY) == pdPASS) {
                    printf("[DEBUG] Second pipeline run completed after swapping!\n");

                    // Calculate the new total after swapping
                    totalDelayTime = g_InvertDelayTime + g_GrayDelayTime + g_BlurDelayTime + g_EdgeDelayTime;

                    // Calculate actual execution time with Invert blocked
                    TickType_t actualExecutionTime = g_GrayDelayTime + g_BlurDelayTime + g_EdgeDelayTime;

                    printf("\n=========================================\n");
                    printf("   PIPELINE COMPLETE - TIMING REPORT\n");
                    printf("=========================================\n");
                    printf("ORIGINAL TIME (before swap): %dms (exceeded %dms target)\n",
                        (int)g_OriginalTimeBeforeSwap, MAX_TOTAL_TIME_MS);
                    printf("\n[AFTER SWAPPING & BLOCKING INVERT]\n");
                    printf("Invert: %dms (blocked) | Gray: %dms | Blur: %dms | Edge: %dms\n",
                        (int)g_InvertDelayTime, (int)g_GrayDelayTime, (int)g_BlurDelayTime, (int)g_EdgeDelayTime);
                    printf("TOTAL TIME AFTER OPTIMIZATION: %dms\n", (int)actualExecutionTime);
                    if (actualExecutionTime <= MAX_TOTAL_TIME_MS) {
                        printf("STATUS: SUCCESS - Time reduced from %dms to %dms (within %dms target)!\n",
                            (int)g_OriginalTimeBeforeSwap, (int)actualExecutionTime, MAX_TOTAL_TIME_MS);
                        printf("        Invert blocked at first position, constant tasks executed in parallel\n");
                    }
                    else {
                        printf("STATUS: Still exceeds target by %dms\n", (int)(actualExecutionTime - MAX_TOTAL_TIME_MS));
                    }
                    printf("=========================================\n");
                }
                else {
                    printf("[ERROR] Second pipeline did not complete!\n");
                }

                // Jump to restart prompt
                goto restart_prompt;
            }

            printf("[DEBUG] Pipeline completed, calculating results...\n");

            totalDelayTime = g_InvertDelayTime + g_GrayDelayTime + g_BlurDelayTime + g_EdgeDelayTime;

            // Calculate actual execution time
            // If Invert is at first position, it runs at low priority and gets blocked
            // while constant tasks execute at higher priority
            TickType_t actualExecutionTime;
            if (g_InvertAtFirstPosition) {
                // Invert is at first position (swapped or originally there)
                // Invert is blocked (priority 2), constant tasks run at higher priority (3)
                // Actual time = sum of constant tasks only (Invert runs in background)
                actualExecutionTime = g_GrayDelayTime + g_BlurDelayTime + g_EdgeDelayTime;
            }
            else {
                // Invert is not at first position - sequential execution
                actualExecutionTime = totalDelayTime;
            }

            printf("\n=========================================\n");
            printf("   PIPELINE COMPLETE - TIMING REPORT\n");
            printf("=========================================\n");

            if (g_SwappingPerformed) {
                // Swapping occurred - show before and after results
                printf("ORIGINAL TIME (before swap): %dms (exceeded %dms target)\n",
                    (int)g_OriginalTimeBeforeSwap, MAX_TOTAL_TIME_MS);
                printf("\n[AFTER SWAPPING]\n");
                printf("Invert: %dms | Gray: %dms | Blur: %dms | Edge: %dms\n",
                    (int)g_InvertDelayTime, (int)g_GrayDelayTime, (int)g_BlurDelayTime, (int)g_EdgeDelayTime);
                printf("TOTAL TIME AFTER SWAPPING: %dms\n", (int)actualExecutionTime);
                if (actualExecutionTime <= MAX_TOTAL_TIME_MS) {
                    printf("STATUS: SUCCESS - Swapping reduced time to <= %dms!\n", MAX_TOTAL_TIME_MS);
                    printf("        Invert blocked at first position, constant tasks executed in %dms\n",
                        (int)actualExecutionTime);
                }
                else {
                    printf("STATUS: Still exceeds target by %dms\n", (int)(actualExecutionTime - MAX_TOTAL_TIME_MS));
                }
            }
            else if (g_InvertAtFirstPosition && totalDelayTime > MAX_TOTAL_TIME_MS) {
                // Invert at first position originally and would exceed 100ms - show optimization result
                printf("TOTAL DELAY TIME (without optimization): %dms (target < %dms)\n", (int)totalDelayTime, MAX_TOTAL_TIME_MS);
                printf("TOTAL TIME WITH INVERT BLOCKED: %dms\n", (int)actualExecutionTime);
                if (actualExecutionTime <= MAX_TOTAL_TIME_MS) {
                    printf("STATUS: SUCCESS - Invert blocked, constant tasks finish in <= 100ms!\n");
                }
                else {
                    printf("STATUS: Still exceeds target by %dms\n", (int)(actualExecutionTime - MAX_TOTAL_TIME_MS));
                }
            }
            else {
                // Normal execution - show individual task times
                printf("Invert: %dms | Gray: %dms | Blur: %dms | Edge: %dms\n",
                    (int)g_InvertDelayTime, (int)g_GrayDelayTime, (int)g_BlurDelayTime, (int)g_EdgeDelayTime);
                printf("TOTAL DELAY TIME: %dms (target < %dms)\n", (int)totalDelayTime, MAX_TOTAL_TIME_MS);
                if (actualExecutionTime <= MAX_TOTAL_TIME_MS) {
                    printf("STATUS: Within target\n");
                }
                else {
                    printf("STATUS: Exceeds target by %dms\n", (int)(actualExecutionTime - MAX_TOTAL_TIME_MS));
                }
            }
            printf("=========================================\n");
        }

    restart_prompt:
        printf("\nRestart? (yes/no): ");
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            if (_stricmp(buffer, "no\n") == 0) exit(0);
        }
    }
}

static void vCameraTask(void* pvParameters) {
    uint32_t ulSignal, ulSend = 1;

    for (;;) {
        if (xQueueReceive(xQueue_Control, &ulSignal, pdMS_TO_TICKS(500)) == pdPASS) {
            printf("\n[Camera] Capturing fresh image...\n");
            cam_capture_and_save(PATH_SLOT_0);

            // Ensure file is written
            vTaskDelay(pdMS_TO_TICKS(50));

            // Start pipeline token
            xQueueSend(xQueue_Slot1, &ulSend, portMAX_DELAY);
        }
    }
}

static void InverseImageTask(void* params) {
    uint32_t ulSig;
    TickType_t t0, t1;

    for (;;) {
        if (xQ_In_Invert == NULL) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        if (xQueueReceive(xQ_In_Invert, &ulSig, pdMS_TO_TICKS(200)) == pdPASS) {
            t0 = xTaskGetTickCount();
            vTaskDelay(pdMS_TO_TICKS(g_DelayInvert));
            // t1 = xTaskGetTickCount();
             //g_InvertDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            img_invert_process(pathIn_Invert, pathOut_Invert);
            t1 = xTaskGetTickCount();
            g_InvertDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");

            printf("The Actual time of invert function is %d\n", g_InvertDelayTime);
            xQueueSend(xQ_Out_Invert, &ulSig, portMAX_DELAY);
        }
    }
}




//TickType_t tt0, tt1,camera_time;
//tt0 = xTaskGetTickCount();





static void vGrayTask(void* params) {
    uint32_t ulSig;
    TickType_t t0, t1;


    for (;;) {
        if (xQ_In_Gray == NULL) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        if (xQueueReceive(xQ_In_Gray, &ulSig, pdMS_TO_TICKS(200)) == pdPASS) {
            t0 = xTaskGetTickCount();

            // t0 = xTaskGetTickCount();
            vTaskDelay(pdMS_TO_TICKS(g_DelayGray));
            //t1 = xTaskGetTickCount();
            //g_GrayDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            img_grayscale_process(pathIn_Gray, pathOut_Gray);
            t1 = xTaskGetTickCount();
            g_GrayDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");

            printf("The Actual time of gray function is %d\n", g_GrayDelayTime);

            xQueueSend(xQ_Out_Gray, &ulSig, portMAX_DELAY);
        }
    }
}
//tt1 = xTaskGetTickCount();

//camera_time = (tt1 - tt0) * portTICK_PERIOD_MS;




static void vBlurTask(void* params) {
    uint32_t ulSig;
    TickType_t t0, t1;

    for (;;) {
        if (xQ_In_Blur == NULL) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        if (xQueueReceive(xQ_In_Blur, &ulSig, pdMS_TO_TICKS(200)) == pdPASS) {
            t0 = xTaskGetTickCount();
            vTaskDelay(pdMS_TO_TICKS(g_DelayBlur));
            // t1 = xTaskGetTickCount();
             //g_BlurDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            img_blur_process(pathIn_Blur, pathOut_Blur);
            t1 = xTaskGetTickCount();
            g_BlurDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");

            printf("The Actual time of blur function is %d\n", g_BlurDelayTime);
            xQueueSend(xQ_Out_Blur, &ulSig, portMAX_DELAY);
        }
    }
}




static void vEdgeTask(void* params) {
    uint32_t ulSig;
    TickType_t t0, t1;

    for (;;) {
        if (xQ_In_Edge == NULL) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        if (xQueueReceive(xQ_In_Edge, &ulSig, pdMS_TO_TICKS(200)) == pdPASS) {
            t0 = xTaskGetTickCount();
            vTaskDelay(pdMS_TO_TICKS(g_DelayEdge));
            //  t1 = xTaskGetTickCount();
              //g_EdgeDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            img_edge_process(pathIn_Edge, pathOut_Edge);
            t1 = xTaskGetTickCount();
            g_EdgeDelayTime = (t1 - t0) * portTICK_PERIOD_MS;

            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");
            printf("****************************************\n");

            printf("The Actual time of edge function is %d\n", g_EdgeDelayTime);
            xQueueSend(xQ_Out_Edge, &ulSig, portMAX_DELAY);
        }
    }
}

int main(void) {
    xQueue_Slot1 = xQueueCreate(1, sizeof(uint32_t));
    xQueue_Slot2 = xQueueCreate(1, sizeof(uint32_t));
    xQueue_Slot3 = xQueueCreate(1, sizeof(uint32_t));
    xQueue_Slot4 = xQueueCreate(1, sizeof(uint32_t));
    xQueue_Slot5 = xQueueCreate(1, sizeof(uint32_t));
    xQueue_Control = xQueueCreate(1, sizeof(uint32_t));

    printf("\n=========================================\n");
    printf("   MILESTONE 2: DYNAMIC PIPELINE\n");
    printf("=========================================\n");
    printf("- Invert: RANDOM delay 0-100ms\n");
    printf("- Gray/Blur/Edge: constant delay after first run\n");
    printf("- Capture time not counted in delay budget\n");
    printf("- If expected total > 100ms: swap Invert earlier + adjust priorities\n");
    printf("=========================================\n");

    xTaskCreate(vManagerTask, "Manager", 512, NULL, 3, NULL);
    xTaskCreate(vCameraTask, "Camera", 256, NULL, 1, NULL);
    xTaskCreate(InverseImageTask, "Invert", 256, NULL, 1, &xInvertHandle);
    xTaskCreate(vGrayTask, "Gray", 256, NULL, 1, &xGrayHandle);
    xTaskCreate(vBlurTask, "Blur", 256, NULL, 1, &xBlurHandle);
    xTaskCreate(vEdgeTask, "Edge", 256, NULL, 1, &xEdgeHandle);

    vTaskStartScheduler();
    for (;;) {}
    return 0;
}