/****************************************************************************

  (c) SYSTEC electronic GmbH, D-07973 Greiz, August-Bebel-Str. 29
      www.systec-electronic.com
  (c) Bernecker + Rainer Industrie-Elektronik Ges.m.b.H.
      A-5142 Eggelsberg, B&R Strasse 1
      www.br-automation.com


  Project:      openPOWERLINK

  Description:  Ethernet Driver for openMAC

  License:

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    3. Neither the name of SYSTEC electronic GmbH nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without prior written permission. For written
       permission, please contact info@systec-electronic.com.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

    Severability Clause:

        If a provision of this License is or becomes illegal, invalid or
        unenforceable in any jurisdiction, that shall not affect:
        1. the validity or enforceability in that jurisdiction of any other
           provision of this License; or
        2. the validity or enforceability in other jurisdictions of that or
           any other provision of this License.

  -------------------------------------------------------------------------
                $RCSfile$

                $Author$

                $Revision$  $Date$

                $State$

                Build Environment:
                    GCC V3.4

----------------------------------------------------------------------------*/


#include "global.h"
#include "EplInc.h"
#include "edrv.h"
#include "Benchmark.h"

#ifdef __NIOS2__
#include "system.h"     // FPGA system definitions
#include <sys/alt_cache.h>
#include <sys/alt_irq.h>
#include <alt_types.h>
#include <io.h>
#elif defined(__MICROBLAZE__)
#include "xparameters.h" // FPGA system definitions
#include "xintc_l.h"
#include "mb_interface.h"
#else
    #error "Configuration is unknown!"
#endif
#include "omethlib.h"   // openMAC header

#include "EplTgtTimeStamp_openMac.h"

#ifdef CPU_UTIL
#include "cpuUtil.h"
#endif

//comment the following lines to disable feature
//#define EDRV_DEBUG        //debugging information forwarded to stdout
//#define EDRV_TTTX        //use time-triggered TX feature for MN
//#define EDRV_2NDTXQUEUE    //use additional TX queue for MN

//---------------------------------------------------------------------------
// defines
//---------------------------------------------------------------------------

//------------------------------------------------------
//--- set phys settings ---
//set phy AC timing behavior (ref. to data sheet)
#define EDRV_PHY_RST_PULSE_US        10000 //length of reset pulse (rst_n = 0)
#define EDRV_PHY_RST_READY_US         5000 //time after phy is ready to operate

//--- packet location definitions ---
#define EDRV_PKT_LOC_TX_RX_INT                0
#define EDRV_PKT_LOC_TX_INT_RX_EXT            1
#define EDRV_PKT_LOC_TX_RX_EXT                2

//--- set the system's base addresses ---
#ifdef __NIOS2__
#ifdef __POWERLINK //POWERLINK IP-core used
    #define EDRV_MAC_BASE           (void *)POWERLINK_0_MAC_REG_BASE
    #define EDRV_MAC_SPAN                   POWERLINK_0_MAC_REG_SPAN
    #define EDRV_MAC_IRQ                    POWERLINK_0_MAC_REG_IRQ
    #define EDRV_MAC_IRQ_IC_ID              POWERLINK_0_MAC_REG_IRQ_INTERRUPT_CONTROLLER_ID
    #define EDRV_RAM_BASE           (void *)(EDRV_MAC_BASE + 0x0800)
    #define EDRV_MII_BASE           (void *)(EDRV_MAC_BASE + 0x1000)
    #define EDRV_IRQ_BASE           (void *)(EDRV_MAC_BASE + 0x1010)
    #define EDRV_DOB_BASE           (void *)(EDRV_MAC_BASE + 0x1020)
    #define EDRV_CMP_BASE           (void *)POWERLINK_0_MAC_CMP_BASE
    #define EDRV_CMP_SPAN                   POWERLINK_0_MAC_CMP_SPAN
    #define EDRV_PKT_LOC                    POWERLINK_0_MAC_REG_PKTLOC
    #define EDRV_PHY_NUM                    POWERLINK_0_MAC_REG_PHYCNT
    #define EDRV_DMA_OBSERVER                POWERLINK_0_MAC_REG_DMAOBSERV
#if EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT                        //TX+RX in M9K
    #define EDRV_MAX_RX_BUFFERS             POWERLINK_0_MAC_REG_MACRXBUFFERS
    #define EDRV_PKT_BASE           (void *)POWERLINK_0_MAC_BUF_BASE
    #define EDRV_PKT_SPAN                   POWERLINK_0_MAC_BUF_MACBUFSIZE
#elif EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT                        //TX in M9K and RX in external memory
    #define EDRV_MAX_RX_BUFFERS             16 //packets are stored in heap, set depending on your needs
    #define EDRV_PKT_BASE           (void *)POWERLINK_0_MAC_BUF_BASE
    #define EDRV_PKT_SPAN                   POWERLINK_0_MAC_BUF_MACBUFSIZE
#elif EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_EXT                        //TX+RX in external memory
    #define EDRV_MAX_RX_BUFFERS             16 //packets are stored in heap, set depending on your needs
    #define EDRV_PKT_BASE           (void *)0 //not used
    #define EDRV_PKT_SPAN                   0 //not used
#endif
#elif defined(__OPENMAC) //OPENMAC IP-core used
    #error "Not supported! Please change to POWERLINK IP-core!"
#else
    #error "Configuration is unknown!"
#endif
#elif defined(__MICROBLAZE__)
    #define EDRV_INTC_BASE                  XPAR_PCP_INTC_BASEADDR
    #define EDRV_MAC_BASE           (void *)XPAR_PLB_POWERLINK_0_MAC_REG_BASEADDR
    #define EDRV_MAC_SPAN                   (XPAR_PLB_POWERLINK_0_MAC_REG_HIGHADDR-XPAR_PLB_POWERLINK_0_MAC_REG_BASEADDR+1)
    #define EDRV_MAC_IRQ                    XPAR_PCP_INTC_PLB_POWERLINK_0_MAC_IRQ_INTR
    #define EDRV_MAC_IRQ_MASK                XPAR_PLB_POWERLINK_0_MAC_IRQ_MASK
    #define EDRV_RAM_BASE           (void *)(EDRV_MAC_BASE + 0x0800)
    #define EDRV_MII_BASE           (void *)(EDRV_MAC_BASE + 0x1000)
    #define EDRV_IRQ_BASE           (void *)(EDRV_MAC_BASE + 0x1010)
    #define EDRV_DOB_BASE           (void *)(EDRV_MAC_BASE + 0x1020)
    #define EDRV_CMP_BASE           (void *)XPAR_PLB_POWERLINK_0_MAC_CMP_BASEADDR
    #define EDRV_CMP_SPAN                   (XPAR_PLB_POWERLINK_0_MAC_CMP_HIGHADDR-XPAR_PLB_POWERLINK_0_MAC_CMP_BASEADDR+1)
    #define EDRV_PKT_LOC                    XPAR_PLB_POWERLINK_0_PACKET_LOCATION
    #define EDRV_PHY_NUM                    XPAR_PLB_POWERLINK_0_PHY_COUNT
    #define EDRV_DMA_OBSERVER                XPAR_PLB_POWERLINK_0_OBSERVER_ENABLE
