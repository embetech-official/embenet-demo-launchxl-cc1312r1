/**
@file
@license   Commercial
@copyright (c) 2023 EMBETECH SP. Z O.O. All rights reserved.
@version   1.1.4417
@purpose   embeNET Node port
@brief     Implementation of radio transceiver interface for embeNET Node
*/


#include "embenet_radio.h"

#include "embenet_critical_section.h"
// clang-format off
#include <ti_drivers_config.h>
#include <ti_radio_config.h>
#include <ti/drivers/rf/RF.h>
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
#include DeviceFamily_constructPath(driverlib/rf_prop_mailbox.h)
#include <ti/drivers/power/PowerCC26X2.h>
#include DeviceFamily_constructPath(inc/hw_rfc_dbell.h)
// clang-format on

#include <stdint.h>
#include <string.h>


enum {
    TX_BUFFER_LENGTH = EMBENET_RADIO_MAX_PSDU_LENGTH + 2 + 6, ///< added 2 extra bytes for packet length field and RSSI, the rest is dummy pool
    RX_BUFFER_LENGTH = EMBENET_RADIO_MAX_PSDU_LENGTH + 2 + 6, ///< added 2 extra bytes for packet length field and RSSI, the rest is dummy pool

    SYNCWORD_LENGTH = 2,     ///< Length of synchronization sequence [Bytes]
    PREAMBLE_LENGTH = 8,     ///< Preamble length [Bytes]
    RADIO_BITRATE   = 50000, ///< Modem bitrate [bps]

    TX_DELAY            = 795,  ///< time between tranmsission trigger and the appearance of preamble
    TX_START_CORRECTION = 550,  ///< after the transmission is trigerred, the start of frame callback is called 550us too early
    TX_END_CORRECTION   = 330,  ///< transmitter triggers the end of frame interrupts 330us earlier than receiver which triggers end of frame reception interrupt
    RX_START_CORRECTION = 1620, ///< transmission time of preamble and synchronization sequence plus extra time of interrupt delay


    BAND_START_FREQUENCY = 863100, ///< Start frequency of transceiver band in kHz
    BAND_CHANNEL_COUNT   = 69,
    CHANNEL_WIDTH        = 100,  ///< Width of single radio channel in kHz
    KHZ_TO_HZ_RATIO      = 1000, ///< Coversion ratio from MHz to Hz

    SENSITIVITY      = -100, /**< [dBm], does not consider as neighbor if RSSI will be lower */
    MIN_OUTPUT_POWER = 0,
    MAX_OUTPUT_POWER = 14,
};


static rfc_CMD_FS_OFF_t commonFsOff = {
    .commandNo                = CMD_FS_OFF,
    .startTrigger.triggerType = TRIG_NOW,
    .condition.rule           = COND_NEVER,
};

static uint8_t                radioRxBuffer[RX_BUFFER_LENGTH];
static rfc_dataEntryPointer_t rxItem = {
    .config.type  = DATA_ENTRY_TYPE_PTR,
    .config.lenSz = 0,
    .length       = sizeof(radioRxBuffer),
    .pNextEntry   = (uint8_t*)&rxItem,
    .pData        = radioRxBuffer,
    .status       = DATA_ENTRY_PENDING,
};

static dataQueue_t rxQueue     = {.pCurrEntry = (uint8_t*)&rxItem};
rfc_CMD_PROP_RX_t  rxChainDoRx = {
     .commandNo                = CMD_PROP_RX,
     .startTrigger.triggerType = TRIG_NOW,
     .condition.rule           = COND_NEVER,
     .pktConf.bUseCrc          = 1,
     .pktConf.bVarLen          = 1,
     .pktConf.endType          = 1,
     .rxConf.bAutoFlushIgnored = 1,
     .rxConf.bAutoFlushCrcErr  = 1,
     .rxConf.bIncludeHdr       = 1,
     .rxConf.bAppendRssi       = 1,
     .syncWord                 = 0x904E,
     .maxPktLen                = EMBENET_RADIO_MAX_PSDU_LENGTH,
     .endTrigger.triggerType   = TRIG_NEVER,
     .pQueue                   = &rxQueue,
     .pOutput                  = 0,
};

rfc_CMD_FS_t rxChainSetFs = {
    .commandNo                = CMD_FS,
    .pNextOp                  = (RF_Op*)&rxChainDoRx,
    .startTrigger.triggerType = TRIG_NOW,
    .condition.rule           = COND_STOP_ON_FALSE,
};

