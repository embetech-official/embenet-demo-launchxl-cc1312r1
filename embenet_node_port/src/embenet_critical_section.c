/**
@file
@license   Commercial
@copyright (c) 2023 EMBETECH SP. Z O.O. All rights reserved.
@version   1.1.4417
@purpose   embeNET Node port
@brief     Implementation of critical section interface for embeNET Node
*/

#include "embenet_critical_section.h"

// clang-format off
#include <ti_drivers_config.h>
#include DeviceFamily_constructPath(driverlib/cpu.h)
// clang-format on
#include <ti/drivers/dpl/HwiP.h>
#include <stdint.h>

static int       irqNestCounter;
static uint32_t  previousIrqState;
static uintptr_t key;
void             EMBENET_CRITICAL_SECTION_Enter(void) {
    if (0 == irqNestCounter) {
        key = HwiP_disable();
        //        uint32_t irqState = CPUprimask();
        //        previousIrqState  = irqState;
        //        CPUcpsid();
    }
    ++irqNestCounter;
}

void EMBENET_CRITICAL_SECTION_Exit(void) {
    --irqNestCounter;
    if (irqNestCounter < 0) {
        irqNestCounter = 0;
    }
    if (0 == irqNestCounter && 0 == previousIrqState) {
        //        CPUcpsie();
        HwiP_restore(key);
    }
}
