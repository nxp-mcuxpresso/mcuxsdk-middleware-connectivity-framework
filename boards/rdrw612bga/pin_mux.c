/*
 * Copyright 2021 NXP.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pin_mux.h"
#include "fsl_common.h"
#include "fsl_io_mux.h"

/* FUNCTION ************************************************************************************************************
 *
 * Function Name : BOARD_InitBootPins
 * Description   : Calls initialization functions.
 *
 * END ****************************************************************************************************************/
void BOARD_InitBootPins(void)
{
    BOARD_InitPins();
    BOARD_InitQuadSpiFlashPins();
}

/* FUNCTION ************************************************************************************************************
 *
 * Function Name : BOARD_InitPins
 * Description   : Configures pin routing and optionally pin electrical features.
 *
 * END ****************************************************************************************************************/
void BOARD_InitPins(void)
{ /*!< Function assigned for the core: Cortex-M33[cm33] */
    MCI_IO_MUX->FC3 = MCI_IO_MUX_FC3_SEL_FC3_USART_DATA_MASK;
    AON_SOC_CIU->MCI_IOMUX_EN0 |= 0x05000000U;

    /* Enable GPIO2 - for debugging */
    MCI_IO_MUX->GPIO_GRP0 |= 0x00000004U;
    SOCCTRL->MCI_IOMUX_EN0 |= 0x00000004U;
}

void BOARD_InitQuadSpiFlashPins(void)
{
    IO_MUX_SetPinMux(IO_MUX_QUAD_SPI_FLASH);
}

/***********************************************************************************************************************
 * EOF
 **********************************************************************************************************************/