/**
 * @brief Sends data set by EMBENET_RADIO_TxEnable, then turns off FS
 */
static uint8_t           radioTxBuffer[TX_BUFFER_LENGTH];
static rfc_CMD_PROP_TX_t txChainDoTx = {
    .commandNo                = CMD_PROP_TX,
    .startTrigger.triggerType = TRIG_NOW,
    .condition.rule           = COND_NEVER,
    .pktConf.bFsOff           = 0,
    .pktConf.bUseCrc          = 0x1,
    .pktConf.bVarLen          = 0x1,
    .syncWord                 = 0x0000904E,
    .pPkt                     = radioTxBuffer,
};


/**
 * @brief Enables and sets FS - This is the beginning of Tx command chain
 * The frequency should be set with setChannel function.
 * setTxp shall be executed before posting this command.
 * On success executes txChainDoTx.
 * On failure skips to commonFsOff
 */
static rfc_CMD_FS_t txChainSetFs = {
    .commandNo                = CMD_FS,
    .startTrigger.triggerType = TRIG_NOW,
    .pNextOp                  = (RF_Op*)&txChainDoTx,
    .condition.rule           = COND_STOP_ON_FALSE,
    .synthConf.bTxMode        = 0x1, // Start FS in Tx Mode
};


static EMBENET_RADIO_CaptureCbt onStartOfFrameHandler; ///< Handler to method called when start of frame interrupt occurs
static EMBENET_RADIO_CaptureCbt onEndOfFrameHandler;   ///< Handler to method called when end of frame interrupt occurs
static void*                    handlersContext;       ///< Context passed to handlers

static uint8_t rxPacket[RX_BUFFER_LENGTH];

static int8_t rxPacketRssi;
static size_t rxPacketLength;


static RF_CmdHandle txTransaction;
static RF_CmdHandle rxTransaction;

static RF_Object     rfObject;
static RF_Handle     rfHandle;
static volatile bool idle;

extern void EXPECT_OnAbortHandler(char const* why, char const* file, int line);


/**
 * @brief In given FS cmd, sets frequency that corresponds with given channel
 * @param[in, out] cmd pointer do FS command to set the frequency
 * @param[in] channel channel to set
 * @note this function does not check, whether the given frequency is possible to set on this transceiver
 */
static void setChannel(rfc_CMD_FS_t* cmd, EMBENET_RADIO_Channel channel) {
    if (channel >= BAND_CHANNEL_COUNT) {
        channel = BAND_CHANNEL_COUNT - 1; // clipping
    }
    channel        = 0;
    uint32_t f     = BAND_START_FREQUENCY + ((uint32_t)channel * CHANNEL_WIDTH);
    cmd->frequency = (uint16_t)(f / KHZ_TO_HZ_RATIO);
    cmd->fractFreq = (uint16_t)((f % KHZ_TO_HZ_RATIO) * ((uint32_t)(UINT16_MAX + 1) / KHZ_TO_HZ_RATIO));
}


/**
 * @brief Sets txp to given value. If the txp is out of bounds, clamps to nearest possible value
 * @param[in] txp txp to set
 */
static void setTxp(EMBENET_RADIO_Power txp) {
    // Clamp txp to be sure that RF_TxPowerTable_findValue will always find some value
    if (txp > txPowerTable_custom868_0[TX_POWER_TABLE_SIZE_custom868_0 - 1].power) {
        txp = RF_TxPowerTable_MAX_DBM;
    }

    if (txp < txPowerTable_custom868_0[0].power) {
        txp = RF_TxPowerTable_MIN_DBM;
    }

    RF_setTxPower(rfHandle, RF_TxPowerTable_findValue(txPowerTable_custom868_0, txp));
}


static void rxProcessCb(RF_Handle h, RF_CmdHandle ch, RF_EventMask e);
static void txProcessCb(RF_Handle h, RF_CmdHandle ch, RF_EventMask e);

