#ifndef SPI_SLAVE_HPP
#define SPI_SLAVE_HPP

#include <libopencm3/cm3/nvic.h>   // NVIC関連
#include <libopencm3/stm32/spi.h>  // STM32F1用 SPIヘッダ (これが唯一利用可能なSPIヘッダ)
#include <libopencm3/stm32/gpio.h> // STM32F1用 GPIOヘッダ (spi_slave.cpp で直接GPIO操作する場合)

#include "common.hpp"     // complexf など
#include "spi_config.h" // SPIコマンドやバッファ定義

#define VNA_SPI_PERIPH SPI1 // libopencm3/stm32/spi.h の SPI1 を使用

// グローバルバッファとフラグのextern宣言
extern VNADataPoint_t spiVnaDataBuffer[MAX_SWEEP_POINTS];
extern volatile uint16_t spiVnaDataBufferCount;
extern volatile bool spiDataReadyFlag;
extern volatile bool spiMeasurementInProgressFlag;

// 関数プロトタイプ
void spi_slave_init(void);
void spi_process_command(uint8_t cmd); // この関数の実装は spi_slave.cpp 内
void spi_prepare_data_for_tx(void);    // この関数の実装は spi_slave.cpp 内
uint8_t spi_get_status(void);          // この関数の実装は spi_slave.cpp 内
void spi_slave_poll(void);             // この関数の実装は spi_slave.cpp 内

void spi_slave_notify_measurement_start(void);
void spi_slave_notify_measurement_complete(void);
void spi_slave_buffer_data_point(uint32_t freq_hz, complexf s11, complexf s21);

#endif // SPI_SLAVE_HPP
