#ifndef SPI_SLAVE_HPP
#define SPI_SLAVE_HPP

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/gd32/f3/spi.h> // Adjust path if different for your libopencm3 setup
#include "common.hpp" // For complexf
#include "spi_config.h" // For definitions

// SPI peripheral to be used (SPI1)
#define VNA_SPI_PERIPH SPI1

// Maximum number of data points to buffer for transmission
// This buffer holds the entire sweep data
extern VNADataPoint_t spiVnaDataBuffer[MAX_SWEEP_POINTS];
extern volatile uint16_t spiVnaDataBufferCount; // Number of valid data points in spiVnaDataBuffer
extern volatile bool spiDataReadyFlag;          // Flag indicating a full sweep data is ready
extern volatile bool spiMeasurementInProgressFlag; // Flag indicating measurement is ongoing

// Initialize SPI1 in slave mode
void spi_slave_init(void);

// Process received SPI command
void spi_process_command(uint8_t cmd);

// Prepare data for SPI transmission (called when CMD_REQUEST_DATA is received)
// This function will load the next chunk of data into the SPI TX FIFO or a temporary TX buffer
// that the ISR will use.
void spi_prepare_data_for_tx(void);

// Get current VNA status for SPI transmission
uint8_t spi_get_status(void);

// Call this from main loop to handle SPI related tasks that are not in ISR
void spi_slave_poll(void);

// Functions to be called from measurement routines
void spi_slave_notify_measurement_start(void);
void spi_slave_notify_measurement_complete(void);
void spi_slave_buffer_data_point(uint32_t freq_hz, complexf s11, complexf s21);


#endif // SPI_SLAVE_HPP
