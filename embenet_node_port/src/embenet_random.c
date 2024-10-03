/**
@file
@license   Commercial
@copyright (c) 2023 EMBETECH SP. Z O.O. All rights reserved.
@version   1.1.4417
@purpose   embeNET Node port
@brief     Implementation of Random interface for embeNET Node
*/

#include "embenet_random.h"

// clang-format off
#include <ti_drivers_config.h>
#include <ti/drivers/TRNG.h>
#include <ti/drivers/power/PowerCC26X2.h>
// clang-format on
extern __attribute__((noreturn)) void EXPECT_OnAbortHandler(char const* why, char const* file, int line);

uint32_t EMBENET_RANDOM_Get(void) {
    Power_setDependency(PowerCC26XX_PERIPH_TRNG);
    TRNG_init();

    TRNG_Params params;
    TRNG_Params_init(&params);
    params.returnBehavior = TRNG_RETURN_BEHAVIOR_POLLING;

    TRNG_Handle handle = TRNG_open(EMBENET_TRNG, &params);
    if (NULL == handle) {
        EXPECT_OnAbortHandler("random generator malfunction", __FILE__, __LINE__);
    }
    uint32_t     v;
    int_fast16_t result;
    for (int i = 0; i != 100; ++i) {
        result = TRNG_getRandomBytes(handle, &v, sizeof(v));
    }


    TRNG_close(handle);
    Power_releaseDependency(PowerCC26XX_PERIPH_TRNG);

    if (result != TRNG_STATUS_SUCCESS) {
        EXPECT_OnAbortHandler("random generator malfunction", __FILE__, __LINE__);
    }

    return v;
}
