#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "image_bridge.h"

#define PIPELINE_MODE 1

const char* ABS_CAPTURE_PATH = "G:\\Embedded_project\\Embedded_project\\Example 11\\Examples\\Example011\\MSVC\\Images\\capture_001.png";
const char* ABS_INVERT_PATH = "G:\\Embedded_project\\Embedded_project\\Example 11\\Examples\\Example011\\MSVC\\Images\\invert_001.png";
const char* ABS_GREY_PATH = "G:\\Embedded_project\\Embedded_project\\Example 11\\Examples\\Example011\\MSVC\\Images\\greyscale_001.png";
const char* ABS_BLUR_PATH = "G:\\Embedded_project\\Embedded_project\\Example 11\\Examples\\Example011\\MSVC\\Images\\blur_001.png";
const char* ABS_EDGE_PATH = "G:\\Embedded_project\\Embedded_project\\Example 11\\Examples\\Example011\\MSVC\\Images\\edge_detection_001.png";

static void vCameraTask(void* pvParameters);
static void InverseImageTask(void* params);
static void vBlurTask(void* params);
static void vEdgeTask(void* params);
static void vGrayTask(void* params);

static QueueHandle_t xQueue1 = NULL;
static QueueHandle_t xQueue2 = NULL;
static QueueHandle_t xQueue3 = NULL;
static QueueHandle_t xQueue4 = NULL;
static QueueHandle_t xQueue5 = NULL;

int main(void)
{
    xQueue1 = xQueueCreate(5, sizeof(uint32_t));
    xQueue2 = xQueueCreate(5, sizeof(uint32_t));
    xQueue3 = xQueueCreate(5, sizeof(uint32_t));
    xQueue4 = xQueueCreate(5, sizeof(uint32_t));
    xQueue5 = xQueueCreate(5, sizeof(uint32_t));

    xTaskCreate(vCameraTask, "CameraTask", 256, NULL, 1, NULL);
    xTaskCreate(InverseImageTask, "InverseImageTask", 256, NULL, 1, NULL);
    xTaskCreate(vBlurTask, "BlurTask", 256, NULL, 1, NULL);
    xTaskCreate(vEdgeTask, "EdgeTask", 256, NULL, 1, NULL);
    xTaskCreate(vGrayTask, "GrayTask", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    for (;;);
    return 0;
}

#if (PIPELINE_MODE == 1)

static void vCameraTask(void* pvParameters)
{
    uint32_t ulSignal = 1;
    for (;;) {
        printf("Capturing image to: %s\n", ABS_CAPTURE_PATH);
        cam_capture_and_save(ABS_CAPTURE_PATH);
        xQueueSend(xQueue1, &ulSignal, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void InverseImageTask(void* params)
{
    uint32_t ulReceivedSignal;
    uint32_t ulSignalToSend = 1;

    for (;;) {
        if (xQueueReceive(xQueue1, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Inversing image...\n");
            int res = img_invert_process(ABS_CAPTURE_PATH, ABS_INVERT_PATH);
            if (res == 0) {
                printf("Inversion Success!\n");
                xQueueSend(xQueue2, &ulSignalToSend, portMAX_DELAY);
            }
            else {
                printf("Failed ya Man\n");
            }
        }
    }
}

static void vGrayTask(void* params)
{
    uint32_t ulReceivedSignal;
    uint32_t ulSignalToSend = 1;

    for (;;)
    {
        if (xQueueReceive(xQueue2, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Grayscaling image...\n");
            int res = img_grayscale_process(ABS_INVERT_PATH, ABS_GREY_PATH);
            if (res == 0) {
                printf("Grayscale Success!\n");
                xQueueSend(xQueue3, &ulSignalToSend, portMAX_DELAY);
            }
            else {
                printf("Failed ya Man\n");
            }
        }
    }
}

static void vBlurTask(void* params)
{
    uint32_t ulReceivedSignal;
    uint32_t ulSignalToSend = 1;

    for (;;)
    {
        if (xQueueReceive(xQueue3, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Blur image ....\n");
            int res = img_blur_process(ABS_GREY_PATH, ABS_BLUR_PATH);
            if (res == 0) {
                printf("Blur Success!\n");
                xQueueSend(xQueue4, &ulSignalToSend, portMAX_DELAY);
            }
            else {
                printf("Failed ya Man\n");
            }
        }
    }
}

static void vEdgeTask(void* params)
{
    uint32_t ulReceivedSignal;

    for (;;)
    {
        if (xQueueReceive(xQueue4, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Edge image ....\n");
            int res = img_edge_process(ABS_BLUR_PATH, ABS_EDGE_PATH);
            if (res == 0) {
                printf("Edge Success!\n");
            }
            else {
                printf("Failed ya Man\n");
            }
        }
    }
}

#elif (PIPELINE_MODE == 2)

static void vCameraTask(void* pvParameters)
{
    uint32_t ulSignal = 1;
    for (;;) {
        printf("Capturing image to: %s\n", ABS_CAPTURE_PATH);
        cam_capture_and_save(ABS_CAPTURE_PATH);

        xQueueSend(xQueue1, &ulSignal, portMAX_DELAY);
        xQueueSend(xQueue1, &ulSignal, portMAX_DELAY);
        xQueueSend(xQueue1, &ulSignal, portMAX_DELAY);
        xQueueSend(xQueue1, &ulSignal, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void InverseImageTask(void* params)
{
    uint32_t ulReceivedSignal;
    for (;;) {
        if (xQueueReceive(xQueue1, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Inversing image (Parallel)...\n");
            int res = img_invert_process(ABS_CAPTURE_PATH, ABS_INVERT_PATH);
            if (res == 0) printf("Inversion Success!\n");
            else printf("Failed ya Man\n");
        }
    }
}

static void vGrayTask(void* params)
{
    uint32_t ulReceivedSignal;
    for (;;)
    {
        if (xQueueReceive(xQueue1, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Grayscaling image (Parallel)...\n");
            int res = img_grayscale_process(ABS_CAPTURE_PATH, ABS_GREY_PATH);
            if (res == 0) printf("Grayscale Success!\n");
            else printf("Failed ya Man\n");
        }
    }
}

static void vBlurTask(void* params)
{
    uint32_t ulReceivedSignal;
    for (;;)
    {
        if (xQueueReceive(xQueue1, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Blur image (Parallel)....\n");
            int res = img_blur_process(ABS_CAPTURE_PATH, ABS_BLUR_PATH);
            if (res == 0) printf("Blur Success!\n");
            else printf("Failed ya Man\n");
        }
    }
}

static void vEdgeTask(void* params)
{
    uint32_t ulReceivedSignal;
    for (;;)
    {
        if (xQueueReceive(xQueue1, &ulReceivedSignal, portMAX_DELAY) == pdPASS)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("Edge image (Parallel)....\n");
            int res = img_edge_process(ABS_CAPTURE_PATH, ABS_EDGE_PATH);
            if (res == 0) printf("Edge Success!\n");
            else printf("Failed ya Man\n");
        }
    }
}

#endif