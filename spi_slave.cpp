#include "spi_slave.hpp"
#include "board.hpp" // For board specific pin definitions if needed (SPI pins are usually in board.hpp)
#include <libopencm3/gd32/f3/rcc.h> // Adjust for your specific GD32 series if f3 is not correct
#include <libopencm3/gd32/f3/gpio.h>
#include <libopencm3/gd32/f3/spi.h>
#include <libopencm3/cm3/nvic.h>
#include <cstring> // For memcpy

// Global buffers and flags defined in hpp
VNADataPoint_t spiVnaDataBuffer[MAX_SWEEP_POINTS];
volatile uint16_t spiVnaDataBufferCount = 0;
volatile bool spiDataReadyFlag = false;
volatile bool spiMeasurementInProgressFlag = false;

// Internal buffers for SPI communication
static volatile uint8_t spi_rx_cmd_buffer[SPI_CMD_RX_BUFFER_SIZE];
static volatile uint8_t spi_rx_cmd_buffer_idx = 0;

// Transmit buffer for SPI ISR. Data is copied here before master clocks it out.
static uint8_t spi_tx_chunk_buffer[SPI_DATA_TX_CHUNK_SIZE];
static volatile uint16_t spi_tx_chunk_buffer_len = 0;
static volatile uint16_t spi_tx_chunk_buffer_idx = 0;

// Current data point index for multi-chunk transmission
static uint16_t current_tx_data_point_index = 0;


void spi_slave_init(void) {
    // Enable SPI1 clock and GPIOA clock (assuming SPI1 pins are on GPIOA)
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_GPIOA); // Or the specific GPIO port clock for your SPI pins

    // Configure SPI1 pins: SCK, MISO, MOSI, NSS
    // PA5: SPI1_SCK, PA6: SPI1_MISO, PA7: SPI1_MOSI, PA4: SPI1_NSS (lcd_cs_pin)

    // GPIO setup for SPI1
    // NSS (PA4) - Input, floating or pull-up/down depending on master
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO4 | GPIO5 | GPIO6 | GPIO7);
    gpio_set_af(GPIOA, GPIO_AF0, GPIO4 | GPIO5 | GPIO6 | GPIO7); // For GD32F303, SPI1 AF is usually AF0 or AF5. Check datasheet.
                                                                // Assuming AF0 for now, common for SPI1 on PA4-7.
                                                                // For GD32F30x, it's often GPIO_AF_5 for SPI1 on PA4-7. Please double check.
                                                                // Let's assume board.hpp handles this or we use direct libopencm3 calls.

    // From board_v2_plus4.hpp:
    // static const int SPI1_CLK_PIN = GPIO5;  // PA5
    // static const int SPI1_MISO_PIN = GPIO6; // PA6
    // static const int SPI1_MOSI_PIN = GPIO7; // PA7
    // static const int lcd_cs_pin = GPIO4;    // PA4 (Used as NSS)

    // Reset SPI1 peripheral
    spi_reset(VNA_SPI_PERIPH);

    // Initialize SPI1 in slave mode
    spi_set_slave_mode(VNA_SPI_PERIPH);
    spi_set_direction_2_lines_full_duplex(VNA_SPI_PERIPH); // Full duplex
    spi_set_data_size(VNA_SPI_PERIPH, SPI_CR2_DS_8BIT);   // 8-bit data
    
    // SPI Mode 1: CPOL=0, CPHA=1
    spi_set_clock_polarity_0(VNA_SPI_PERIPH); // CPOL = 0
    spi_set_clock_phase_1(VNA_SPI_PERIPH);    // CPHA = 1

    spi_set_nss_hard(VNA_SPI_PERIPH); // Use hardware NSS pin. Slave select is controlled by master.
                                      // If NSS is software, use spi_set_nss_soft and spi_enable_ss_output/spi_disable_ss_output.
                                      // For slave mode with hardware NSS, this is usually correct.

    // Baudrate prescaler is not set by slave, it follows master's clock.
    // spi_set_baudrate_prescaler(VNA_SPI_PERIPH, SPI_CR1_BR_FPCLK_DIV_4); // Not needed for slave

    spi_set_dff_8bit(VNA_SPI_PERIPH); // Data frame format 8-bit
    
    spi_enable_rx_buffer_not_empty_interrupt(VNA_SPI_PERIPH);
    // spi_enable_tx_buffer_empty_interrupt(VNA_SPI_PERIPH); // Enable TXE interrupt when we have data to send

    // Enable SPI1 interrupt in NVIC
    nvic_enable_irq(NVIC_SPI1_IRQ);
    nvic_set_priority(NVIC_SPI1_IRQ, 1); // Set appropriate priority

    // Enable SPI1 peripheral
    spi_enable(VNA_SPI_PERIPH);

    // Initialize flags and buffers
    spiDataReadyFlag = false;
    spiMeasurementInProgressFlag = false;
    spiVnaDataBufferCount = 0;
    spi_rx_cmd_buffer_idx = 0;
    spi_tx_chunk_buffer_len = 0;
    spi_tx_chunk_buffer_idx = 0;
    current_tx_data_point_index = 0;

    // Prepare a default byte to send if master clocks when slave has nothing (e.g. status idle)
    // SPI_DR(VNA_SPI_PERIPH) = STATUS_IDLE; // Or 0xFF
}

