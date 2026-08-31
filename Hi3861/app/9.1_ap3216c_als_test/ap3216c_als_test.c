#include <stdio.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "ap3216c_als.h"
#include "front_light_uart.h"

#define ALS_SAMPLE_INTERVAL_MS 500
#define ALS_TASK_STACK_SIZE (4 * 1024)
#define ALS_AVERAGE_SAMPLE_COUNT 3
#define LIGHT_STATE_CONFIRM_COUNT 3
#define LIGHT_ON_THRESHOLD 300
#define LIGHT_OFF_THRESHOLD 350

typedef enum {
    FRONT_LIGHT_OFF = 0,
    FRONT_LIGHT_ON = 1,
} FrontLightState;

static const char *FrontLightStateName(FrontLightState state)
{
    return state == FRONT_LIGHT_ON ? "ON" : "OFF";
}

static uint16_t UpdateAlsAverage(uint16_t sample, uint16_t samples[],
    uint8_t *sampleIndex, uint8_t *sampleCount)
{
    uint32_t sum = 0;
    uint8_t index;

    samples[*sampleIndex] = sample;
    *sampleIndex = (*sampleIndex + 1) % ALS_AVERAGE_SAMPLE_COUNT;
    if (*sampleCount < ALS_AVERAGE_SAMPLE_COUNT) {
        (*sampleCount)++;
    }

    for (index = 0; index < *sampleCount; index++) {
        sum += samples[index];
    }

    return (uint16_t)(sum / *sampleCount);
}

static uint8_t UpdateFrontLightDecision(uint16_t alsAverage, FrontLightState *lightState,
    FrontLightState *pendingState, uint8_t *confirmationCount)
{
    FrontLightState candidateState;

    if (alsAverage < LIGHT_ON_THRESHOLD) {
        candidateState = FRONT_LIGHT_ON;
    } else if (alsAverage > LIGHT_OFF_THRESHOLD) {
        candidateState = FRONT_LIGHT_OFF;
    } else {
        *confirmationCount = 0;
        return 0;
    }

    if (candidateState == *lightState) {
        *pendingState = *lightState;
        *confirmationCount = 0;
        return 0;
    }

    if (candidateState != *pendingState) {
        *pendingState = candidateState;
        *confirmationCount = 1;
    } else {
        (*confirmationCount)++;
    }

    if (*confirmationCount >= LIGHT_STATE_CONFIRM_COUNT) {
        *lightState = candidateState;
        *pendingState = candidateState;
        *confirmationCount = 0;
        return 1;
    }

    return 0;
}

static void AlsTestTask(void *argument)
{
    uint32_t result;
    uint32_t sampleNumber = 0;
    uint16_t alsRaw = 0;
    uint16_t alsAverage = 0;
    uint16_t alsSamples[ALS_AVERAGE_SAMPLE_COUNT] = {0};
    uint8_t sampleIndex = 0;
    uint8_t sampleCount = 0;
    uint8_t confirmationCount = 0;
    FrontLightState lightState = FRONT_LIGHT_OFF;
    FrontLightState pendingState = FRONT_LIGHT_OFF;

    (void)argument;

    result = Ap3216cAlsInit();
    if (result != 0) {
        printf("[AP3216C] init failed, error=0x%08x\r\n", result);
        return;
    }

    result = FrontLightUartInit();
    if (result != 0) {
        printf("[LIGHT] UART2 init failed, error=0x%08x\r\n", result);
        return;
    }

    result = FrontLightUartSend(0);
    if (result != 0) {
        printf("[LIGHT] initial LIGHT_OFF send failed\r\n");
        return;
    }

    printf("[AP3216C] ALS test started, interval=%d ms\r\n", ALS_SAMPLE_INTERVAL_MS);
    printf("[LIGHT] initial=OFF, on_below=%d, off_above=%d, confirm=%d\r\n",
        LIGHT_ON_THRESHOLD, LIGHT_OFF_THRESHOLD, LIGHT_STATE_CONFIRM_COUNT);

    while (1) {
        result = Ap3216cAlsRead(&alsRaw);
        if (result == 0) {
            sampleNumber++;
            alsAverage = UpdateAlsAverage(alsRaw, alsSamples, &sampleIndex, &sampleCount);

            if (sampleCount < ALS_AVERAGE_SAMPLE_COUNT) {
                printf("[AP3216C] sample=%u, als_raw=%u, averaging=%u/%u, light=%s\r\n",
                    (unsigned int)sampleNumber, (unsigned int)alsRaw,
                    (unsigned int)sampleCount, (unsigned int)ALS_AVERAGE_SAMPLE_COUNT,
                    FrontLightStateName(lightState));
            } else {
                if (UpdateFrontLightDecision(alsAverage, &lightState, &pendingState,
                    &confirmationCount)) {
                    result = FrontLightUartSend(lightState == FRONT_LIGHT_ON);
                    if (result == 0) {
                        printf("[LIGHT] LIGHT_%s sent, als_average=%u\r\n",
                            FrontLightStateName(lightState), (unsigned int)alsAverage);
                    } else {
                        printf("[LIGHT] LIGHT_%s send failed\r\n",
                            FrontLightStateName(lightState));
                        lightState = lightState == FRONT_LIGHT_ON ?
                            FRONT_LIGHT_OFF : FRONT_LIGHT_ON;
                        confirmationCount = LIGHT_STATE_CONFIRM_COUNT - 1;
                    }
                }
                printf("[AP3216C] sample=%u, als_raw=%u, als_average=%u, light=%s\r\n",
                    (unsigned int)sampleNumber, (unsigned int)alsRaw,
                    (unsigned int)alsAverage, FrontLightStateName(lightState));
            }
        } else {
            printf("[AP3216C] read failed, error=0x%08x\r\n", result);
            confirmationCount = 0;
        }

        usleep(ALS_SAMPLE_INTERVAL_MS * 1000);
    }
}

static void Ap3216cAlsTestEntry(void)
{
    osThreadAttr_t attributes = {0};
    osThreadId_t threadId;

    attributes.name = "ap3216c_als_test";
    attributes.stack_size = ALS_TASK_STACK_SIZE;
    attributes.priority = osPriorityNormal;

    threadId = osThreadNew(AlsTestTask, NULL, &attributes);
    if (threadId == NULL) {
        printf("[AP3216C] failed to create ALS test task\r\n");
    }
}

APP_FEATURE_INIT(Ap3216cAlsTestEntry);