EMBENET_RADIO_Status EMBENET_RADIO_Init(void) {
    RF_Params rfParams;
    RF_Params_init(&rfParams);
    txTransaction = RF_ALLOC_ERROR;
    rxTransaction = RF_ALLOC_ERROR;

    rfHandle = RF_open(&rfObject, &RF_prop_custom868_0, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup_custom868_0, &rfParams);

    if (NULL == rfHandle) {
        EXPECT_OnAbortHandler("radio initialization failure", __FILE__, __LINE__);
    }
    EMBENET_RADIO_Idle();
    return EMBENET_RADIO_STATUS_SUCCESS;
}


void EMBENET_RADIO_SetCallbacks(EMBENET_RADIO_CaptureCbt onStartFrame, EMBENET_RADIO_CaptureCbt onEndFrame, void* cbtContext) {
    onStartOfFrameHandler = onStartFrame;
    onEndOfFrameHandler   = onEndFrame;
    handlersContext       = cbtContext;
}


void EMBENET_RADIO_Deinit(void) {
    if (rfHandle != NULL) {
        EMBENET_RADIO_Idle();
        RF_close(rfHandle);
    }
}


EMBENET_RADIO_Status EMBENET_RADIO_Idle(void) {
    idle = true;
    RF_flushCmd(rfHandle, txTransaction, 0);
    RF_flushCmd(rfHandle, rxTransaction, 0);
    RF_postCmd(rfHandle, (RF_Op*)&commonFsOff, RF_PriorityHigh, NULL, 0);

    return EMBENET_RADIO_STATUS_SUCCESS;
}

EMBENET_RADIO_Status EMBENET_RADIO_TxEnable(EMBENET_RADIO_Channel channel, EMBENET_RADIO_Power txp, uint8_t const* psdu, size_t psduLen) {
    idle = false;

    if (psduLen > EMBENET_RADIO_MAX_PSDU_LENGTH) {
        psduLen = EMBENET_RADIO_MAX_PSDU_LENGTH;
    }

    setTxp(txp);
    setChannel(&txChainSetFs, channel);

    memcpy(radioTxBuffer, psdu, psduLen);
    txChainDoTx.pktLen = (uint8_t)psduLen;

    return EMBENET_RADIO_STATUS_SUCCESS;
}


EMBENET_RADIO_Status EMBENET_RADIO_TxNow(void) {
    txTransaction = RF_postCmd(rfHandle, (RF_Op*)&txChainSetFs, RF_PriorityHigh, txProcessCb, RF_EventCmdDone | RF_EventLastCmdDone);
    return (RF_ALLOC_ERROR == txTransaction) ? EMBENET_RADIO_STATUS_GENERAL_ERROR : EMBENET_RADIO_STATUS_SUCCESS;
}


EMBENET_RADIO_Status EMBENET_RADIO_RxEnable(EMBENET_RADIO_Channel channel) {
    idle = false;
    if (rxTransaction != RF_ALLOC_ERROR) {
        return EMBENET_RADIO_STATUS_WRONG_STATE;
    }
    setChannel(&rxChainSetFs, channel);

    return EMBENET_RADIO_STATUS_SUCCESS;
}


EMBENET_RADIO_Status EMBENET_RADIO_RxNow(void) {
    rxTransaction = RF_postCmd(rfHandle, (RF_Op*)&rxChainSetFs, RF_PriorityHigh, rxProcessCb, RF_EventMdmSoft | RF_EventRxOk | RF_EventRxNOk);
    return (RF_ALLOC_ERROR == rxTransaction) ? EMBENET_RADIO_STATUS_GENERAL_ERROR : EMBENET_RADIO_STATUS_SUCCESS;
}


EMBENET_RADIO_RxInfo EMBENET_RADIO_GetReceivedFrame(uint8_t* buffer, size_t bufferLength) {
    if (bufferLength < rxPacketLength) {
        rxPacketLength = 0;
    }
    EMBENET_RADIO_RxInfo info = {.crcValid = rxPacketLength != 0, .lqi = 0, .mpduLength = rxPacketLength, .rssi = rxPacketRssi};
    if (rxPacketLength != 0) {
        memcpy(buffer, rxPacket, rxPacketLength);
    }
    return info;
}


