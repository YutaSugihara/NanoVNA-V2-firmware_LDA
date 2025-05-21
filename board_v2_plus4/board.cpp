#include "board.hpp"
#include <libopencm3/cm3/systick.h>
#include <libopencm3/gd32/f3/rcu.h>  // GD32用 RCU (Reset and Clock Unit)
#include <libopencm3/gd32/f3/gpio.h>
#include <libopencm3/gd32/f3/i2c.h>  // I2C用
#include <libopencm3/gd32/f3/spi.h>  // SPI用
#include "spi_slave.hpp" // SPIスレーブ処理をインクルード

// --- グローバル変数 ---
volatile uint32_t system_millis = 0; // ミリ秒カウンタ

// --- クロック設定 ---
static void rcu_config(void) {
    // GD32F303CC (board_v2_plus4) は最大120MHzで動作
    // 外部クリスタル (HSE_VALUE = 8MHz) を使用
    rcu_predv0_config(RCU_PREDV0_DIV1); // PRE DV0はPLLの入力前
    rcu_pll_config(RCU_PLLSRC_HXTAL, RCU_PLL_MUL15); // 8MHz * 15 = 120MHz (GD32F303の場合、最大値と乗数を確認)
                                                    // GD32F30xでは PLL倍率は最大 x30 (RCU_PLL_MUL30) など、モデルにより異なる
                                                    // ここでは例として RCU_PLL_MUL15 を使用。実際のボードに合わせてください。
                                                    // NanoVNA V2 Plus4のファームウェアでは、実際にはより複雑なクロック設定がされている可能性があります。
                                                    // (例: rcu_system_clock_source_config(RCU_SCS_PLL); rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1); etc.)
                                                    // ここでは簡略化のためPLL設定のみ示します。既存のプロジェクトのクロック設定を参照してください。
    rcu_osci_on(RCU_PLL_CK);
    rcu_osci_stab_wait(RCU_PLL_CK);
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLL);

    // AHB, APB1, APB2 のプリスケーラ設定 (F_CPU に応じて)
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);      // AHB = SYSCLK
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV2);   // APB1 = SYSCLK / 2 (最大60MHz)
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV1);   // APB2 = SYSCLK

    // ペリフェラルクロック有効化
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_AF); // Alternate Function IO clock enable

#if BOARD_REVISION >= 2
    rcu_periph_clock_enable(RCU_I2C0); // Si5351用 I2C0
#endif
    // SPI1のクロックは spi_slave_init() 内で有効化される
}

// --- SysTick設定 ---
static void systick_config(void) {
    // 1msごとに割り込みを発生させる
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB); // AHBクロックを使用 (F_CPU)
    systick_set_reload(F_CPU / 1000 - 1); // 1ms
    systick_interrupt_enable();
    systick_counter_enable();
}

// --- SysTick割り込みハンドラ ---
extern "C" void sys_tick_handler(void) {
    system_millis++;
}

// --- LED制御 ---
void led_set(bool on) {
    if (on) {
        gpio_bit_reset(LED_GREEN_PORT, LED_GREEN_PIN); // LED ON (カソードコモンの場合)
    } else {
        gpio_bit_set(LED_GREEN_PORT, LED_GREEN_PIN);   // LED OFF
    }
}

// --- ボタン状態取得 ---
bool button_pressed() {
    return gpio_input_bit_get(BUTTON_PORT, BUTTON_PIN) == 0; // プルアップされていると仮定
}


// --- I2C 初期化 (Si5351用) ---
#if BOARD_REVISION >= 2
static void i2c_setup(void) {
    // I2C0 ピン設定 (PB6: SCL, PB7: SDA)
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO6 | GPIO7);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO6 | GPIO7); // オープンドレイン
    gpio_set_af(GPIOB, GPIO_AF_1, GPIO6 | GPIO7); // GD32F30x I2C0 AF1

    i2c_reset(si5351_i2c_dev);
    i2c_peripheral_disable(si5351_i2c_dev);
    // I2Cクロック設定 (例: 100kHz)
    // 詳細な設定は元のファームウェアのi2c設定を参照してください。
    // i2c_set_clock_frequency(si5351_i2c_dev, I2C_CR2_FREQ_36MHZ); // APB1クロック周波数
    // i2c_set_ccr(si5351_i2c_dev, 180); // 100kHz for 36MHz APB1
    // i2c_set_trise(si5351_i2c_dev, 37);
    i2c_enable_analog_noise_filter(si5351_i2c_dev);
    i2c_enable_digital_noise_filter(si5351_i2c_dev, 1); // フィルタ値は調整
    i2c_peripheral_enable(si5351_i2c_dev);
}
#endif


// --- ボード初期化 ---
void boardInit() {
    rcu_config();
    systick_config();

    // GPIO初期化
    // LEDピン設定
    gpio_mode_setup(LED_GREEN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_GREEN_PIN);
    gpio_set_output_options(LED_GREEN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, LED_GREEN_PIN);
    led_set(false); // 初期状態はLEDオフ

    // ボタンピン設定
    gpio_mode_setup(BUTTON_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BUTTON_PIN);

    // LCD、タッチパネル関連の初期化を削除
    // lcd_spi_pins(); // SPIピン設定は spi_slave_init() で行うため削除
    // lcd_cs_init();  // CSピンはNSSとして spi_slave_init() で設定
    // lcd_dc_init();
    // lcd_rst_init();
    // lcd_reset();
    // lcd_spi_init(); // SPIマスター初期化は削除
    // lcd_bl_init(100);
    // touch_init();   // タッチパネル初期化は削除

    // SPIスレーブ初期化を呼び出し
    spi_slave_init();

#if BOARD_REVISION >= 2
    i2c_setup();    // Si5351用I2C初期化
    si5351_init();  // Si5351初期化 (この関数は synthesizers.cpp などにあると想定)
#endif

    // USB CDC ACM 初期化 (必要であれば維持、不要なら削除)
    // usb_serial_init(); // この関数がどこで定義されているか確認
                         // もしUSB機能が不要であれば、関連する初期化も削除
}

// LCDおよびタッチパネル関連の関数実装を削除 (board.hpp からプロトタイプも削除済み)
// ... (lcd_spi_transfer_bulk, touch_get_point などの実装があった場所) ...

#if BOARD_REVISION >= 2
void si5351_init() {
    // Si5351の初期化処理 (synthesizers.cpp などにある実際の処理を呼び出すか、ここに実装)
    // この関数は通常、I2C経由でSi5351レジスタを設定します。
    // 例: synthesizers_init(); // もしこのような集約関数があれば
}
#endif
