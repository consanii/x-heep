/**
 * @file main.c
 * @brief BSP profiling routine
 *
 * This file execute repeated write and read-back operations on the flash memory.
 * The operations are performed using the standard and quad speed modes, and using
 * the DMA controller or not (based on flag). All the operations are checked for
 * correctness. The profiling is performed using RV_TIMER_AO_START_ADDRESS timer.
 *
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "core_v_mini_mcu.h"
#include "csr.h"
#include "hart.h"
#include "handler.h"
#include "soc_ctrl.h"
#include "spi_host.h"
#include "fast_intr_ctrl.h"
#include "fast_intr_ctrl_regs.h"
#include "x-heep.h"
#include "w25q128jw.h"
#include "data_array.bin"
#include "rv_timer.h"
#include "rv_plic.h"
#include "rv_plic_regs.h"

#include "power_manager.h"
#include "rv_plic.h"
#include "rv_plic_regs.h"
#include "rv_timer_regs.h"

// If set, the profiling will be performed using the DMA controller
#define DMA_MODE 0

// Flash address to write to (different from the address where the buffer is stored)
#define FLASH_ADDR 0x00008500

// Len in bytes of the test buffer, 1kB
#define MAX_TEST_BUF_LEN 1024 // From 1 to 1024

// End buffer
uint32_t flash_data_32[MAX_TEST_BUF_LEN];

// Test buffer
// The test buffer is contained in data_array.bin and is 1kB long
// const uint32_t flash_original_32[] = {...}

// Private functions
static mmio_region_t init_timer(void);
static void reset_timer(uint32_t hart_id);

// Global variables
int hart_id = 0;
rv_timer_t timer_0_1;
mmio_region_t timer_0_1_reg;


int main(int argc, char *argv[]) {
    printf("BSP profiling standard functions\n\r");
    error_codes_t status;
    uint32_t errors = 0;
    uint32_t flag = 0;
    uint64_t timer_value = 0;

    // Init SPI host and SPI<->Flash bridge parameters 
    status = w25q128jw_init();
    if (status != FLASH_OK) return EXIT_FAILURE;

    // Init timer
    timer_0_1_reg = init_timer();

    // Not required anymore, keeping it for compatibility
    uint8_t *test_buffer = flash_original_32;
    uint8_t *flash_data = flash_data_32;

    uint32_t *test_buffer_check = flash_original_32;
    uint32_t *flash_data_check = flash_data_32;

    #if DMA_MODE
    printf("Start profile routine DMA MODE - standard speed...\n\r");
    #else
    printf("Start profile routine NORMAL MODE - standard speed...\n\r");
    #endif
    for (int i = 1; i <= MAX_TEST_BUF_LEN; i++) {
        // Reset timer
        reset_timer(hart_id);

        // Start timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerEnabled);

        // WRITE TO FLASH memory at specific address
        #if DMA_MODE
        status = w25q128jw_write_standard_dma(FLASH_ADDR, test_buffer, i);
        #else
        status = w25q128jw_write_standard(FLASH_ADDR, test_buffer, i);
        #endif

        // Stop timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerDisabled);

        // Check for errors during write
        if (status != FLASH_OK) return EXIT_FAILURE;

        // Read and print timer value
        rv_timer_counter_read(&timer_0_1, hart_id, &timer_value);
        printf("W%u, ", timer_value);

        // -------------------------------

        // Reset timer
        reset_timer(hart_id);

        // Start timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerEnabled);

        // READ FROM FLASH memory at the same address
        #if DMA_MODE
        status = w25q128jw_read_standard_dma(FLASH_ADDR, flash_data, i);
        #else
        status = w25q128jw_read_standard(FLASH_ADDR, flash_data, i);
        #endif
        
        // Stop timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerDisabled);

        // Check for errors during read
        if (status != FLASH_OK) return EXIT_FAILURE;

        // Read and print timer value
        rv_timer_counter_read(&timer_0_1, hart_id, &timer_value);
        printf("R%u, ", timer_value);

        // -------------------------------

        // Check if what we read is correct (i.e. flash_original == flash_data)
        uint32_t errors = 0;
        for (int j=0; j < (i%4==0 ? i/4 : i/4+1); j++) {
            if (j < i/4 ) {
                if(flash_data_check[j] != test_buffer_check[j]) {
                    printf("iteration %u - index@%u : %x != %x(ref)\n\r", i, j, flash_data_check[j], test_buffer_check[j]);
                    errors++;
                }
            } else {
                uint32_t last_bytes = 0;
                memcpy(&last_bytes, &test_buffer_check[j], i % 4);
                if (flash_data_check[j] != last_bytes) {
                    printf("iteration %u - index@%u : %x != %x(ref)\n\r", i, j, flash_data_check[j], last_bytes);
                    errors++;
                }
            }
        }
        if (errors > 0) flag = 1;
        errors = 0;
    }


    #if DMA_MODE
    printf("\nStart profile routine DMA MODE - quad speed...\n\r");
    #else
    printf("\nStart profile routine NORMAL MODE - quad speed...\n\r");
    #endif
    for (int i = 1; i <= MAX_TEST_BUF_LEN; i++) {
        // Reset timer
        reset_timer(hart_id);

        // Start timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerEnabled);

        // WRITE TO FLASH memory at specific address
        #if DMA_MODE
        status = w25q128jw_write_quad_dma(FLASH_ADDR, test_buffer, i);
        #else
        status = w25q128jw_write_quad(FLASH_ADDR, test_buffer, i);
        #endif

        // Stop timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerDisabled);

        // Check for errors during write
        if (status != FLASH_OK) return EXIT_FAILURE;

        // Read and print timer value
        rv_timer_counter_read(&timer_0_1, hart_id, &timer_value);
        printf("W%u, ", timer_value);

        // -------------------------------

        // Reset timer
        reset_timer(hart_id);

        // Start timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerEnabled);

        // READ FROM FLASH memory at the same address
        #if DMA_MODE
        status = w25q128jw_read_quad_dma(FLASH_ADDR, flash_data, i);
        #else
        status = w25q128jw_read_quad(FLASH_ADDR, flash_data, i);
        #endif
        
        // Stop timer
        rv_timer_counter_set_enabled(&timer_0_1, hart_id, kRvTimerDisabled);

        // Check for errors during read
        if (status != FLASH_OK) return EXIT_FAILURE;

        // Read and print timer value
        rv_timer_counter_read(&timer_0_1, hart_id, &timer_value);
        printf("R%u, ", timer_value);

        // -------------------------------

        // Check if what we read is correct (i.e. flash_original == flash_data)
        uint32_t errors = 0;
        for (int j=0; j < (i%4==0 ? i/4 : i/4+1); j++) {
            if (j < i/4 ) {
                if(flash_data_check[j] != test_buffer_check[j]) {
                    printf("iteration %u - index@%u : %x != %x(ref)\n\r", i, j, flash_data_check[j], test_buffer_check[j]);
                    errors++;
                }
            } else {
                uint32_t last_bytes = 0;
                memcpy(&last_bytes, &test_buffer_check[j], i % 4);
                if (flash_data_check[j] != last_bytes) {
                    printf("iteration %u - index@%u : %x != %x(ref)\n\r", i, j, flash_data_check[j], last_bytes);
                    errors++;
                }
            }
        }
        if (errors > 0) flag = 1;
        errors = 0;
    }

    // Exit status based on errors found
    if (errors == 0) {
        printf("\nsuccess!\n\r");
        return EXIT_SUCCESS;
    } else {
        printf("\nfailure, %d errors!\n\r", errors);
        return EXIT_FAILURE;
    }
}




// -----------------
// Private functions
// -----------------

static mmio_region_t init_timer(void) {
    // Get current Frequency
    soc_ctrl_t soc_ctrl;
    soc_ctrl.base_addr = mmio_region_from_addr((uintptr_t)SOC_CTRL_START_ADDRESS);
    uint32_t freq_hz = soc_ctrl_get_frequency(&soc_ctrl);
    printf("Freq: %u\n", freq_hz);

    // Initialize timer
    mmio_region_t timer_0_1_reg = mmio_region_from_addr(RV_TIMER_AO_START_ADDRESS);
    rv_timer_init(timer_0_1_reg, (rv_timer_config_t){.hart_count = 2, .comparator_count = 1}, &timer_0_1);
    /* defining timer prescale and step based on its desired freq */
    uint64_t kTickFreqHz = 1000 * 1000; // 1 MHz
    rv_timer_tick_params_t tick_params;
    rv_timer_approximate_tick_params(freq_hz, kTickFreqHz, &tick_params);
    if (tick_params.prescale==0){
        printf("Timer approximate function was not able to set a correct value prescale\n");
    }

    // Return timer memory region
    return timer_0_1_reg;
}

static void reset_timer(uint32_t hart_id) {
    mmio_region_write32(
        timer_0_1_reg,
        reg_for_hart(hart_id, RV_TIMER_TIMER_V_LOWER0_REG_OFFSET), 0x0
    );
    mmio_region_write32(
        timer_0_1_reg,
        reg_for_hart(hart_id, RV_TIMER_TIMER_V_UPPER0_REG_OFFSET), 0x0
    );
}
