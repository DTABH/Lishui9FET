/*
Library for King-Meter displays

Copyright © 2015 Michael Fabry (Michael@Fabry.de)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


// Includes

#include "config.h"
#include "main.h"
#include "display_kingmeter.h"
#include "stm32f0xx_hal.h"
#include <string.h>


// Definitions

#define KM_STARTCODE        0x3A
#define KM_SRCADD           0x1A

#define RXSTATE_STARTCODE   0
#define RXSTATE_SENDTXMSG   1
#define RXSTATE_MSGBODY     2
#define RXSTATE_DONE        3

/* Number of replies that may wait for the UART. The display sends 0x53 and
 * 0x52 back to back, so more than one reply can be pending. */
#define KM_TX_QUEUE_LEN     4


extern UART_HandleTypeDef huart1;


// Hashtable used for handshaking in 901U protocol

 const uint8_t KM_901U_HANDSHAKE[64] =
 {
 		137,
 		159,
 		134,
 		249,
 		88,
 		11,
 		250,
 		61,
 		33,
 		150,
 		3,
 		193,
 		118,
 		141,
 		209,
 		94,
 		226,
 		68,
 		146,
 		158,
 		145,
 		127,
 		216,
 		62,
 		116,
 		230,
 		101,
 		211,
 		251,
 		54,
 		229,
 		247,
 		20,
 		222,
 		59,
 		63,
 		35,
 		252,
 		142,
 		238,
 		23,
 		197,
 		84,
 		77,
 		147,
 		173,
 		210,
 		57,
 		142,
 		223,
 		157,
 		97,
 		36,
 		160,
 		229,
 		237,
 		75,
 		80,
 		37,
 		113,
 		154,
 		88,
 		23,
 		120
 };

 const uint16_t WheelSize_LookUp[8]= {1277,1436,1596,1756,1915,2075,2154,2234};
 const uint8_t powertable[6] = {0,34,51,77,115,255};


// Globals (watchable in the debugger)

volatile uint32_t KM_RxFramesGood = 0;
volatile uint32_t KM_RxFramesBad  = 0;
volatile uint32_t KM_TxFramesDropped = 0;

volatile uint8_t  KM_LastRxFrame[KM_MAX_FRAME];
volatile uint8_t  KM_LastRxLength = 0;
volatile uint8_t  KM_LastTxFrame[KM_MAX_TXBUFF];
volatile uint8_t  KM_LastTxLength = 0;


uint8_t pas_tolerance  = 0;
uint8_t wheel_magnets = 1;
uint8_t vcutoff = 30;
uint8_t spd_max1 = 25;
uint8_t ui8_RxLength=1;


// RX parser state (byte wise, independent of DMA/IDLE chunk boundaries)

static uint8_t  km_rx_frame[KM_MAX_FRAME];
static uint8_t  km_rx_idx = 0;          // bytes collected in current candidate
static uint8_t  km_rx_len = 0;          // expected total length once LEN is known
static uint16_t km_rx_tail = 0;         // consumer index into the circular DMA buffer

// TX queue

static uint8_t  km_tx_queue[KM_TX_QUEUE_LEN][KM_MAX_TXBUFF];
static uint8_t  km_tx_queue_len[KM_TX_QUEUE_LEN];
static uint8_t  km_tx_head = 0;
static uint8_t  km_tx_tail = 0;
static uint8_t  km_tx_count = 0;


// Internal prototypes

static void KM_RxByte(KINGMETER_t* KM_ctx, uint8_t byte);
static void KM_ParseFrame(KINGMETER_t* KM_ctx, const uint8_t* frame, uint8_t length);
static void KM_Handle52(KINGMETER_t* KM_ctx, const uint8_t* frame);
static void KM_Handle53(KINGMETER_t* KM_ctx, const uint8_t* frame);

static void KM_TxEnqueue(const uint8_t* frame, uint8_t length);
static void KM_TxFinishAndEnqueue(uint8_t* frame, uint8_t length);
static void KM_Send52(KINGMETER_t* KM_ctx);
static void KM_Send53(KINGMETER_t* KM_ctx, uint8_t challenge);

static uint8_t lowByte(uint16_t word);
static uint8_t highByte(uint16_t word);


/* Public functions (Prototypes declared by display_kingmeter.h) */

/****************************************************************************************************
 * KingMeter_Init() - Initializes the display object
 *
 ****************************************************************************************************/