// SPI1 Interrupt Service Routine
void spi1_isr(void) {
    // Check for RXNE (Receive Buffer Not Empty)
    if (spi_is_rx_nonempty(VNA_SPI_PERIPH)) {
        uint8_t received_byte = spi_read(VNA_SPI_PERIPH);

        // Store received byte in command buffer (simple 1-byte command for now)
        if (spi_rx_cmd_buffer_idx < SPI_CMD_RX_BUFFER_SIZE) {
            spi_rx_cmd_buffer[spi_rx_cmd_buffer_idx++] = received_byte;
        }
        // For single byte commands, process immediately or signal main loop
        // For this design, we'll let the main loop poll and process
        // spi_process_command(received_byte); // Or queue it
    }

    // Check for TXE (Transmit Buffer Empty) - master is clocking out data
    if (spi_is_tx_empty(VNA_SPI_PERIPH)) {
        if (spi_tx_chunk_buffer_idx < spi_tx_chunk_buffer_len) {
            spi_write(VNA_SPI_PERIPH, spi_tx_chunk_buffer[spi_tx_chunk_buffer_idx++]);
        } else {
            // All data in the current chunk sent. Master might send dummy byte to get this.
            spi_write(VNA_SPI_PERIPH, 0xFF); // Send dummy byte if no more data in chunk
            // Disable TXE interrupt if no more data to send overall, or wait for next CMD_REQUEST_DATA
            // spi_disable_tx_buffer_empty_interrupt(VNA_SPI_PERIPH);
        }
    }

    // Check for OVR (Overrun Error)
    if (spi_get_flag(VNA_SPI_PERIPH, SPI_SR_OVR)) {
        // Clear overrun error: read DR then SR (or specific sequence for GD32)
        (void)SPI_DR(VNA_SPI_PERIPH);
        (void)SPI_SR(VNA_SPI_PERIPH);
        // Handle overrun error (e.g., log, reset state)
    }
}

void spi_process_command(uint8_t cmd) {
    // This function is called from the main loop poll
    switch (cmd) {
        case CMD_TRIGGER_SWEEP:
            if (!spiMeasurementInProgressFlag) {
                spi_slave_notify_measurement_start();
                // The actual sweep start will be handled in main2.cpp's logic
                // triggered by spiMeasurementInProgressFlag.
                // For now, just set the flag. The main loop will see this.
            } else {
                // Optionally, send a busy status if master tries to trigger while measuring
            }
            break;

        case CMD_REQUEST_DATA:
            spi_prepare_data_for_tx();
            break;

        case CMD_REQUEST_STATUS:
            {
                uint8_t status = spi_get_status();
                // Prepare status byte for transmission.
                // This is tricky with slave SPI. Master clocks out data.
                // One way: ISR loads this into SPI_DR if TXE and no other data.
                // Another way: master sends a dummy byte, slave responds with status.
                // For now, assume master sends CMD_REQUEST_STATUS, then clocks out 1 byte for status.
                spi_tx_chunk_buffer[0] = status;
                spi_tx_chunk_buffer_len = 1;
                spi_tx_chunk_buffer_idx = 0;
                // spi_enable_tx_buffer_empty_interrupt(VNA_SPI_PERIPH); // Enable TXE to send it
            }
            break;

        default:
            // Unknown command
            break;
    }
}


