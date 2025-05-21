#ifndef BOARD_HPP
#define BOARD_HPP

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/scb.h>
#include <mculib/fastwiring.hpp>
#include <mculib/dma_adc.hpp>
#include <mculib/si5351.hpp>
#include <mculib/adf4350.hpp>
#include <mculib/softspi.hpp>
// #include "ili9341.hpp" // LCD削除のためコメントアウト
// #include "xpt2046.hpp" // タッチパネル削除のためコメントアウト
#include "../rfsw.hpp" // ルートディレクトリのrfsw.hppをインクルードするように修正
#include "../main.hpp" // ルートディレクトリのmain.hppをインクルードするように修正

// <<< SPI Slave 追加 >>>
#include "../spi_slave.hpp" // ルートディレクトリのspi_slave.hppをインクルードするように修正
// <<< SPI Slave 追加終わり >>>


#define BOARD_REVISION 4 // NanoVNA V2 Plus4 (ビルドログに合わせて4のままにしています)

// Pin definitions
#define PIN_BUTTON PA0
#define PIN_LED PC13

// Synthesizer SPI
#define PIN_SYNTH_MOSI PB15
#define PIN_SYNTH_MISO PB14
#define PIN_SYNTH_SCK PB13

// ADF4350 pins
#define PIN_ADF_CS PB12
#define PIN_ADF_LD PA8

// SI5351 pins
#define PIN_SI_SDA PB7
#define PIN_SI_SCL PB6

// ADC pins
#define PIN_ADC_CH0 PA1   // Reflection ADC
#define PIN_ADC_CH1 PA2   // Transmission ADC
#define PIN_ADC_CH2 PA3   // Reference ADC
// #define PIN_ADC_VREF PA0  // PA0 is PIN_BUTTON now

// RF switch control pins
#define PIN_SW_CTL0 PB0  // S11/S21 switch
#define PIN_SW_CTL1 PB1  // S11/S21 switch

// SPI Slave pins (repurposed from LCD/Touch SPI1)
#define PIN_SLAVE_SPI_SCK  PA5 // 元 lcd_clk
#define PIN_SLAVE_SPI_MISO PA6 // 元 lcd_miso
#define PIN_SLAVE_SPI_MOSI PA7 // 元 lcd_mosi
#define PIN_SLAVE_SPI_NSS  PA4 // 元 lcd_cs (SPI1_NSS) - SPIスレーブのNSSとして使用

// Function prototypes
void boardInitPre(void);
void boardInit(void);
void boardNMI(void);
void boardHardfault(void);
void boardSysTick(void);
void boardSleep(uint32_t msec);
void boardSetLED(bool on);

// RF switch functions
extern RFSW rfsw;
void rfswInit(void);

// Synthesizer functions
extern SoftSPI synthSPI;
extern Si5351 si5351;
extern ADF4350 adf4350;
void synthInit(void);
void synthSetFrequency(uint32_t freq);
uint32_t synthGetFrequency(void);
void synthSetPower(uint8_t power); // 0-3 for ADF4350
void synthSetReference(bool external);
void synthSetOutput(bool on);

// ADC functions
extern DMA_ADC dma_adc;
void adcInit(void);
void adcRead(uint16_t *sample_buf, int num_samples);

// <<< SPI Slave 追加 >>>
extern SPISlave spi_slave; // SPISlaveのインスタンスを宣言
// void spi_slave_init_board(); // SPISlave::init() で十分な場合は不要
// <<< SPI Slave 追加終わり >>>

#endif // BOARD_HPP