void KingMeter_Init (KINGMETER_t* KM_ctx)

{
    uint8_t i;


    KM_ctx->RxState                         = RXSTATE_STARTCODE;
    KM_ctx->DirectSetpoint                  = 0xFF; //hier aktuellen Timerwert als Startzeitpunkt

    for(i=0; i<KM_MAX_RXBUFF; i++)
    {
        KM_ctx->RxBuff[i]                   = 0x00;
    }

    KM_ctx->RxCnt                           = 0;

    // Reset RX parser and TX queue
    km_rx_idx                               = 0;
    km_rx_len                               = 0;
    km_rx_tail                              = 0;
    km_tx_head                              = 0;
    km_tx_tail                              = 0;
    km_tx_count                             = 0;

    KM_RxFramesGood                         = 0;
    KM_RxFramesBad                          = 0;
    KM_TxFramesDropped                      = 0;
    KM_LastRxLength                         = 0;
    KM_LastTxLength                         = 0;

    // Settings received from display:
    KM_ctx->Settings.DoublePushAssist      	= 0;
    KM_ctx->Settings.PAS_SCN_Tolerance      = (uint8_t) pas_tolerance;
    KM_ctx->Settings.Ramp_End            	= RAMP_END;
    KM_ctx->Settings.LegalFlag        		= 1;
    KM_ctx->Settings.Level_Behavior        	= 0;
    KM_ctx->Settings.RideMode      			= 0;
    KM_ctx->Settings.SPS_SpdMagnets         = PULSES_PER_REVOLUTION;
    KM_ctx->Settings.VOL_1_UnderVolt_x10    = (uint16_t) (vcutoff * 10);
    KM_ctx->Settings.WheelSize_mm           = WHEEL_CIRCUMFERENCE;

    // Parameters received from display in operation mode:

	KM_ctx->Rx.AssistLevel                  = 128;					//MK5S Level 0...255
    KM_ctx->Rx.AssistLevelIndex             = 0;                    //0..5 (0 = off)


    KM_ctx->Rx.Headlight                    = KM_HEADLIGHT_OFF;
    KM_ctx->Rx.Battery                      = KM_BATTERY_NORMAL;
    KM_ctx->Rx.PushAssist                   = KM_PUSHASSIST_OFF;
    KM_ctx->Rx.PowerAssist                  = KM_POWERASSIST_ON;
    KM_ctx->Rx.Throttle                     = KM_THROTTLE_ON;
    KM_ctx->Rx.CruiseControl                = KM_CRUISE_OFF;
    KM_ctx->Rx.OverSpeed                    = KM_OVERSPEED_NO;
    KM_ctx->Rx.SPEEDMAX_Limit           	= SPEEDLIMIT*100;
    KM_ctx->Rx.CUR_Limit_mA                	= BATTERY_CURRENT_MAX;

    // Parameters to be send to display in operation mode:
    KM_ctx->Tx.Battery                      = KM_BATTERY_NORMAL;
    KM_ctx->Tx.Wheeltime_ms                 = KM_MAX_WHEELTIME;
    KM_ctx->Tx.Error                        = KM_ERROR_NONE;
    KM_ctx->Tx.Current_x10                  = 0;


    //Start UART with DMA  HAL_UART_Receive_DMA

    if (UART_Start_Receive_DMA(&huart1, (uint8_t *)KM_ctx->RxBuff, KM_MAX_RXBUFF) != HAL_OK)
     {
 	   Error_Handler();
     }
}



/****************************************************************************************************
 * KingMeter_Service() - Communicates data from and to the display
 *
 ***************************************************************************************************/
void KingMeter_Service(KINGMETER_t* KM_ctx)
{
    KingMeter_RxService(KM_ctx);
    KingMeter_TxService(KM_ctx);
}


/****************************************************************************************************
 * KingMeter_RxService() - Consumes all bytes the circular DMA has received so far
 *
 ***************************************************************************************************/
void KingMeter_RxService(KINGMETER_t* KM_ctx)
{
    uint16_t head;

    /* Producer index of the circular DMA buffer. Reading CNDTR once is
     * enough: bytes that arrive while we parse are handled next call. */
    head = (uint16_t)((KM_MAX_RXBUFF - DMA1_Channel3->CNDTR) & (KM_MAX_RXBUFF - 1));

    while (km_rx_tail != head)
    {
        uint8_t byte = KM_ctx->RxBuff[km_rx_tail];
        km_rx_tail = (uint16_t)((km_rx_tail + 1) & (KM_MAX_RXBUFF - 1));
        KM_RxByte(KM_ctx, byte);
    }
}