void spi_prepare_data_for_tx(void) {
    if (spiDataReadyFlag && spiVnaDataBufferCount > 0) {
        if (current_tx_data_point_index < spiVnaDataBufferCount) {
            uint16_t points_to_send = 0;
            uint16_t remaining_points = spiVnaDataBufferCount - current_tx_data_point_index;
            
            if (remaining_points * sizeof(VNADataPoint_t) > SPI_DATA_TX_CHUNK_SIZE) {
                points_to_send = SPI_DATA_TX_CHUNK_SIZE / sizeof(VNADataPoint_t);
            } else {
                points_to_send = remaining_points;
            }

            if (points_to_send > 0) {
                memcpy(spi_tx_chunk_buffer, 
                       &spiVnaDataBuffer[current_tx_data_point_index], 
                       points_to_send * sizeof(VNADataPoint_t));
                spi_tx_chunk_buffer_len = points_to_send * sizeof(VNADataPoint_t);
                spi_tx_chunk_buffer_idx = 0;
                current_tx_data_point_index += points_to_send;
                // spi_enable_tx_buffer_empty_interrupt(VNA_SPI_PERIPH); // Enable TXE to start sending
            } else { // No more data points to send in this sweep
                spi_tx_chunk_buffer_len = 0;
                spi_tx_chunk_buffer_idx = 0;
                // Optionally send a completion marker or rely on master counting bytes
            }
        } else { // All data for the sweep has been sent, reset for next potential request
            spi_tx_chunk_buffer_len = 0;
            spi_tx_chunk_buffer_idx = 0;
            // current_tx_data_point_index = 0; // Reset for a new full data request cycle
            // spiDataReadyFlag = false; // Or keep it true until a new sweep
        }
    } else {
        // Data not ready or no data
        spi_tx_chunk_buffer_len = 0;
        spi_tx_chunk_buffer_idx = 0;
        // Optionally prepare a "no data" or "not ready" response if master expects something
    }
}


uint8_t spi_get_status(void) {
    if (spiMeasurementInProgressFlag) {
        return STATUS_MEASURING;
    }
    if (spiDataReadyFlag) {
        return STATUS_DATA_READY;
    }
    // Could add STATUS_BUSY if a SPI transaction is active in ISR
    return STATUS_IDLE;
}

void spi_slave_poll(void) {
    // Check if any command byte has been received by ISR
    if (spi_rx_cmd_buffer_idx > 0) {
        // For now, assume single-byte commands.
        // A more robust system might use a ring buffer for commands.
        uint8_t cmd = spi_rx_cmd_buffer[0]; // Get the first command byte
        
        // Critical section if modifying shared command buffer
        __disable_irq();
        // Shift buffer or reset index (simple reset for single byte command)
        spi_rx_cmd_buffer_idx = 0; 
        __enable_irq();

        spi_process_command(cmd);
    }
}

void spi_slave_notify_measurement_start(void) {
    spiMeasurementInProgressFlag = true;
    spiDataReadyFlag = false;
    spiVnaDataBufferCount = 0;
    current_tx_data_point_index = 0; // Reset for new data set
}

void spi_slave_notify_measurement_complete(void) {
    spiMeasurementInProgressFlag = false;
    spiDataReadyFlag = true;
    // spiVnaDataBufferCount should be set by spi_slave_buffer_data_point
}

void spi_slave_buffer_data_point(uint32_t freq_hz, complexf s11, complexf s21) {
    if (spiVnaDataBufferCount < MAX_SWEEP_POINTS) {
        spiVnaDataBuffer[spiVnaDataBufferCount].frequency = (float)freq_hz;
        spiVnaDataBuffer[spiVnaDataBufferCount].s11_real = s11.real;
        spiVnaDataBuffer[spiVnaDataBufferCount].s11_imag = s11.imag;
        spiVnaDataBuffer[spiVnaDataBufferCount].s21_real = s21.real;
        spiVnaDataBuffer[spiVnaDataBufferCount].s21_imag = s21.imag;
        spiVnaDataBufferCount++;
    }
}

