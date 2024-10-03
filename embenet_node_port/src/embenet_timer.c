/**
@file
@license   Commercial
@copyright (c) 2023 EMBETECH SP. Z O.O. All rights reserved.
@version   1.1.4417
@purpose   embeNET Node port
@brief     Implementation of Timer interface for the embeNET Node
*/

#include "embenet_timer.h"


// clang-format off
#include <ti_drivers_config.h>
#include <ti/drivers/timer/GPTimerCC26XX.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/prcm.h)
// clang-format on

typedef struct {
    EMBENET_TIMER_CompareCallback callback;
    void*                         context;
    GPTimerCC26XX_Handle          hTimer;
} EMBENET_TIMER_Descriptor;

static EMBENET_TIMER_Descriptor embenetTimerDescriptor;

enum {
    EMBENET_TIMER_MAX_COMPARE_DURATION   = 0x7FFFFFFF,
    EMBENET_TIMER_EVENT_GUARD_TIME_TICKS = 40, // Equivalent of 30us. Minimal duration between current time and scheduled event. If the
                                               // duration is smaller, the event handling routine will be triggered immediately.
};

// Converts ticks to microseconds
static inline uint32_t EMBENET_TIMER_TicksToUs(uint32_t ticks) {
    return (uint32_t)((uint64_t)ticks * 4 / 3);
}

// Converts microseconds to ticks
static inline uint32_t EMBENET_TIMER_UsToTicks(uint32_t time) {
    return (uint32_t)((uint64_t)time * 3 / 4);
}

void EMBENET_TIMER_ISR_Handler(GPTimerCC26XX_Handle handle, GPTimerCC26XX_IntMask interruptMask);

void EMBENET_TIMER_Init(EMBENET_TIMER_CompareCallback compareCallback, void* context) {
    EMBENET_TIMER_Deinit();


    embenetTimerDescriptor = (EMBENET_TIMER_Descriptor){.callback = compareCallback, .context = context};

    PRCMGPTimerClockDivisionSet(PRCM_CLOCK_DIV_64); // 48MHz clock gives 1,(3)us per tick, so 3 ticks equals 4us

    PRCMLoadSet(); // Apply PRCM configuration
    // Initialize the timer and register interrupt handler
    GPTimerCC26XX_Params params;
    GPTimerCC26XX_Params_init(&params);
    params.width          = GPT_CONFIG_32BIT;
    params.mode           = GPT_MODE_PERIODIC_UP;
    params.debugStallMode = GPTimerCC26XX_DEBUG_STALL_ON;


    embenetTimerDescriptor.hTimer = GPTimerCC26XX_open(CONFIG_GPTIMER_0, &params); // The GPTimer should have an alias EMBENET_GBTIMER but the configurator tool does not have a appropriate option
    if (NULL == embenetTimerDescriptor.hTimer) {
        while (1)
            ;
    }
    // timer will shorten the duration by 0,33us on every ~72 minutes
    GPTimerCC26XX_setLoadValue(embenetTimerDescriptor.hTimer, 3221225471 /* (2^32-1)*3/4=3221225471,25 */);
    GPTimerCC26XX_registerInterrupt(embenetTimerDescriptor.hTimer, EMBENET_TIMER_ISR_Handler, 0);

    // Start the timer
    GPTimerCC26XX_start(embenetTimerDescriptor.hTimer);
}

void EMBENET_TIMER_Deinit(void) {
    if (embenetTimerDescriptor.hTimer != NULL) {
        GPTimerCC26XX_stop(embenetTimerDescriptor.hTimer);
        GPTimerCC26XX_unregisterInterrupt(embenetTimerDescriptor.hTimer);
        GPTimerCC26XX_close(embenetTimerDescriptor.hTimer);
        embenetTimerDescriptor.hTimer = NULL;
    }
}

void EMBENET_TIMER_SetCompare(EMBENET_TimeUs compareValue) {
    EMBENET_TimeUs currentValue = GPTimerCC26XX_getFreeRunValue(embenetTimerDescriptor.hTimer);
    // All time manipulations are valid only on us
    compareValue = EMBENET_TIMER_UsToTicks(compareValue);

    // Check whether the next compare value is really near current value. If so,
    // forcefully trigger interrupt at once
    GPTimerCC26XX_enableInterrupt(embenetTimerDescriptor.hTimer, GPT_INT_MATCH);
    if ((EMBENET_TimeUs)(compareValue - EMBENET_TIMER_EVENT_GUARD_TIME_TICKS - currentValue) < (EMBENET_TimeUs)(EMBENET_TIMER_MAX_COMPARE_DURATION)) {
        GPTimerCC26XX_setMatchValue(embenetTimerDescriptor.hTimer, compareValue);
    } else {
        IntPendSet(embenetTimerDescriptor.hTimer->hwAttrs->intNum); // Yup. this is kinda awfull, however now we can use whatever timer we like (or configure externally)
    }
}

EMBENET_TimeUs EMBENET_TIMER_ReadCounter(void) {
    return EMBENET_TIMER_TicksToUs(GPTimerCC26XX_getFreeRunValue(embenetTimerDescriptor.hTimer));
}

EMBENET_TimeUs EMBENET_TIMER_GetMaxCompareDuration(void) {
    return (EMBENET_TimeUs)EMBENET_TIMER_MAX_COMPARE_DURATION;
}

void EMBENET_TIMER_ISR_Handler(GPTimerCC26XX_Handle handle, GPTimerCC26XX_IntMask interruptMask) {
    GPTimerCC26XX_disableInterrupt(handle, GPT_INT_MATCH);

    if (embenetTimerDescriptor.callback != NULL) {
        embenetTimerDescriptor.callback(embenetTimerDescriptor.context);
    }
}