#if EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT
    #define EDRV_MAX_RX_BUFFERS             XPAR_PLB_POWERLINK_0_MAC_RX_BUFFERS
    #define EDRV_PKT_BASE           (void *)XPAR_PLB_POWERLINK_0_MAC_PKT_BASEADDR
    #define EDRV_PKT_SPAN                   XPAR_PLB_POWERLINK_0_MAC_PKT_SIZE
#elif EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_EXT
    #define EDRV_MAX_RX_BUFFERS             16 //packets are stored in heap, set depending on your needs
    #define EDRV_PKT_BASE           (void *)0 //not used
    #define EDRV_PKT_SPAN                   0 //not used
#elif EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT
    #define EDRV_MAX_RX_BUFFERS             16 //packets are stored in heap, set depending on your needs
    #define EDRV_PKT_BASE           (void *)XPAR_PLB_POWERLINK_0_MAC_PKT_BASEADDR
    #define EDRV_PKT_SPAN                   XPAR_PLB_POWERLINK_0_MAC_PKT_SIZE
#else
    #error "Configuration is unknown!"
#endif
#endif

#define EDRV_GET_MAC_TIME()            IORD_32DIRECT(EDRV_CMP_BASE, 0)

//--- set driver's MTU ---
#define EDRV_MAX_BUFFER_SIZE        1518

//--- set driver's filters ---
#define EDRV_MAX_FILTERS            16

//--- set driver's auto-response frames ---
#define EDRV_MAX_AUTO_RESPONSES     14

//--- set additional transmit queue size ---
#define EDRV_MAX_TX_BUF2            16


#if (EDRV_MAX_RX_BUFFERS > 16)
    #error "This MAC version can handle 16 Rx buffers, not more!"
#elif (EDRV_MAX_RX_BUFFERS == 0)
#warning "Rx buffers set to zero -> set value by yourself!"
#undef EDRV_MAX_RX_BUFFERS
#define EDRV_MAX_RX_BUFFERS 6
#endif

#if (EDRV_AUTO_RESPONSE == FALSE && !defined(EDRV_TTTX))
    #error "Please enable EDRV_AUTO_RESPONSE in EplCfg.h to use openMAC for CN!"
#endif
#if (EDRV_AUTO_RESPONSE != FALSE && defined(EDRV_TTTX))
#error "Please disable EDRV_AUTO_RESPONSE in EplCfg.h to use openMAC for MN!"
#endif

#if (defined(EDRV_2NDTXQUEUE) && !defined(EDRV_TTTX))
    #undef EDRV_2NDTXQUEUE //2nd TX queue makes no sense here..
    #undef EDRV_MAX_TX_BUF2
#endif


// borrowed from omethlibint.h
#define GET_TYPE_BASE(typ, element, ptr)    \
    ((typ*)( ((size_t)ptr) - (size_t)&((typ*)0)->element ))

#ifdef __NIOS2__
#include <unistd.h>
#elif defined(__MICROBLAZE__)
#include "xilinx_usleep.h"
#endif
#define EDRV_USLEEP(time)            usleep(time)

#ifdef __NIOS2__
#define EDRV_RD32(base, offset)        IORD_32DIRECT(base, offset)
#define EDRV_RD16(base, offset)        IORD_16DIRECT(base, offset)
#elif defined(__MICROBLAZE__)
#define EDRV_RD32(base, offset)        Xil_In32((base+offset))
#define EDRV_RD16(base, offset)        Xil_In16((base+offset))
#else
#error "Configuration unknown!"
#endif

//---------------------------------------------------------------------------
// local types
//---------------------------------------------------------------------------

typedef struct _tEdrvInstance
{
    //EPL spec
    tEdrvInitParam           m_InitParam;

    //openMAC HAL Ethernet Driver
    ometh_config_typ         m_EthConf;
    OMETH_H                  m_hOpenMac;
    OMETH_HOOK_H             m_hHook;
    OMETH_FILTER_H           m_ahFilter[EDRV_MAX_FILTERS];

    phy_reg_typ*             m_pPhy[EDRV_PHY_NUM];
    BYTE                     m_ubPhyCnt;

    // auto-response Tx buffers
    tEdrvTxBuffer*           m_apTxBuffer[EDRV_MAX_FILTERS];

    //tx msg counter
    DWORD                    m_dwMsgFree;
    DWORD                    m_dwMsgSent;
#if (EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT || EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT)
    //needed for buffer management not located in heap
#if EDVR_PKT_LOC == 0
    void*                    m_pRxBufBase;
#endif
    void*                    m_pTxBufBase;
    BYTE                     m_ubTxBufCnt;
    void*                    m_pBufBase;
    DWORD                    m_dwBufSpan;
#endif
#if EDRV_DMA_OBSERVER != 0
    BOOL                     m_fDmaError;
#endif

#ifdef EDRV_2NDTXQUEUE
    //additional tx queue
    tEdrvTxBuffer*           m_apTxQueue[EDRV_MAX_TX_BUF2];
    int                      m_iTxQueueWr;
    int                      m_iTxQueueRd;
#endif

#ifdef EDRV_SOCJITTER_MONITOR
    DWORD                    m_dwSocLastTimeStamp;
    BOOL                     m_fSocMonitorValid;
    DWORD                    m_dwSocMaxJitterNs;
#endif

} tEdrvInstance;

//---------------------------------------------------------------------------
// prototypes
//---------------------------------------------------------------------------


// RX Hook function
static int EdrvRxHook(void *arg, ometh_packet_typ  *pPacket, OMETH_BUF_FREE_FCT  *pFct);

static void EdrvCbSendAck(ometh_packet_typ *pPacket, void *arg, unsigned long time);

static void EdrvIrqHandler (void* pArg_p
#ifndef ALT_ENHANCED_INTERRUPT_API_PRESENT
        , DWORD dwInt_p
#endif
        );



//---------------------------------------------------------------------------
// module globale vars
//---------------------------------------------------------------------------


static tEdrvInstance EdrvInstance_l;



//=========================================================================//
//                                                                         //
//          P U B L I C   F U N C T I O N S                                //
//                                                                         //
//=========================================================================//