EMBENET_RADIO_Status EMBENET_RADIO_StartContinuousTx(EMBENET_RADIO_ContinuousTxMode mode, EMBENET_RADIO_Channel channel, EMBENET_RADIO_Power txp) {
    uint8_t              dummy  = 0;
    EMBENET_RADIO_Status result = EMBENET_RADIO_TxEnable(channel, txp, &dummy, 1);
    if (EMBENET_RADIO_STATUS_SUCCESS == result) {
        RF_cmdTxTest_custom868_0.config.bUseCw = (EMBENET_RADIO_CONTINUOUS_TX_MODE_CARRIER == mode) ? 1 : 0;
        RF_runCmd(rfHandle, (RF_Op*)&RF_cmdTxTest_custom868_0, RF_PriorityNormal, NULL, 0);
    }
    return EMBENET_RADIO_STATUS_SUCCESS;
}


EMBENET_RADIO_Capabilities const* EMBENET_RADIO_GetCapabilities(void) {
    static EMBENET_RADIO_Capabilities timings = {.idleToTxReady   = 30,       //< TxEnable does a couple of calculations and sets pointers
                                                 .idleToRxReady   = 30,       //< RxEnable does a couple of calculations and sets pointers
                                                 .activeToTxReady = 30,       //< TxEnable does a couple of calculations and sets pointers
                                                 .activeToRxReady = 30,       //< RxEnable does a couple of calculations and sets pointers
                                                 .txDelay         = TX_DELAY, //< the time needed to prepare radio for transmission
                                                 .rxDelay         = 180,      //< hard to obtain, let it be the same as for transmission
                                                 .txRxStartDelay  = RX_START_CORRECTION,
                                                 .sensitivity     = SENSITIVITY,
                                                 .maxOutputPower  = MAX_OUTPUT_POWER,
                                                 .minOutputPower  = MIN_OUTPUT_POWER};
    return &timings;
}


static void rxProcessCb(RF_Handle h, RF_CmdHandle ch, RF_EventMask e) {
    (void)ch; // warning suppress
    (void)h;  // warning suppress
    if (idle) {
        return; // do nothing if idled
    }

    EMBENET_TimeUs t = EMBENET_TIMER_ReadCounter();
    if ((e & RF_EventMdmSoft) != 0) { // RX started
        if (onStartOfFrameHandler != NULL) {
            onStartOfFrameHandler(handlersContext, t - RX_START_CORRECTION);
        }
    } else if (((RF_EventRxOk | RF_EventRxNOk) & e) != 0) { // RX finished
        if ((rxChainDoRx.status == PROP_DONE_OK) || (rxChainDoRx.status == PROP_DONE_RXERR)) {
            if ((PROP_DONE_OK == rxChainDoRx.status) && (radioRxBuffer[0] <= RX_BUFFER_LENGTH)) {
                rxPacketLength = radioRxBuffer[0];
                rxPacketRssi   = (int8_t)radioRxBuffer[rxPacketLength + 1];
                memcpy(rxPacket, radioRxBuffer + 1, rxPacketLength);
            } else {
                rxPacketLength = 0;
                rxPacketRssi   = 0;
            }
            rxItem.status = DATA_ENTRY_PENDING;
            rxTransaction = RF_ALLOC_ERROR; // Clear current op handle
            if (onEndOfFrameHandler != NULL) {
                onEndOfFrameHandler(handlersContext, t);
            }
        }
    } else { // Transaction error
        rxItem.status = DATA_ENTRY_PENDING;
        rxTransaction = RF_ALLOC_ERROR; // Clear current op handle
    }
}

static void txProcessCb(RF_Handle h, RF_CmdHandle ch, RF_EventMask e) {
    (void)h;  // warning suppress
    (void)ch; // warning suppress
    if (idle) {
        return; // do nothing if idled
    }
    EMBENET_TimeUs t = EMBENET_TIMER_ReadCounter();

    if ((e & RF_EventLastCmdDone) != 0) { // End of transmission
        txTransaction = RF_ALLOC_ERROR;
        if ((PROP_DONE_OK == txChainDoTx.status) || (DONE_OK == txChainDoTx.status)) {
            if (onEndOfFrameHandler != NULL) {
                onEndOfFrameHandler(handlersContext, t + TX_END_CORRECTION);
            }
        }
    } else if (e & RF_EventCmdDone) { // synthesizer set and probably running, packet TX will start soon
        if ((e & RF_EventCmdDone) != 0) {
            if (onStartOfFrameHandler != NULL) {
                onStartOfFrameHandler(handlersContext, EMBENET_TIMER_ReadCounter() + TX_START_CORRECTION);
            }
        }
    } else { // Transaction error
        txTransaction = RF_ALLOC_ERROR;
    }
}
