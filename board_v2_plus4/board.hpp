#ifndef BOARD_HPP
#define BOARD_HPP

// !!!!! 重要 !!!!!
// 1. GD32ファミリー定義:
//    本来はMakefileのコンパイルフラグで定義すべきマクロです。
//    GD32F303CCはHigh-densityデバイスの可能性が高いため、GD32F30X_HD を試します。
//    正しい定義は、お使いのlibopencm3のドキュメントやGD32のシリーズ分類を確認してください。
#ifndef GD32F30X_HD // Makefileで定義されていない場合のフォールバック
#define GD32F30X_HD
#endif
// 他の可能性として: GD32F303ZE (具体的な型番), GD32F30X_CL など。

// 2. Makefileの修正勧告:
//    Makefile内の EXTRA_CFLAGS から -DMCULIB_DEVICE_STM32F103, -DSTM32F103, -DSTM32F1
//    といったSTM32関連の定義を【必ず削除】し、代わりに上記のような適切なGD32ファミリーの
//    定義 (例: -DGD32F30X_HD) を追加してください。これが最も重要な修正です。

// libopencm3のインクルードパスのルートが -Ilibopencm3/include となっていることを前提とする
// ヘッダファイルが libopencm3/gd32/ の直下にあると仮定して修正
#include <libopencm3/gd32/rcc.h>
#include <libopencm3/gd32/gpio.h>
#include <libopencm3/gd32/spi.h>
// #include <libopencm3/gd32/f3/rcc.h>  // 修正前
// #include <libopencm3/gd32/f3/gpio.h> // 修正前
// #include <libopencm3/gd32/f3/spi.h>  // 修正前

#include "common.hpp"
#include "spi_slave.hpp" // SPIスレーブ処理のヘッダをインクルード

// BOARD_REVISION は spi_config.h またはビルドフラグで定義されることを想定

// クロック周波数
#define HSE_VALUE    8000000U
#define SYSCLK_FREQ_120MHz 120000000U

#ifndef F_CPU
#define F_CPU SYSCLK_FREQ_120MHz
#endif

// SPIピン定義
#define SPI1_PORT GPIOA
#define SPI1_CLK_PIN GPIO5
#define SPI1_MISO_PIN GPIO6
#define SPI1_MOSI_PIN GPIO7
#define SPI1_NSS_PIN GPIO4

// その他ボード固有定義
#define BUTTON_PORT GPIOA
#define BUTTON_PIN GPIO0

#define LED_GREEN_PORT GPIOC
#define LED_GREEN_PIN GPIO13

// 関数プロトタイプ
void boardInit();
void led_set(bool on);
bool button_pressed();
void delay_ms(uint32_t ms);

#if BOARD_REVISION >= 2
#define si5351_i2c_dev I2C0
#define SI5351_I2C_ADDRESS (0x60 << 1)
void si5351_init();
#endif

#if BOARD_REVISION >= 4
#define SYS_setHWSweep 100
#define SYS_setHWLO1 101
#define SYS_setHWLO2 102
#define SYS_setHWMode 103
#define SYS_setHWGain 104
#define SYS_HWreadADC 105
#define SYS_HWsetAttn 106
#endif

#endif // BOARD_HPP