//---------------------------------------------------------------------------
//
// Function:    EdrvInit
//
// Description:
//
// Parameters:  pEdrvInitParam_p    = pointer to struct including the init-parameters
//
// Returns:     Errorcode           = kEplSuccessful
//                                  = kEplNoResource
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvInit(tEdrvInitParam * pEdrvInitParam_p)
{
tEplKernel      Ret = kEplSuccessful;
int             i;
BYTE            abFilterMask[31],
                abFilterValue[31];

#if EDRV_DMA_OBSERVER != 0
    PRINTF0("INFO: DMA monitor circuit is enabled.\n");
#endif

    memset(&EdrvInstance_l, 0, sizeof(EdrvInstance_l)); //reset driver struct

    memset(EDRV_MAC_BASE, 0, EDRV_MAC_SPAN); //reset openMAC register and RAM

#if (EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT || EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT)
    memset(EDRV_PKT_BASE, 0, EDRV_PKT_SPAN); //reset MAC-internal buffers
#endif

    EdrvInstance_l.m_InitParam = *pEdrvInitParam_p;

    PRINTF0("initialize openMAC...");

    ////////////////////
    // initialize phy //
    ////////////////////
    omethMiiControl(EDRV_MII_BASE, MII_CTRL_RESET);
    EDRV_USLEEP(EDRV_PHY_RST_PULSE_US);
    omethMiiControl(EDRV_MII_BASE, MII_CTRL_ACTIVE);
    EDRV_USLEEP(EDRV_PHY_RST_READY_US);

    ////////////////////////////////
    // initialize ethernet driver //
    ////////////////////////////////
    omethInit();

    EdrvInstance_l.m_EthConf.adapter = 0; //adapter number
    EdrvInstance_l.m_EthConf.macType = OMETH_MAC_TYPE_01;    // more info in omethlib.h

    EdrvInstance_l.m_EthConf.mode = 0; //set supported modes
    EdrvInstance_l.m_EthConf.mode |= OMETH_MODE_HALFDUPLEX; //only half-duplex allowed
    EdrvInstance_l.m_EthConf.mode |= OMETH_MODE_100MBIT; //only 100Mbps mode allowed
    //TODO: Marvell 88E1111 dislikes disabling auto-negotiation - workaround will follow
    //EdrvInstance_l.m_EthConf.mode |= OMETH_MODE_DIS_AUTO_NEG; //phys are fixed to selected mode (no auto-negotiation)

#ifdef __NIOS2__
    alt_remap_uncached((void*)EDRV_MAC_BASE, EDRV_MAC_SPAN);
    alt_remap_uncached((void*)EDRV_CMP_BASE, EDRV_CMP_SPAN);
#endif

    EdrvInstance_l.m_EthConf.pPhyBase = EDRV_MII_BASE;
    EdrvInstance_l.m_EthConf.pRamBase = EDRV_RAM_BASE;
    EdrvInstance_l.m_EthConf.pRegBase = EDRV_MAC_BASE;
#if EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT
    //set mac-internal buffer base
#ifdef __NIOS2__
    EdrvInstance_l.m_EthConf.pBufBase = (void*) alt_remap_uncached(EDRV_PKT_BASE, EDRV_PKT_SPAN);
    EdrvInstance_l.m_pBufBase = (void*) alt_remap_uncached(EDRV_PKT_BASE, EDRV_PKT_SPAN);
#elif defined(__MICROBLAZE__)
    EdrvInstance_l.m_EthConf.pBufBase = (void*) EDRV_PKT_BASE;
    EdrvInstance_l.m_pBufBase = (void*) EDRV_PKT_BASE;
#else
#error "Configuration unknown!"
#endif
    EdrvInstance_l.m_EthConf.pktLoc = OMETH_PKT_LOC_MACINT;
    EdrvInstance_l.m_dwBufSpan = EDRV_PKT_SPAN;
#elif EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_EXT
    //use heap as packet buffer
    EdrvInstance_l.m_EthConf.pBufBase = 0;
    EdrvInstance_l.m_EthConf.pktLoc = OMETH_PKT_LOC_HEAP;
#elif EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT
    //use heap as rx packet buffer
#ifdef __NIOS2__
    EdrvInstance_l.m_pBufBase = (void*) alt_remap_uncached(EDRV_PKT_BASE, EDRV_PKT_SPAN);
#elif defined(__MICROBLAZE__)
    EdrvInstance_l.m_pBufBase = (void*) EDRV_PKT_BASE;
#endif
    EdrvInstance_l.m_EthConf.pBufBase = 0;
    EdrvInstance_l.m_EthConf.pktLoc = OMETH_PKT_LOC_HEAP;
    EdrvInstance_l.m_dwBufSpan = EDRV_PKT_SPAN;
#else
#error "Configuration unknown!"
#endif

    EdrvInstance_l.m_EthConf.rxBuffers = EDRV_MAX_RX_BUFFERS;
    EdrvInstance_l.m_EthConf.rxMtu = EDRV_MAX_BUFFER_SIZE;

    EdrvInstance_l.m_hOpenMac = omethCreate(&EdrvInstance_l.m_EthConf);

    if (EdrvInstance_l.m_hOpenMac == 0)
    {
        Ret = kEplNoResource;
        PRINTF0(" error!\n");
        goto Exit;
    }

    //init driver struct
    EdrvInstance_l.m_dwMsgFree = 0;
    EdrvInstance_l.m_dwMsgSent = 0;

    //verify phy management
    EdrvInstance_l.m_ubPhyCnt = 0;

    for(i=0; i<EDRV_PHY_NUM; i++)
    {
        EdrvInstance_l.m_pPhy[i] = omethPhyInfo(EdrvInstance_l.m_hOpenMac, i);
        if(EdrvInstance_l.m_pPhy[i] != 0)
        {
            EdrvInstance_l.m_ubPhyCnt++;
        }
    }

    PRINTF1("%i phy found\n", EdrvInstance_l.m_ubPhyCnt);

    if(EdrvInstance_l.m_ubPhyCnt != EDRV_PHY_NUM)
    {
        PRINTF1(" -> but %i phy should be found!\n", EDRV_PHY_NUM);
        Ret = kEplNoResource;
        goto Exit;
    }

    // initialize the filters, so that they won't match any normal Ethernet frame
    EPL_MEMSET(abFilterMask, 0, sizeof(abFilterMask));
    EPL_MEMSET(abFilterMask, 0xFF, 6);
    EPL_MEMSET(abFilterValue, 0, sizeof(abFilterValue));

    // initialize RX hook
    EdrvInstance_l.m_hHook = omethHookCreate(EdrvInstance_l.m_hOpenMac, EdrvRxHook, 0); //last argument max. pending
    if (EdrvInstance_l.m_hHook == 0)
    {
        Ret = kEplNoResource;
        goto Exit;
    }

    for (i = 0; i < EDRV_MAX_FILTERS; i++)
    {
        EdrvInstance_l.m_ahFilter[i] = omethFilterCreate(EdrvInstance_l.m_hHook, (void*) i, abFilterMask, abFilterValue);
        if (EdrvInstance_l.m_ahFilter[i] == 0)
        {
            Ret = kEplNoResource;
            goto Exit;
        }

        omethFilterDisable(EdrvInstance_l.m_ahFilter[i]);
#if (EDRV_AUTO_RESPONSE == TRUE)
        if (i < EDRV_MAX_AUTO_RESPONSES)
        {
            int iRet;

            // initialize the auto response for each filter ...
            iRet = omethResponseInit(EdrvInstance_l.m_ahFilter[i]);
            if (iRet != 0)
            {
                Ret = kEplNoResource;
                goto Exit;
            }

            // ... but disable it
            omethResponseDisable(EdrvInstance_l.m_ahFilter[i]);
        }
#endif
    }

    //moved following lines here, since omethHookCreate may change tx buffer base!
#if EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT
    //get rx/tx buffer base
    EdrvInstance_l.m_pRxBufBase = omethGetRxBufBase(EdrvInstance_l.m_hOpenMac);
    EdrvInstance_l.m_pTxBufBase = omethGetTxBufBase(EdrvInstance_l.m_hOpenMac);
#elif EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT
    //get tx buffer base
#ifdef __NIOS2__
    EdrvInstance_l.m_pTxBufBase = (void*) alt_remap_uncached(EDRV_PKT_BASE, EDRV_PKT_SPAN);
#elif defined(__MICROBLAZE__)
    EdrvInstance_l.m_pTxBufBase = (void*) EDRV_PKT_BASE;
#endif
#endif

    // $$$ d.k. additional Filters for own MAC address and broadcast MAC
    //          will be necessary, if Virtual Ethernet driver is available

    ///////////////////////////
    // start Ethernet Driver //
    ///////////////////////////
    omethStart(EdrvInstance_l.m_hOpenMac, TRUE);
    PRINTF0("Ethernet Driver started\n");

    ////////////////////
    // link NIOS' irq //
    ////////////////////

// register openMAC Rx and Tx IRQ
#ifdef __NIOS2__
    if (alt_ic_isr_register(EDRV_MAC_IRQ_IC_ID, EDRV_MAC_IRQ,
                EdrvIrqHandler, EdrvInstance_l.m_hOpenMac, NULL))
    {
        Ret = kEplNoResource;
    }
#elif defined(__MICROBLAZE__)
    {
        DWORD curIntEn = EDRV_RD32(EDRV_INTC_BASE, XIN_IER_OFFSET);

        XIntc_RegisterHandler(EDRV_INTC_BASE, EDRV_MAC_IRQ,
                (XInterruptHandler)EdrvIrqHandler, (void*)EdrvInstance_l.m_hOpenMac);

        XIntc_EnableIntr(EDRV_INTC_BASE, EDRV_MAC_IRQ_MASK | curIntEn);
    }
#else
#error "Configuration unknown!"
#endif

    //wait some time (phy may not be ready...)
    EDRV_USLEEP(1*1000*1000);

Exit:
    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvShutdown
//
// Description: Shutdown the Ethernet controller
//
// Parameters:  void
//
// Returns:     Errorcode   = kEplSuccessful
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvShutdown(void)
{

#ifdef EDRV_DEBUG
    //before we stop openMAC, wait for all packets to be sent (might take some time...)
    while(EdrvInstance_l.m_dwMsgSent != EdrvInstance_l.m_dwMsgFree);
#endif

    omethStop(EdrvInstance_l.m_hOpenMac);

// deregister openMAC Rx and Tx IRQ
#ifdef __NIOS2__
    alt_ic_isr_register(EDRV_MAC_IRQ_IC_ID, EDRV_MAC_IRQ, NULL, NULL, NULL);
#elif defined(__MICROBLAZE__)
    XIntc_RegisterHandler(EDRV_INTC_BASE, EDRV_MAC_IRQ,
                (XInterruptHandler)NULL, (void*)NULL);
#else
#error "again, configuration unknown!"
#endif

#ifdef EDRV_DEBUG
    //okay, before we destroy openMAC, observe its statistics!
    {
        ometh_stat_typ *pMacStat = NULL;

        pMacStat = omethStatistics(EdrvInstance_l.m_hOpenMac);

        if( pMacStat == NULL )
        {
            PRINTF0("Serious error occurred!? Can't find the statistics!\n");
        }
        else
        {
            PRINTF0("--- omethStatistics ---\n");
            PRINTF0("----  RX           ----\n");
            PRINTF1(" CRC ERROR = %i\n",         (int)pMacStat->rxCrcError);
            PRINTF1(" HOOK DISABLED = %i\n",     (int)pMacStat->rxHookDisabled);
            PRINTF1(" HOOK OVERFLOW = %i\n",     (int)pMacStat->rxHookOverflow);
            PRINTF1(" LOST = %i\n",                 (int)pMacStat->rxLost);
            PRINTF1(" OK = %i\n",                 (int)pMacStat->rxOk);
            PRINTF1(" OVERSIZE = %i\n",             (int)pMacStat->rxOversize);
            PRINTF0("----  TX           ----\n");
            PRINTF1(" COLLISION = %i\n",         (int)pMacStat->txCollision);
            PRINTF1(" DONE = %i\n",                 (int)pMacStat->txDone[0]);
            PRINTF1(" SPURIOUS IRQ = %i\n",         (int)pMacStat->txSpuriousInt);
        }
        PRINTF0("\n");
    }

#if EDRV_DMA_OBSERVER != 0
    if( EdrvInstance_l.m_fDmaError == TRUE )
    {
        //if you see this the openMAC DMA is connected to slow memory!
        // -> use embedded memory or 10 nsec SRAM!!!
        printf("OPENMAC DMA TRANSFER ERROR\n");
    }
#endif
#endif

    PRINTF0("Shutdown Ethernet Driver... ");

    if (omethDestroy(EdrvInstance_l.m_hOpenMac) != 0) {
        PRINTF0("error\n");
        return kEplNoResource;
    }
    PRINTF0("done\n");

    return kEplSuccessful;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvAllocTxMsgBuffer
//
// Description:
//
// Parameters:  pBuffer_p   = pointer to Buffer structure
//
// Returns:     Errorcode   = kEplSuccessful
//                          = kEplEdrvNoFreeBufEntry
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvAllocTxMsgBuffer       (tEdrvTxBuffer * pBuffer_p)
{
tEplKernel          Ret = kEplSuccessful;
ometh_packet_typ*   pPacket = NULL;

    if (pBuffer_p->m_uiMaxBufferLen > EDRV_MAX_BUFFER_SIZE)
    {
        Ret = kEplEdrvNoFreeBufEntry;
        goto Exit;
    }

    //openMAC does no padding, use memory for padding
    if( pBuffer_p->m_uiMaxBufferLen < MIN_ETH_SIZE)
    {
        pBuffer_p->m_uiMaxBufferLen = MIN_ETH_SIZE;
    }

#ifdef EDRV_DEBUG
    PRINTF2("%s: allocate %i bytes\n", __func__, (int)(pBuffer_p->m_uiMaxBufferLen + sizeof (pPacket->length)));
#endif

#if EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_EXT
    // malloc aligns each allocated buffer so every type fits into this buffer.
    // this means 8 Byte alignment.
#ifdef __NIOS2__
    pPacket = (ometh_packet_typ*) alt_uncached_malloc(pBuffer_p->m_uiMaxBufferLen + sizeof (pPacket->length));
#elif defined(__MICROBLAZE__)
    pPacket = (ometh_packet_typ*) malloc(pBuffer_p->m_uiMaxBufferLen + sizeof (pPacket->length));
#else
#error "Configuration unknown!"
#endif
#elif (EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT || EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT)
    {
        static void *pNextBuffer = NULL;
            //stores the next buffer to be allocated
        int iBufLength = pBuffer_p->m_uiMaxBufferLen + sizeof (pPacket->length);
            //gives the length of the buffer to be allocated
        void *pBufHighAddr = (void*)(EdrvInstance_l.m_pBufBase + EdrvInstance_l.m_dwBufSpan);
            //gives the high address of the packet buffer (upper limit)
        void *pBufBaseAddr = (void*)(EdrvInstance_l.m_pTxBufBase);
            //gives the base address of the TX part in the packet buffer (lower limit)

        //check if buffers are allocated
        if ( EdrvInstance_l.m_ubTxBufCnt == 0 )
        {
            pPacket = (ometh_packet_typ*)EdrvInstance_l.m_pTxBufBase; //first buffer allocation
        }
        else
        {
            pPacket = (ometh_packet_typ*)pNextBuffer; //use next buffer for allocation
        }

        //check if new buffer is within buffer limits
        {
            void *p = (void*)pPacket;
            int i;

            //first, test if packet buffer base is within range
            //second, test if packet buffer high is within range
            for(i=0; i<2; i++)
            {
                if( (p > pBufHighAddr) || (p < pBufBaseAddr) )
                {
                    PRINTF0("MAC-internal buffer overflow!\n");
                    Ret = kEplEdrvNoFreeBufEntry;
                    goto Exit;
                }
                p += iBufLength;
            }
        }

        //set new buffer to zeros
        memset((void*)pPacket, 0, iBufLength); //set zeros

        //calculate next buffer for next allocation
        pNextBuffer = (((void*)pPacket) + iBufLength);
        //align buffer
        {   DWORD tmp = (DWORD)pNextBuffer;
            tmp += 3; tmp &= ~3;
            pNextBuffer = (void*)tmp;
        }

        EdrvInstance_l.m_ubTxBufCnt++; //new buffer added
    }
#else
#error "Configuration unknown!"
#endif
    if (pPacket == NULL)
    {
        Ret = kEplEdrvNoFreeBufEntry;
        goto Exit;
    }

    pPacket->length = pBuffer_p->m_uiMaxBufferLen;

    pBuffer_p->m_BufferNumber.m_dwVal = EDRV_MAX_FILTERS;

    pBuffer_p->m_pbBuffer = (BYTE*) &pPacket->data;

Exit:
    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvReleaseTxMsgBuffer
//
// Description:
//
// Parameters:  pBuffer_p   = pointer to Buffer structure
//
// Returns:     Errorcode   = kEplSuccessful
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvReleaseTxMsgBuffer     (tEdrvTxBuffer * pBuffer_p)
{
tEplKernel          Ret = kEplSuccessful;
ometh_packet_typ*   pPacket = NULL;

    if (pBuffer_p->m_BufferNumber.m_dwVal < EDRV_MAX_FILTERS)
    {
        // disable auto-response
        omethResponseDisable(EdrvInstance_l.m_ahFilter[pBuffer_p->m_BufferNumber.m_dwVal]);
    }

    if (pBuffer_p->m_pbBuffer == NULL)
    {
        Ret = kEplEdrvInvalidParam;
        goto Exit;
    }

    pPacket = GET_TYPE_BASE(ometh_packet_typ, data, pBuffer_p->m_pbBuffer);

    // mark buffer as free, before actually freeing it
    pBuffer_p->m_pbBuffer = NULL;
#if EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_EXT
    //free tx buffer
#ifdef __NIOS2__
    alt_uncached_free(pPacket);
#elif defined(__MICROBLAZE__)
    free(pPacket);
#endif
#elif (EDRV_PKT_LOC == EDRV_PKT_LOC_TX_RX_INT || EDRV_PKT_LOC == EDRV_PKT_LOC_TX_INT_RX_EXT)
    EdrvInstance_l.m_ubTxBufCnt--;
#else
#error "Configuration unknown"
#endif

Exit:
    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvUpdateTxMsgBuffer
//
// Description: Update tx-message buffer for use with auto-response filter
//
// Parameters:  pBuffer_p   = pointer to Buffer structure
//
// Returns:     Errorcode   = kEplSuccessful
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvUpdateTxMsgBuffer     (tEdrvTxBuffer * pBuffer_p)
{
tEplKernel          Ret = kEplSuccessful;
ometh_packet_typ*   pPacket = NULL;

//#if EDRV_DMA_OBSERVER != 0
//    if( EdrvInstance_l.m_fDmaError == TRUE )
//    {
//        //provoke error to kill the node
//        Ret = kEplEdrvInvalidParam;
//        goto Exit;
//    }
//#endif

    if (pBuffer_p->m_BufferNumber.m_dwVal >= EDRV_MAX_FILTERS)
    {
        Ret = kEplEdrvInvalidParam;
        goto Exit;
    }

    pPacket = GET_TYPE_BASE(ometh_packet_typ, data, pBuffer_p->m_pbBuffer);

    pPacket->length = pBuffer_p->m_uiTxMsgLen;

#if XPAR_MICROBLAZE_USE_DCACHE && XPAR_MICROBLAZE_DCACHE_USE_WRITEBACK
    /*
     * before handing over the packet buffer to openMAC
     * flush the packet's memory range due to write-back policy
     */
    microblaze_flush_dcache_range((DWORD)pPacket, pPacket->length);
#endif

    // Update autoresponse buffer
    EdrvInstance_l.m_apTxBuffer[pBuffer_p->m_BufferNumber.m_dwVal] = pBuffer_p;

    pPacket = omethResponseSet(EdrvInstance_l.m_ahFilter[pBuffer_p->m_BufferNumber.m_dwVal], pPacket);
    if (pPacket == OMETH_INVALID_PACKET)
    {
        Ret = kEplNoResource;
        goto Exit;
    }

Exit:
    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvSendTxMsg
//
// Description:
//
// Parameters:  pBuffer_p   = buffer descriptor to transmit
//
// Returns:     Errorcode   = kEplSuccessful
//                          = kEplEdrvNoFreeBufEntry
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvSendTxMsg              (tEdrvTxBuffer * pBuffer_p)
{
tEplKernel          Ret = kEplSuccessful;
ometh_packet_typ*   pPacket = NULL;
unsigned long       ulTxLength;

//#if EDRV_DMA_OBSERVER != 0
//    if( EdrvInstance_l.m_fDmaError == TRUE )
//    {
//        //provoke error to kill the node
//        Ret = kEplEdrvNoFreeBufEntry;
//        goto Exit;
//    }
//#endif

#ifndef EDRV_TTTX
    if (pBuffer_p->m_BufferNumber.m_dwVal < EDRV_MAX_FILTERS)
    {
        Ret = kEplEdrvInvalidParam;
        goto Exit;
    }
#endif

    pPacket = GET_TYPE_BASE(ometh_packet_typ, data, pBuffer_p->m_pbBuffer);

    pPacket->length = pBuffer_p->m_uiTxMsgLen;
#ifdef EDRV_TTTX
    if( (pBuffer_p->m_dwTimeOffsetAbsTk & 1) == 1)
    {
        //free tx descriptors available
        ulTxLength = omethTransmitTime(EdrvInstance_l.m_hOpenMac, pPacket,
                        EdrvCbSendAck, pBuffer_p, pBuffer_p->m_dwTimeOffsetAbsTk);

        if( ulTxLength == 0 )
        {
#ifdef EDRV_2NDTXQUEUE
            //time triggered sent failed => move to 2nd tx queue
            if( (EdrvInstance_l.m_iTxQueueWr - EdrvInstance_l.m_iTxQueueRd) >= EDRV_MAX_TX_BUF2)
            {
                PRINTF0("\n***Queue is FULL!***\n\n");
                Ret = kEplEdrvNoFreeBufEntry;
                goto Exit;
            }
            else
            {
                EdrvInstance_l.m_apTxQueue[EdrvInstance_l.m_iTxQueueWr & (EDRV_MAX_TX_BUF2-1)] = pBuffer_p;
                EdrvInstance_l.m_iTxQueueWr++;
                Ret = kEplSuccessful;
                goto Exit; //packet will be sent!
            }
#else
            PRINTF0("\n***No TX Descriptor available!***\n\n");
            Ret = kEplEdrvNoFreeBufEntry;
            goto Exit;
#endif
        }
    }
    else
    {
#endif
        ulTxLength = omethTransmitArg(EdrvInstance_l.m_hOpenMac, pPacket,
                            EdrvCbSendAck, pBuffer_p);
#ifdef EDRV_TTTX
    }
#endif

    if (ulTxLength > 0)
    {
        EdrvInstance_l.m_dwMsgSent++;
        Ret = kEplSuccessful;
    }
    else
    {
        Ret = kEplEdrvNoFreeBufEntry;
    }

Exit:
    if( Ret != kEplSuccessful )
    {
        BENCHMARK_MOD_01_TOGGLE(7);
    }

    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvChangeFilter
//
// Description: Change all rx-filters or one specific rx-filter
//              of the openMAC
//
// Parameters:  pFilter_p           = pointer to array of filter entries
//              uiCount_p           = number of filters in array
//              uiEntryChanged_p    = selects one specific filter which is
//                                    to be changed. If value is equal to
//                                    or larger than uiCount_p, all entries
//                                    are selected.
//              uiChangeFlags_p     = If one specific entry is selected,
//                                    these flag bits show which filter
//                                    properties have been changed.
//                                    available flags:
//                                      EDRV_FILTER_CHANGE_MASK
//                                      EDRV_FILTER_CHANGE_VALUE
//                                      EDRV_FILTER_CHANGE_STATE
//                                      EDRV_FILTER_CHANGE_AUTO_RESPONSE
//                                    if auto-response delay is supported:
//                                      EDRV_FILTER_CHANGE_AUTO_RESPONSE_DELAY
//
// Returns:     Errorcode           = kEplSuccessful
//                                  = kEplEdrvInvalidParam
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvChangeFilter(tEdrvFilter*    pFilter_p,
                            unsigned int    uiCount_p,
                            unsigned int    uiEntryChanged_p,
                            unsigned int    uiChangeFlags_p)
{
tEplKernel      Ret = kEplSuccessful;
unsigned int    uiIndex;
unsigned int    uiEntry;

    if (((uiCount_p != 0) && (pFilter_p == NULL))
        || (uiCount_p >= EDRV_MAX_AUTO_RESPONSES))
    {
        Ret = kEplEdrvInvalidParam;
        goto Exit;
    }

    if (uiEntryChanged_p >= uiCount_p)
    {   // no specific entry changed
        // -> all entries changed

        // at first, disable all filters in openMAC
        for (uiEntry = 0; uiEntry < EDRV_MAX_FILTERS; uiEntry++)
        {
            omethFilterDisable(EdrvInstance_l.m_ahFilter[uiEntry]);
            omethResponseDisable(EdrvInstance_l.m_ahFilter[uiEntry]);
        }

        for (uiEntry = 0; uiEntry < uiCount_p; uiEntry++)
        {
            // set filter value and mask
            for (uiIndex = 0; uiIndex < sizeof (pFilter_p->m_abFilterValue); uiIndex++)
            {
                omethFilterSetByteValue(EdrvInstance_l.m_ahFilter[uiEntry],
                                        uiIndex,
                                        pFilter_p[uiEntry].m_abFilterValue[uiIndex]);

                omethFilterSetByteMask(EdrvInstance_l.m_ahFilter[uiEntry],
                                       uiIndex,
                                       pFilter_p[uiEntry].m_abFilterMask[uiIndex]);
            }

            // set auto response
            if (pFilter_p[uiEntry].m_pTxBuffer != NULL)
            {
                EdrvInstance_l.m_apTxBuffer[uiEntry] = pFilter_p[uiEntry].m_pTxBuffer;

                // set buffer number of TxBuffer to filter entry
                pFilter_p[uiEntry].m_pTxBuffer[0].m_BufferNumber.m_dwVal = uiEntry;
                pFilter_p[uiEntry].m_pTxBuffer[1].m_BufferNumber.m_dwVal = uiEntry;
                EdrvUpdateTxMsgBuffer(pFilter_p[uiEntry].m_pTxBuffer);
                omethResponseEnable(EdrvInstance_l.m_ahFilter[uiEntry]);

#if EDRV_AUTO_RESPONSE_DELAY != FALSE
                {
                DWORD dwDelayNs;

                    // set auto-response delay
                    dwDelayNs = pFilter_p[uiEntry].m_pTxBuffer->m_dwTimeOffsetNs;
                    if (dwDelayNs == 0)
                    {   // no auto-response delay is set
                        // send frame immediately after IFG
                        omethResponseTime(EdrvInstance_l.m_ahFilter[uiEntry], 0);
                    }
                    else
                    {   // auto-response delay is set
                    DWORD dwDelayAfterIfgNs;

                        if (dwDelayNs < EPL_C_DLL_T_IFG)
                        {   // set delay to a minimum of IFG
                            dwDelayNs = EPL_C_DLL_T_IFG;
                        }
                        dwDelayAfterIfgNs = dwDelayNs - EPL_C_DLL_T_IFG;
                        omethResponseTime(EdrvInstance_l.m_ahFilter[uiEntry],
                                          OMETH_NS_2_TICKS(dwDelayAfterIfgNs));
                    }
                }
#endif
            }

            if (pFilter_p[uiEntry].m_fEnable != FALSE)
            {   // enable the filter
                omethFilterEnable(EdrvInstance_l.m_ahFilter[uiEntry]);
            }
        }
    }
    else
    {   // specific entry should be changed

        if (((uiChangeFlags_p & (EDRV_FILTER_CHANGE_VALUE
                                 | EDRV_FILTER_CHANGE_MASK
#if EDRV_AUTO_RESPONSE_DELAY != FALSE
                                 | EDRV_FILTER_CHANGE_AUTO_RESPONSE_DELAY
#endif
                                 | EDRV_FILTER_CHANGE_AUTO_RESPONSE)) != 0)
            || (pFilter_p[uiEntryChanged_p].m_fEnable == FALSE))
        {
            // disable this filter entry
            omethFilterDisable(EdrvInstance_l.m_ahFilter[uiEntryChanged_p]);

            if ((uiChangeFlags_p & EDRV_FILTER_CHANGE_VALUE) != 0)
            {   // filter value has changed
                for (uiIndex = 0; uiIndex < sizeof (pFilter_p->m_abFilterValue); uiIndex++)
                {
                    omethFilterSetByteValue(EdrvInstance_l.m_ahFilter[uiEntryChanged_p],
                                            uiIndex,
                                            pFilter_p[uiEntryChanged_p].m_abFilterValue[uiIndex]);
                }
            }

            if ((uiChangeFlags_p & EDRV_FILTER_CHANGE_MASK) != 0)
            {   // filter mask has changed
                for (uiIndex = 0; uiIndex < sizeof (pFilter_p->m_abFilterMask); uiIndex++)
                {
                    omethFilterSetByteMask(EdrvInstance_l.m_ahFilter[uiEntryChanged_p],
                                           uiIndex,
                                           pFilter_p[uiEntryChanged_p].m_abFilterMask[uiIndex]);
                }
            }

            if ((uiChangeFlags_p & EDRV_FILTER_CHANGE_AUTO_RESPONSE) != 0)
            {   // filter auto-response state or frame has changed
                if (pFilter_p[uiEntryChanged_p].m_pTxBuffer != NULL)
                {   // auto-response enable
                    // set buffer number of TxBuffer to filter entry
                    pFilter_p[uiEntryChanged_p].m_pTxBuffer[0].m_BufferNumber.m_dwVal = uiEntryChanged_p;
                    pFilter_p[uiEntryChanged_p].m_pTxBuffer[1].m_BufferNumber.m_dwVal = uiEntryChanged_p;
                    EdrvUpdateTxMsgBuffer(pFilter_p[uiEntryChanged_p].m_pTxBuffer);
                    omethResponseEnable(EdrvInstance_l.m_ahFilter[uiEntryChanged_p]);
                }
                else
                {   // auto-response disable
                    omethResponseDisable(EdrvInstance_l.m_ahFilter[uiEntryChanged_p]);
                }
            }

#if EDRV_AUTO_RESPONSE_DELAY != FALSE
            if ((uiChangeFlags_p & EDRV_FILTER_CHANGE_AUTO_RESPONSE_DELAY) != 0)
            {   // filter auto-response delay has changed
            DWORD dwDelayNs;

                if (pFilter_p[uiEntryChanged_p].m_pTxBuffer == NULL)
                {
                    Ret = kEplEdrvInvalidParam;
                    goto Exit;
                }
                dwDelayNs = pFilter_p[uiEntryChanged_p].m_pTxBuffer->m_dwTimeOffsetNs;

                if (dwDelayNs == 0)
                {   // no auto-response delay is set
                    // send frame immediately after IFG
                    omethResponseTime(EdrvInstance_l.m_ahFilter[uiEntryChanged_p], 0);
                }
                else
                {   // auto-response delay is set
                DWORD dwDelayAfterIfgNs;

                    if (dwDelayNs < EPL_C_DLL_T_IFG)
                    {   // set delay to a minimum of IFG
                        dwDelayNs = EPL_C_DLL_T_IFG;
                    }
                    dwDelayAfterIfgNs = dwDelayNs - EPL_C_DLL_T_IFG;
                    omethResponseTime(EdrvInstance_l.m_ahFilter[uiEntryChanged_p],
                                      OMETH_NS_2_TICKS(dwDelayAfterIfgNs));
                }
            }
#endif
        }

        if (pFilter_p[uiEntryChanged_p].m_fEnable != FALSE)
        {   // enable the filter
            omethFilterEnable(EdrvInstance_l.m_ahFilter[uiEntryChanged_p]);
        }
    }

Exit:
    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvDefineRxMacAddrEntry
//
// Description: Set a multicast entry into the Ethernet controller
//
// Parameters:  pbMacAddr_p     = pointer to multicast entry to set
//
// Returns:     Errorcode       = kEplSuccessful
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvDefineRxMacAddrEntry (BYTE * pbMacAddr_p)
{
tEplKernel  Ret = kEplSuccessful;

    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvUndefineRxMacAddrEntry
//
// Description: Reset a multicast entry in the Ethernet controller
//
// Parameters:  pbMacAddr_p     = pointer to multicast entry to reset
//
// Returns:     Errorcode       = kEplSuccessful
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvUndefineRxMacAddrEntry (BYTE * pbMacAddr_p)
{
tEplKernel  Ret = kEplSuccessful;

    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvTxMsgReady
//
// Description: starts copying the buffer to the ethernet controller's FIFO
//
// Parameters:  pbBuffer_p - bufferdescriptor to transmit
//
// Returns:     Errorcode - kEplSuccessful
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvTxMsgReady              (tEdrvTxBuffer * pBuffer_p)
{
    tEplKernel Ret = kEplSuccessful;

    return Ret;
}


//---------------------------------------------------------------------------
//
// Function:    EdrvTxMsgStart
//
// Description: starts transmission of the ethernet controller's FIFO
//
// Parameters:  pbBuffer_p - bufferdescriptor to transmit
//
// Returns:     Errorcode - kEplSuccessful
//
// State:
//
//---------------------------------------------------------------------------

tEplKernel EdrvTxMsgStart              (tEdrvTxBuffer * pBuffer_p)
{
tEplKernel Ret = kEplSuccessful;

    return Ret;
}


//=========================================================================//
//                                                                         //
//          P R I V A T E   F U N C T I O N S                              //
//                                                                         //
//=========================================================================//

//---------------------------------------------------------------------------
//
// Function:     EdrvInterruptHandler
//
// Description:  interrupt handler
//
// Parameters:   void
//
// Returns:      void
//
// State:
//
//---------------------------------------------------------------------------

static void EdrvIrqHandler (void* pArg_p
#ifndef ALT_ENHANCED_INTERRUPT_API_PRESENT
        , DWORD dwInt_p
#endif
        )
{

#ifdef CPU_UTIL
    isrcall_cpuutil();
#endif

#if EDRV_DMA_OBSERVER != 0
    //read DMA observer feature
    if( EDRV_RD16(EDRV_DOB_BASE, 0) != 0 )
    {
        EdrvInstance_l.m_fDmaError = TRUE;
        BENCHMARK_MOD_01_TOGGLE(7);

        omethStop(pArg_p); //since openMAC was naughty, stop it!
    }
#endif

    //handle sent packets
    while( EDRV_RD32(EDRV_IRQ_BASE, 0) & 0x1 )
    {
        omethTxIrqHandler(pArg_p);
    }

#if (defined(EDRV_2NDTXQUEUE) && defined(EDRV_TTTX))
    //observe additional TX queue and send packet if necessary
    while( (EdrvInstance_l.m_iTxQueueWr - EdrvInstance_l.m_iTxQueueRd) &&
        (omethTransmitPending(EdrvInstance_l.m_hOpenMac) < 16U) )
    {
        tEdrvTxBuffer*         pBuffer_p;
        ometh_packet_typ*   pPacket;
        unsigned long       ulTxLength = 0U;

        pBuffer_p = EdrvInstance_l.m_apTxQueue[EdrvInstance_l.m_iTxQueueRd & (EDRV_MAX_TX_BUF2-1)];

        pPacket = GET_TYPE_BASE(ometh_packet_typ, data, pBuffer_p->m_pbBuffer);

        pPacket->length = pBuffer_p->m_uiTxMsgLen;

        //offset is the openMAC time tick (no conversion needed)
        ulTxLength = omethTransmitTime(EdrvInstance_l.m_hOpenMac, pPacket,
                        EdrvCbSendAck, pBuffer_p, pBuffer_p->m_dwTimeOffsetAbsTk);

        if( ulTxLength > 0 )
        {
            EdrvInstance_l.m_iTxQueueRd++;
            EdrvInstance_l.m_dwMsgSent++;
        }
        else
        {
            //no tx descriptor is free
        }
    }
#endif

    //handle received packets
    if( EDRV_RD32(EDRV_IRQ_BASE, 0) & 0x2 )
    {
        omethRxIrqHandler(pArg_p);
    }
}

//---------------------------------------------------------------------------
//
// Function:    EdrvCbSendAck
//
// Description: Tx callback function from openMAC
//
// Parameters:  *pPacket        = packet which should be released
//
// Returns:     void
//
// State:
//
//---------------------------------------------------------------------------
static void EdrvCbSendAck(ometh_packet_typ *pPacket, void *arg, unsigned long time)
{
    EdrvInstance_l.m_dwMsgFree++;
    BENCHMARK_MOD_01_SET(1);

    if (arg != NULL)
    {
    tEdrvTxBuffer*  pTxBuffer = arg;

        if (pTxBuffer->m_pfnTxHandler != NULL)
        {
            pTxBuffer->m_pfnTxHandler(pTxBuffer);
        }
    }

    BENCHMARK_MOD_01_RESET(1);
}


//---------------------------------------------------------------------------
//
// Function:    EdrvRxHook
//
// Description: This function will be called out of the Interrupt, when the
//              received packet fits to the filter
//
// Parameters:  arg         = user specific argument pointer
//              pPacket     = pointer to received packet
//              pFct        = pointer to free function (don't care)
//
// Returns:     0 if frame was used
//              -1 if frame was not used
//
// State:
//
//---------------------------------------------------------------------------

static int EdrvRxHook(void *arg, ometh_packet_typ  *pPacket, OMETH_BUF_FREE_FCT  *pFct)
{
tEdrvRxBuffer       rxBuffer;
unsigned int        uiIndex;
tEplTgtTimeStamp    TimeStamp;

    BENCHMARK_MOD_01_SET(6);
    rxBuffer.m_BufferInFrame = kEdrvBufferLastInFrame;
    rxBuffer.m_pbBuffer = (BYTE *) &pPacket->data;
    rxBuffer.m_uiRxMsgLen = pPacket->length;
    TimeStamp.m_dwTimeStamp = omethGetTimestamp(pPacket);
    rxBuffer.m_pTgtTimeStamp = &TimeStamp;

#if XPAR_MICROBLAZE_USE_DCACHE
    /*
     * before handing over the received packet to the stack
     * flush the packet's memory range
     */
    microblaze_flush_dcache_range((DWORD)pPacket, pPacket->length);
#endif

    EdrvInstance_l.m_InitParam.m_pfnRxHandler(&rxBuffer); //pass frame to Powerlink Stack

    uiIndex = (unsigned int) arg;

    if (EdrvInstance_l.m_apTxBuffer[uiIndex] != NULL)
    {   // filter with auto-response frame triggered
        BENCHMARK_MOD_01_SET(5);
        // call Tx handler function from DLL
        if (EdrvInstance_l.m_apTxBuffer[uiIndex]->m_pfnTxHandler != NULL)
        {
            EdrvInstance_l.m_apTxBuffer[uiIndex]->m_pfnTxHandler(EdrvInstance_l.m_apTxBuffer[uiIndex]);
        }
        BENCHMARK_MOD_01_RESET(5);
    }

    BENCHMARK_MOD_01_RESET(6);

    return 0;
}