/****************************************************************************************************
 * KingMeter_TxService() - Sends the next queued reply if the UART is free
 *
 ***************************************************************************************************/
void KingMeter_TxService(KINGMETER_t* KM_ctx)
{
    (void)KM_ctx;

    if (km_tx_count == 0)
    {
        return;
    }

    /* Do not overwrite a reply that is still being transmitted. */
    if (huart1.gState != HAL_UART_STATE_READY)
    {
        return;
    }

    if (HAL_UART_Transmit_DMA(&huart1,
                              km_tx_queue[km_tx_tail],
                              km_tx_queue_len[km_tx_tail]) == HAL_OK)
    {
        km_tx_tail = (uint8_t)((km_tx_tail + 1) % KM_TX_QUEUE_LEN);
        km_tx_count--;
    }
}


/****************************************************************************************************
 * KM_RxByte() - Byte wise 5S frame parser
 *
 * Searches for 0x3A, validates the length field, collects exactly one frame
 * and hands it to KM_ParseFrame(). Works independently of how the DMA/IDLE
 * interrupt splits the byte stream into chunks.
 *
 ***************************************************************************************************/
static void KM_RxByte(KINGMETER_t* KM_ctx, uint8_t byte)
{
    if (km_rx_idx == 0)
    {
        if (byte == KM_STARTCODE)
        {
            km_rx_frame[0] = byte;
            km_rx_idx = 1;
        }
        return;
    }

    if (km_rx_idx == 1)
    {
        if (byte == KM_SRCADD)
        {
            km_rx_frame[1] = byte;
            km_rx_idx = 2;
        }
        else if (byte == KM_STARTCODE)
        {
            /* Restart, keep the new start code */
            km_rx_frame[0] = byte;
        }
        else
        {
            km_rx_idx = 0;
        }
        return;
    }

    if (km_rx_idx == 2)
    {
        km_rx_frame[2] = byte;
        km_rx_idx = 3;
        return;
    }

    if (km_rx_idx == 3)
    {
        if (byte <= KM_MAX_DATALEN)
        {
            km_rx_frame[3] = byte;
            km_rx_len = (uint8_t)(byte + 8);    // 3 header + data + 2 checksum + CRLF
            km_rx_idx = 4;
        }
        else
        {
            /* Implausible length: restart and check this byte as start code */
            km_rx_idx = 0;
            km_rx_len = 0;
            if (byte == KM_STARTCODE)
            {
                km_rx_frame[0] = byte;
                km_rx_idx = 1;
            }
        }
        return;
    }

    km_rx_frame[km_rx_idx++] = byte;

    if (km_rx_idx >= km_rx_len)
    {
        KM_ParseFrame(KM_ctx, km_rx_frame, km_rx_len);
        km_rx_idx = 0;
        km_rx_len = 0;
    }
}


/****************************************************************************************************
 * KM_ParseFrame() - Checks a complete frame and dispatches it
 *
 ***************************************************************************************************/
static void KM_ParseFrame(KINGMETER_t* KM_ctx, const uint8_t* frame, uint8_t length)
{
    uint8_t  i;
    uint8_t  data_len;
    uint16_t checksum;
    uint16_t received;

    // Keep the last frame for debugging (also invalid ones)
    if (length > KM_MAX_FRAME)
    {
        length = KM_MAX_FRAME;
    }
    for (i = 0; i < length; i++)
    {
        KM_LastRxFrame[i] = frame[i];
    }
    KM_LastRxLength = length;

    if (length < 8)
    {
        KM_RxFramesBad++;
        return;
    }

    data_len = frame[3];

    if ((uint16_t)data_len + 8 != length)
    {
        KM_RxFramesBad++;
        return;
    }

    // Every 5S frame ends with CR LF
    if (frame[length - 2] != 0x0D || frame[length - 1] != 0x0A)
    {
        KM_RxFramesBad++;
        return;
    }

    // Checksum = sum of bytes 1 .. (3 + data_len)
    checksum = 0;
    for (i = 1; i <= (uint8_t)(3 + data_len); i++)
    {
        checksum += frame[i];
    }

    received = (uint16_t)frame[4 + data_len]
             | ((uint16_t)frame[5 + data_len] << 8);

    if (checksum != received)
    {
        KM_RxFramesBad++;
        return;
    }

    KM_RxFramesGood++;

    switch (frame[2])
    {
        case 0x52:      // Operation mode
            KM_Handle52(KM_ctx, frame);
            break;

        case 0x53:      // Settings / handshake mode
            KM_Handle53(KM_ctx, frame);
            break;

        case 0x54:      // Direct setpoint
            KM_ctx->DirectSetpoint = (int8_t)frame[4];
            break;

        default:
            break;
    }
}


