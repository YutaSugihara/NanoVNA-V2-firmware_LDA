#ifndef SPI_CONFIG_H
#define SPI_CONFIG_H

// Define the board revision you are working with, if not already globally defined
// This might influence some low-level configurations.
// For board_v2_plus4, BOARD_REVISION is typically 4 or higher.
// Ensure this aligns with your project's definitions.
#ifndef BOARD_REVISION
#define BOARD_REVISION 4
#endif

// Fixed Measurement Parameters (Adjust these values as needed)
#define SPI_SLAVE_START_FREQ     50000UL    // Example: 50 kHz
#define SPI_SLAVE_STOP_FREQ      900000000UL // Example: 900 MHz
#define SPI_SLAVE_NUM_POINTS     201         // Must be <= SWEEP_POINTS_MAX from your build flags
#define SPI_SLAVE_AVERAGE_N      1           // Example: 1 average

// SPI Commands (Master to Slave)
#define CMD_TRIGGER_SWEEP   0xA0 // Trigger VNA measurement
#define CMD_REQUEST_DATA    0xB0 // Request VNA data
#define CMD_REQUEST_STATUS  0xC0 // Request VNA status (optional)

// VNA Status Codes (Slave to Master, in response to CMD_REQUEST_STATUS)
#define STATUS_IDLE         0x01 // VNA is idle, ready for trigger
#define STATUS_MEASURING    0x02 // VNA is currently measuring
#define STATUS_DATA_READY   0x03 // VNA measurement complete, data is ready
#define STATUS_BUSY         0x04 // VNA is busy with other SPI transaction
#define STATUS_ERROR        0xFE // VNA error

// SPI Data Buffers
// Max points defined by your build command (EXTRA_CFLAGS="-DSWEEP_POINTS_MAX=201")
#define MAX_SWEEP_POINTS SPI_SLAVE_NUM_POINTS

// Data structure for one measurement point
// Frequency (4 bytes) + S11_real (4) + S11_imag (4) + S21_real (4) + S21_imag (4) = 20 bytes
typedef struct {
    float frequency;
    float s11_real;
    float s11_imag;
    float s21_real;
    float s21_imag;
} VNADataPoint_t;

// Size of the SPI command receive buffer
#define SPI_CMD_RX_BUFFER_SIZE 8

// Size of the SPI data transmit buffer (for one chunk of data)
// This should be a multiple of VNADataPoint_t size and manageable for SPI transaction
// For example, if SPI FIFO is small, send a few points at a time.
// Let's define it to hold a certain number of points or a fixed byte size.
// For now, let's make it large enough for a few data points.
// Max data for 201 points is 201 * 20 bytes = 4020 bytes.
// SPI TX buffer for individual transactions. Master will request data multiple times if needed.
#define SPI_DATA_TX_CHUNK_SIZE (10 * sizeof(VNADataPoint_t)) // Send 10 points per chunk (200 bytes)

#endif // SPI_CONFIG_H
