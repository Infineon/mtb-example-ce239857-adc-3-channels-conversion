/******************************************************************************
* File Name:   main.c
*
* Description: This code example demonstrates how to configure ADC 3 channels to
* simultaneous sample by PWM trigger and store the result in the FIFO.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cy_pdl.h"
#include "mtb_hal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* The number of ADC channels */
#define ADC_CHANNNELS_NUM                   (16U)

/*******************************************************************************
* Global Variables
********************************************************************************/
/* For the Retarget -IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    DEBUG_UART_context;           /** UART context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj;           /** Debug UART HAL object  */

/* The TCPWM interrupt configuration structure */
cy_stc_sysint_t tcpwm_intr_config =
{
    .intrSrc = USER_TCPWM_IRQ,
    .intrPriority = 0U,
};

/* ADC channel result buffer */
volatile uint8_t adc_conversion_done = 0U;
uint16_t adc_result_buf[ADC_CHANNNELS_NUM] = {0U};
uint8_t channel_id0 = 0U;
uint8_t channel_id1 = 0U;
uint8_t channel_id2 = 0U;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
/* TCPWM interrupt handler */
void user_tcpwm_intr_handler(void);

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function. It configure three channels (AN_A0, AN_A1 and AN_A7) 
* of SAR ADC group-0 for simultaneous sampling. 
* Configured TCPWM to generate 1 sec ISR (user_tcpwm_intr_handler)and to give 
* triggers to the ADC conversion.Conversion results are printed by UART.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Start the HPPASS autonomous controller (AC) from state 0*/
    if(CY_HPPASS_SUCCESS != Cy_HPPASS_AC_Start(0U, 1000U))
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io to use the debug UART port */
    result = (cy_rslt_t)Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);

    /* UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config, &DEBUG_UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* HAL retarget_io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }


    /* Initialize TCPWM using the config structure generated using device configurator*/
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_PWM_Init(USER_TCPWM_HW,USER_TCPWM_NUM, &USER_TCPWM_config))
    {
        CY_ASSERT(0);
    }
    /* Enable the initialized TCPWM */
    Cy_TCPWM_PWM_Enable(USER_TCPWM_HW, USER_TCPWM_NUM);


    /* Configure user TCPWM interrupt */
    Cy_SysInt_Init(&tcpwm_intr_config, user_tcpwm_intr_handler);
    NVIC_EnableIRQ(tcpwm_intr_config.intrSrc);


    /* Enable global interrupts */
    __enable_irq();

    /*Start the timer*/
    Cy_TCPWM_TriggerStart_Single(USER_TCPWM_HW, USER_TCPWM_NUM);

    for (;;)
    {
        if(adc_conversion_done)
        {
            adc_conversion_done = 0U;
            printf("\x1b[2J\x1b[;H");
            printf("ADC Result - AN_A%d: 0x%x, AN_A%d: 0x%x, AN_A%d: 0x%x\r\n\r\n", \
                   channel_id0, adc_result_buf[channel_id0], channel_id1, adc_result_buf[channel_id1], channel_id2, adc_result_buf[channel_id2]);
        }
    }

}

/*******************************************************************************
* Function Name: user_tcpwm_intr_handler
********************************************************************************
* Summary:
* This is the TCPWM interrupt handler. This ISR read the ADC channel results 
* and toggle the user LED.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void user_tcpwm_intr_handler(void)
{
    uint32_t intrStatus = Cy_TCPWM_GetInterruptStatusMasked(USER_TCPWM_HW, USER_TCPWM_NUM);

    Cy_TCPWM_ClearInterrupt(USER_TCPWM_HW, USER_TCPWM_NUM, intrStatus);

    /* Read all data from FIFO 0 */
    adc_result_buf[channel_id0] = Cy_HPPASS_FIFO_Read(0U, &channel_id0);
    adc_result_buf[channel_id1] = Cy_HPPASS_FIFO_Read(0U, &channel_id1);
    adc_result_buf[channel_id2] = Cy_HPPASS_FIFO_Read(0U, &channel_id2);

    /* Invert the USER LED state */
    Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);

    adc_conversion_done = 1U;

}

/* [] END OF FILE */