/****************************************************************************************************
 * KM_Handle52() - Operation mode frame from the display
 *
 ***************************************************************************************************/
static void KM_Handle52(KINGMETER_t* KM_ctx, const uint8_t* frame)
{
    if (frame[3] >= 2)
    {
        if (KM_ctx->Settings.Level_Behavior)
        {
            KM_ctx->Rx.AssistLevel = powertable[(frame[4] / 50)];
        }
        else
        {
            KM_ctx->Rx.AssistLevel = frame[4];              // 0..255
        }

        /* C500 sends 0, 50, 100, 150, 200, 250 for levels 0..5.
         * Keep a separate 0..5 index for the per-level speed limits. */
        KM_ctx->Rx.AssistLevelIndex = frame[4] / 50;
        if (KM_ctx->Rx.AssistLevelIndex > 5)
        {
            KM_ctx->Rx.AssistLevelIndex = 5;
        }

        KM_ctx->Rx.Headlight     = (frame[5] & 0xC0) >> 6;  // KM_HEADLIGHT_OFF / ON / LOW / HIGH
        KM_ctx->Rx.Battery       = (frame[5] & 0x20) >> 5;  // KM_BATTERY_NORMAL / LOW
        KM_ctx->Rx.PushAssist    = (frame[5] & 0x10) >> 4;  // KM_PUSHASSIST_OFF / ON
        KM_ctx->Rx.PowerAssist   = (frame[5] & 0x08) >> 3;  // KM_POWERASSIST_OFF / ON
        KM_ctx->Rx.Throttle      = (frame[5] & 0x04) >> 2;  // KM_THROTTLE_OFF / ON
        KM_ctx->Rx.CruiseControl = (frame[5] & 0x02) >> 1;  // KM_CRUISE_OFF / ON
        KM_ctx->Rx.OverSpeed     = (frame[5] & 0x01);       // KM_OVERSPEED_NO / YES
    }

    kingmeter_update();

    KM_Send52(KM_ctx);
}


/****************************************************************************************************
 * KM_Handle53() - Settings / handshake frame from the display
 *
 ***************************************************************************************************/
static void KM_Handle53(KINGMETER_t* KM_ctx, const uint8_t* frame)
{
    uint8_t  challenge;
    uint16_t current_limit;

    /* The handshake challenge is always the second to last data byte,
     * independent of the frame length (7 data bytes on C500, 5 on some
     * other 5S displays). */
    challenge = frame[frame[3] + 2];

    if (frame[3] >= 7)
    {
        // Full settings frame (C500 uses 7 data bytes)
        KM_ctx->Settings.DoublePushAssist = ((frame[4] >> 6) & 1) + 1; // ist eigentlich PAS direction
        KM_ctx->Settings.Ramp_End         = (uint16_t)((frame[4] & 0x3F) << 6);
        KM_ctx->Settings.LegalFlag        = 1;                          // immer Geschwindigkeitsbegrenzung
        KM_ctx->Settings.Level_Behavior   = (frame[6] >> 7) & 1;        // 0=direct, 1=lookup table

        /* frame[6] bits 4..6 are the C500 PAS parameter setting, NOT the
         * controller ride mode. Keep RideMode at its initialized value
         * (0 = Powerlevels) so a cadence PAS sensor works. */
        // KM_ctx->Settings.RideMode      = (frame[6] >> 4) & 3;

        if ((frame[6] & 7) != 0)
        {
            KM_ctx->Settings.SPS_SpdMagnets = frame[6] & 7;
        }

        KM_ctx->Settings.WheelSize_mm = WheelSize_LookUp[(frame[10] & 7)];

        if (((frame[10] >> 3) + 10) * 100 > SPEEDLIMIT * 100)
        {
            KM_ctx->Rx.SPEEDMAX_Limit = SPEEDLIMIT * 100;   // Maximal auf SPEEDLIMIT im Display änderbar
        }
        else
        {
            KM_ctx->Rx.SPEEDMAX_Limit = ((frame[10] >> 3) + 10) * 100;
        }

        current_limit = (uint16_t)((frame[8] & 0x3F) * 500);

        /* Never accept a zero/nonsense current limit: kingmeter_update()
         * divides the assist current by it and a zero would kill PAS. */
        if (current_limit >= 3000)
        {
            KM_ctx->Rx.CUR_Limit_mA = current_limit;
        }
    }

    kingmeter_update();

    KM_Send53(KM_ctx, (uint8_t)(challenge & 0x3F));
}


