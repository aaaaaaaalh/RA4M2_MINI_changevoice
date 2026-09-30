/* generated vector source file - do not edit */
#include "bsp_api.h"
/* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
#if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = sci_uart_rxi_isr, /* SCI9 RXI (Receive data full) */
            [1] = sci_uart_txi_isr, /* SCI9 TXI (Transmit data empty) */
            [2] = sci_uart_tei_isr, /* SCI9 TEI (Transmit end) */
            [3] = sci_uart_eri_isr, /* SCI9 ERI (Receive error) */
            [4] = r_icu_isr, /* ICU IRQ6 (External pin interrupt 6) */
            [5] = adc_scan_end_isr, /* ADC0 SCAN END (End of A/D scanning operation) */
            [6] = iic_master_rxi_isr, /* IIC0 RXI (Receive data full) */
            [7] = iic_master_txi_isr, /* IIC0 TXI (Transmit data empty) */
            [8] = iic_master_tei_isr, /* IIC0 TEI (Transmit end) */
            [9] = iic_master_eri_isr, /* IIC0 ERI (Transfer error) */
            [10] = dmac_int_isr, /* DMAC0 INT (DMAC0 transfer end) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_SCI9_RXI,GROUP0), /* SCI9 RXI (Receive data full) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TXI,GROUP1), /* SCI9 TXI (Transmit data empty) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TEI,GROUP2), /* SCI9 TEI (Transmit end) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_SCI9_ERI,GROUP3), /* SCI9 ERI (Receive error) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_ICU_IRQ6,GROUP4), /* ICU IRQ6 (External pin interrupt 6) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_ADC0_SCAN_END,GROUP5), /* ADC0 SCAN END (End of A/D scanning operation) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_IIC0_RXI,GROUP6), /* IIC0 RXI (Receive data full) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_IIC0_TXI,GROUP7), /* IIC0 TXI (Transmit data empty) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_IIC0_TEI,GROUP0), /* IIC0 TEI (Transmit end) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_IIC0_ERI,GROUP1), /* IIC0 ERI (Transfer error) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_DMAC0_INT,GROUP2), /* DMAC0 INT (DMAC0 transfer end) */
        };
        #endif
        #endif
