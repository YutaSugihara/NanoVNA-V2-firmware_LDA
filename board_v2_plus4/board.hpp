#ifndef BOARD_HPP
#define BOARD_HPP

// MakefileでMCUファミリー定義 (-DSTM32F1, -DMCULIB_DEVICE_STM32F1 など) が
// 設定され、libopencm3とmculibがSTM32F1として動作することを期待します。

// --- 標準ライブラリインクルード ---
#include <cstdint> // uint32_t, uint16_t など
#include <array>   // std::array (rfsw.hpp などで使用する場合)

// --- mculibコア機能 (最優先でインクルード) ---
// これが Pad, digitalWrite, pinMode, INPUT, OUTPUT などを提供するはず
#include <mculib/mculib.hpp>     // mculib のメインヘッダ
// #include <mculib/fastwiring.hpp> // mculib.hpp に含まれていない場合や、より基本的なAPIが必要な場合

// --- libopencm3 コアヘッダ (STM32F1として扱う) ---
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

// --- libopencm3 STM32F1 固有ヘッダ ---
// Makefileの DEVICE=stm32f103c8 により、これらのヘッダがSTM32F1用として解決されることを期待
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/i2c.h> // STM32F1にはI2C1, I2C2がある
#include <libopencm3/stm32/spi.h>  // 利用可能な唯一のSPIヘッダ
#include <libopencm3/stm32/usart.h> // 必要に応じて

// --- プロジェクト固有ヘッダ (型の定義を含むものを先に) ---
#include "common.hpp"           // complexf, freqHz_t など
#include "spi_config.h"         // SPIコマンドやバッファサイズ定義

// 以下のヘッダは、中で定義されている型に依存する extern 宣言より前に来る必要がある
#include "calibration.hpp"      // CalibrationData の定義が必要
struct CalibrationData;         // 前方宣言 (calibration.hppで定義されるはず)

#include "vna_measurement.hpp"  // VNAMeasurement, VNAObservation, SweepArgs の定義が必要
class VNAMeasurement;           // 前方宣言
struct VNAObservation;          // 前方宣言
struct SweepArgs;               // 前方宣言 (vna_measurement.hpp または main.hpp で定義)

#include "flash.hpp"            // FlashStore クラス宣言
class FlashStore;               // 前方宣言

#include "rfsw.hpp"             // RFSW 型と rfsw 関数の宣言 (Pad型に依存)
class RFSW;                     // 前方宣言
enum class RFSWState;           // 前方宣言

#include "synthesizers.hpp"     // Si5351, ADF4350 クラス/型宣言
class Si5351;                   // 前方宣言
class ADF4350;                  // 前方宣言

#include "spi_slave.hpp"        // SPIスレーブ関数プロトタイプ (SPIペリフェラル定義に依存)

// mculib ペリフェラルラッパー
#include <mculib/softspi.hpp>   // For mculib::SoftSPI
#include <mculib/dma_adc.hpp>   // For mculib::DMAADC, DMAChannel, ADCDevice

// --- ボードリビジョン ---
#ifndef BOARD_REVISION
#define BOARD_REVISION 4 // 元のプロジェクトに合わせる (V2 Plus4 は >=4)
#endif

// --- クロック周波数 ---
#define HSE_VALUE    (8000000UL)
#define SYSCLK_FREQ_72MHz  (72000000UL) // STM32F103の最大クロック
#ifndef F_CPU
#define F_CPU SYSCLK_FREQ_72MHz // STM32F103として扱うため、一旦72MHzに
#endif

// --- ピン定義 ---
// mculib/fastwiring.hpp が Pad をグローバルに定義していると仮定。
// もし mculib::Pad が正しい場合は、mculib/fastwiring.hpp のインクルードと
// MakefileのMCULIB_DEVICE_STM32F1定義が機能しているか確認。
const Pad PIN_BUTTON = PA0;
const Pad PIN_LED    = PC13;

const Pad PIN_SYNTH_MOSI = PB15;
const Pad PIN_SYNTH_MISO = PB14;
const Pad PIN_SYNTH_SCK  = PB13;

const Pad PIN_ADF_CS     = PB12;
const Pad PIN_ADF_LD     = PA8;

const Pad PIN_SI_SDA     = PB7;
const Pad PIN_SI_SCL     = PB6;

const Pad PIN_ADC_CH0    = PA1;
const Pad PIN_ADC_CH1    = PA2;
const Pad PIN_ADC_CH2    = PA3;

const Pad PIN_SW_CTL0    = PB0;
const Pad PIN_SW_CTL1    = PB1;

const Pad PIN_SLAVE_SPI_NSS  = PA4;
const Pad PIN_SLAVE_SPI_SCK  = PA5;
const Pad PIN_SLAVE_SPI_MISO = PA6;
const Pad PIN_SLAVE_SPI_MOSI = PA7;

// --- グローバルオブジェクトのextern宣言 ---
// これらの型の定義が上記インクルードによって利用可能になっている必要がある
extern RFSW rfsw;
extern mculib::SoftSPI synthSPI; // SoftSPIのコンストラクタは.cppで
extern Si5351 si5351;           // synthesizers.hppで定義されているSi5351型を想定
extern ADF4350 adf4350;         // synthesizers.hppで定義されているADF4350型を想定
extern mculib::DMAADC dma_adc;  // DMAADCのコンストラクタは.cppで

// --- 関数プロトタイプ ---
void boardInit();
void led_set(bool on);
bool button_pressed();
void delay_ms(uint32_t ms);
void adcInit();
void adcRead(uint16_t* sample_buf, int num_samples);
void synthSetFrequency(uint64_t freq);
void synthSetPower(int power);
void synthSetOutput(bool on);

#if BOARD_REVISION >= 4
#define SYS_setHWSweep 100
#define SYS_setHWLO1 101
#define SYS_setHWLO2 102
#define SYS_setHWMode 103
#define SYS_setHWGain 104
#define SYS_HWreadADC 105
#define SYS_HWsetAttn 106
#endif

// Flashメモリレイアウト関連定数
// STM32F103C8 (64KB Flash) を想定。ページサイズは1KB。
// GD32F303CCT6 (256KB Flash, 2KB page) とは異なるため、リンカスクリプトとの整合性に注意。
namespace board {
    constexpr uint32_t FLASH_PAGE_SIZE = 1024; // STM32F103C8 のページサイズ
    // アプリケーション開始アドレス (ブートローダが存在する場合、そのサイズを考慮)
    constexpr uint32_t APP_START_ADDRESS       = 0x08004000; // NanoVNA V2の一般的なブートローダ後のアドレス
    constexpr uint32_t USERFLASH_START_ADDRESS = APP_START_ADDRESS;
    constexpr uint32_t FLASH_BASE_ADDRESS      = 0x08000000;
    // STM32F103C8T6 のFlashサイズ (64KB) を基準とする
    constexpr uint32_t FLASH_SIZE_BYTES        = (64 * 1024);
    constexpr uint32_t USERFLASH_END_ADDRESS   = FLASH_BASE_ADDRESS + FLASH_SIZE_BYTES -1;
}

#endif // BOARD_HPP
