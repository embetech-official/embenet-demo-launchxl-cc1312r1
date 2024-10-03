/**
@file
@license   Commercial
@copyright (c) 2023 EMBETECH SP. Z O.O. All rights reserved.
@version   1.1.4417
@purpose   embeNET Node port
@brief     Implementation of EUI64 interface for embeNET Node
*/

#include "embenet_eui64.h"

// clang-format off
#include <ti_drivers_config.h>
#include DeviceFamily_constructPath(inc/hw_ccfg.h)
// clang-format on

#include <string.h>

uint64_t EMBENET_EUI64_Get(void) {
    uint64_t native;
    memcpy(&native,
           (void*)CCFG_O_IEEE_MAC_0,
           sizeof(native)); // CCFG_O_IEEE_MAC_0 and CCFG_O_IEEE_MAC_1 lies next
                            // to each other, so we can read both registers at
                            // the same time
    return native;
}
