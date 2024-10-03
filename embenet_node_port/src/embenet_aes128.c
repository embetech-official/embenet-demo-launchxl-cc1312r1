/**
@file
@license   Commercial
@copyright (c) 2023 EMBETECH SP. Z O.O. All rights reserved.
@version   1.1.4417
@purpose   embeNET Node port
@brief     Implementation of AES-128 interface for embeNET Node
*/

#include "embenet_aes128.h"

#include "embenet_critical_section.h"

// clang-format off
#include <ti_drivers_config.h>
#include <ti/drivers/AESECB.h>
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
#include <ti/drivers/power/PowerCC26X2.h>
// clang-format on

#include <stdlib.h>
#include <string.h>


static AESECB_Handle handle;
static CryptoKey     cryptoKey;
static uint8_t       keyStorage[16U];

extern __attribute__((noreturn)) void EXPECT_OnAbortHandler(char const* why, char const* file, int line);

void EMBENET_AES128_Init(void) {
    Power_setDependency(PowerCC26XX_PERIPH_CRYPTO);

    AESECB_init();

    AESECB_Params params;
    AESECB_Params_init(&params);
    params.returnBehavior = AESECB_RETURN_BEHAVIOR_POLLING;
    handle                = AESECB_open(EMBENET_AES, &params);

    if (handle == NULL) {
        EXPECT_OnAbortHandler("AES malfunction", __FILE__, __LINE__);
    }

    CryptoKeyPlaintext_initKey(&cryptoKey, keyStorage, sizeof(keyStorage));
}

void EMBENET_AES128_Deinit(void) {
    AESECB_close(handle);
    Power_releaseDependency(PowerCC26XX_PERIPH_CRYPTO);

    memset(keyStorage, 0, sizeof(keyStorage));
}

void EMBENET_AES128_SetKey(uint8_t const key[16U]) {
    memcpy(keyStorage, key, sizeof(keyStorage));
}

void EMBENET_AES128_Encrypt(uint8_t data[16U]) {
    uint8_t plaintext[16U];

    memcpy(plaintext, data, sizeof(plaintext));

    AESECB_Operation operation;
    AESECB_Operation_init(&operation);

    operation.key         = &cryptoKey;
    operation.input       = plaintext;
    operation.output      = data;
    operation.inputLength = sizeof(plaintext);

    int_fast16_t encryptionResult = AESECB_oneStepEncrypt(handle, &operation);

    if (encryptionResult != AESECB_STATUS_SUCCESS) {
        // handle error
    }
}

void EMBENET_AES128_Decrypt(uint8_t data[16U]) {
    uint8_t ciphertext[16U];

    memcpy(ciphertext, data, sizeof(ciphertext));

    AESECB_Operation operation;
    AESECB_Operation_init(&operation);

    operation.key         = &cryptoKey;
    operation.input       = ciphertext;
    operation.output      = data;
    operation.inputLength = sizeof(ciphertext);

    int_fast16_t encryptionResult = AESECB_oneStepDecrypt(handle, &operation);

    if (encryptionResult != AESECB_STATUS_SUCCESS) {
        // handle error
    }
}