/****************************************************************************************************
 * TX helpers
 *
 ***************************************************************************************************/
static void KM_TxEnqueue(const uint8_t* frame, uint8_t length)
{
    if (length == 0 || length > KM_MAX_TXBUFF)
    {
        KM_TxFramesDropped++;
        return;
    }

    if (km_tx_count >= KM_TX_QUEUE_LEN)
    {
        // Queue full: drop the oldest reply
        km_tx_tail = (uint8_t)((km_tx_tail + 1) % KM_TX_QUEUE_LEN);
        km_tx_count--;
        KM_TxFramesDropped++;
    }

    memcpy(km_tx_queue[km_tx_head], frame, length);
    km_tx_queue_len[km_tx_head] = length;
    km_tx_head = (uint8_t)((km_tx_head + 1) % KM_TX_QUEUE_LEN);
    km_tx_count++;
}


static void KM_TxFinishAndEnqueue(uint8_t* frame, uint8_t length)
{
    uint8_t  i;
    uint16_t checksum = 0;

    for (i = 1; i < length; i++)
    {
        checksum += frame[i];
    }

    frame[length]     = lowByte(checksum);
    frame[length + 1] = highByte(checksum);
    frame[length + 2] = 0x0D;
    frame[length + 3] = 0x0A;

    length = (uint8_t)(length + 4);

    // Keep the last reply for debugging
    for (i = 0; i < length; i++)
    {
        KM_LastTxFrame[i] = frame[i];
    }
    KM_LastTxLength = length;

    KM_TxEnqueue(frame, length);
}


static void KM_Send52(KINGMETER_t* KM_ctx)
{
    uint8_t tx[KM_MAX_TXBUFF];

    tx[0] = KM_STARTCODE;                                   // StartCode
    tx[1] = KM_SRCADD;                                      // SrcAdd: Controller
    tx[2] = 0x52;                                           // CmdCode
    tx[3] = 0x05;                                           // DataSize

    if (KM_ctx->Tx.Battery == KM_BATTERY_LOW)
    {
        tx[4] = 0x40;                                       // UnderVoltage bit
    }
    else
    {
        tx[4] = 0x00;
    }

    tx[5] = (uint8_t)((KM_ctx->Tx.Current_x10 * 3) / 10);   // Current in 1/3 A
    tx[6] = highByte(KM_ctx->Tx.Wheeltime_ms);              // WheelSpeed high
    tx[7] = lowByte(KM_ctx->Tx.Wheeltime_ms);               // WheelSpeed low
    tx[8] = KM_ctx->Tx.Error;                               // Error

    KM_TxFinishAndEnqueue(tx, 9);
}


static void KM_Send53(KINGMETER_t* KM_ctx, uint8_t challenge)
{
    uint8_t tx[KM_MAX_TXBUFF];

    (void)KM_ctx;

    tx[0] = KM_STARTCODE;                                   // StartCode
    tx[1] = KM_SRCADD;                                      // SrcAdd: Controller
    tx[2] = 0x53;                                           // CmdCode
    tx[3] = 0x05;                                           // Number of Databytes
    tx[4] = 0x00;
    tx[5] = 0x00;
    tx[6] = 0x0D;
    tx[7] = KM_901U_HANDSHAKE[challenge];                   // Handshake answer
    tx[8] = 0x00;

    KM_TxFinishAndEnqueue(tx, 9);
}


static uint8_t lowByte(uint16_t word){
	return word & 0xFF;
}

static uint8_t highByte(uint16_t word){
	return word >>8;
}
