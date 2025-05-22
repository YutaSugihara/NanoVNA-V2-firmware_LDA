#include "board_v2_plus4/board.hpp" // board.hpp を最初にインクルード
// 他の必要な標準ライブラリやプロジェクトヘッダは board.hpp 経由でインクルードされる想定

// --- グローバル変数定義 (board.hpp で extern 宣言されたもの) ---
// これらは mculib や他のライブラリの初期化に依存するため、
// main2.cpp などで適切にインスタンス化・初期化される必要がある。
// ここでは定義せず、他の場所で定義されることを前提とする。
// RFSW rfsw;
// mculib::SoftSPI synthSPI;
// Si5351 si5351;
// ADF4350 adf4350;
// mculib::DMAADC dma_adc;

volatile uint32_t system_millis = 0; // ミリ秒カウンタ (SysTickでインクリメント)

// --- クロック設定 (STM32F103互換モード - 最大72MHzを想定) ---
static void rcc_config(void) {
    // libopencm3の標準的なSTM32F1向けクロック設定関数を使用
    // HSE(8MHz)からPLL経由で72MHzを生成
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    // 必要なペリフェラルクロックを有効化
    rcc_periph_clock_enable(RCC_GPIOA); // GPIOA ポート
    rcc_periph_clock_enable(RCC_GPIOB); // GPIOB ポート
    rcc_periph_clock_enable(RCC_GPIOC); // GPIOC ポート
    rcc_periph_clock_enable(RCC_AFIO);  // 代替機能IOクロック (STM32F1では必須)

#if BOARD_REVISION >= 2
    rcc_periph_clock_enable(RCC_I2C1); // I2C1 を使用 (PB6/PB7)
#endif
    rcc_periph_clock_enable(RCC_SPI1); // SPI1 を使用 (SPIスレーブ用)
    rcc_periph_clock_enable(RCC_ADC1); // ADC1 を使用
    rcc_periph_clock_enable(RCC_DMA1); // DMA1 を使用 (ADC用)
}

// --- SysTick設定 ---
static void systick_config(void) {
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB); // AHBクロックを使用
    // F_CPU は board.hpp で SYSCLK_FREQ_72MHz (72000000UL) に定義されているはず
    systick_set_reload(F_CPU / 1000 - 1); // 1msごとに割り込み
    systick_interrupt_enable();
    systick_counter_enable();
}

// --- SysTick割り込みハンドラ ---
extern "C" void sys_tick_handler(void) {
    system_millis++;
}

// --- LED制御 ---
void led_set(bool on) {
    // PIN_LED は board.hpp で const Pad として定義されている
    // mculib/fastwiring.hpp の digitalWrite を使用
    if (on) {
        digitalWrite(PIN_LED, LOW); // LEDがアクティブローの場合を想定 (NanoVNA V2 Plus4の回路による)
    } else {
        digitalWrite(PIN_LED, HIGH);
    }
}

// --- ボタン状態取得 ---
bool button_pressed() {
    // PIN_BUTTON は board.hpp で const Pad として定義されている
    // mculib/fastwiring.hpp の digitalRead を使用
    return digitalRead(PIN_BUTTON) == LOW; // ボタンがプルアップされていて、押すとLOWになる場合を想定
}

// --- 遅延関数 ---
void delay_ms(uint32_t ms) {
    uint32_t start = system_millis;
    while((system_millis - start) < ms);
}

// --- I2C 初期化 (Si5351用 - STM32F103のI2C1を想定) ---
#if BOARD_REVISION >= 2
static void i2c_setup(void) {
    // I2C1のクロックはrcc_configで有効化済み

    // I2C1 ピン設定 (STM32F103のデフォルト: PB6: SCL, PB7: SDA)
    // AF オープンドレインとして設定
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_OD_AF, GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO6 | GPIO7);

    i2c_peripheral_disable(I2C1); // 設定前に無効化
    i2c_reset(I2C1); // I2Cペリフェラルをリセット

    // APB1クロック周波数 (PCLK1) を設定 (rcc_configで36MHzに設定済みと仮定)
    i2c_set_clock_frequency(I2C1, rcc_apb1_frequency / 1000000); // MHz単位で設定

    // CCR (Clock Control Register) の設定 (100kHz Standard mode)
    // CCR = (APB1 Freq) / (2 * I2C Freq)
    // 例: 36MHz / (2 * 100kHz) = 180
    i2c_set_ccr(I2C1, 180);

    // TRISE (Rise Time Register) の設定
    // TRISE = (Maximum SCL Rise Time / APB1 Clock Period) + 1
    // 例: (1000ns / (1/(36MHz))) + 1 = (1000ns / 27.78ns) + 1 approx 36 + 1 = 37
    i2c_set_trise(I2C1, (rcc_apb1_frequency / 1000000) + 1); // 36MHz APB1の場合、36+1 = 37

    i2c_peripheral_enable(I2C1);
}
#endif

// --- ADC 初期化 ---
void adcInit() {
    // mculib::DMAADC dma_adc オブジェクトの初期化は、
    // その定義場所 (例: main2.cpp やグローバルスコープ) で行う。
    // ここでは、もし直接libopencm3のADCを使う場合のピン設定例を示すが、
    // mculib::DMAADCを使用する方針なので、ピン設定はmculib側で行われるはず。
    // gpio_set_mode(PIN_ADC_CH0.port, GPIO_MODE_INPUT_ANALOG, GPIO_CNF_INPUT_ANALOG, PIN_ADC_CH0.pin);
    // gpio_set_mode(PIN_ADC_CH1.port, GPIO_MODE_INPUT_ANALOG, GPIO_CNF_INPUT_ANALOG, PIN_ADC_CH1.pin);
    // gpio_set_mode(PIN_ADC_CH2.port, GPIO_MODE_INPUT_ANALOG, GPIO_CNF_INPUT_ANALOG, PIN_ADC_CH2.pin);
}

// --- ADC 読み取り ---
void adcRead(uint16_t* sample_buf, int num_samples) {
    // dma_adc.read_samples(sample_buf, num_samples); // mculib を使用
    (void)sample_buf; (void)num_samples; // 未使用警告抑制
}

// --- シンセサイザ関連関数 (実装は synthesizers.cpp と仮定) ---
void synthSetFrequency(uint64_t freq) {
    // adf4350.setFrequency(freq); // または si5351
    (void)freq;
}

void synthSetPower(int power) {
    // adf4350.setPower(power); // または si5351
    (void)power;
}

void synthSetOutput(bool on) {
    // adf4350.setOutputEnable(on);
    // si5351.setOutputEnable(0, on); // チャンネル0の場合
    (void)on;
}

// --- ボード初期化 ---
void boardInit() {
    rcc_config();    // クロック設定 (STM32F1互換モード)
    systick_config(); // SysTick設定

    // GPIOピンの初期化 (mculib を使用)
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH); // LEDオフ (アクティブローと仮定)
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // SPIスレーブ初期化
    spi_slave_init();

#if BOARD_REVISION >= 2
    i2c_setup(); // I2C1 を使用するように変更
    // Si5351オブジェクトの初期化はmain2.cppなどで行う
#endif

    // ADC初期化
    // adcInit(); // dma_adcオブジェクトの初期化はmain2.cppなどで行う
}

#if BOARD_REVISION >= 2
void si5351_init() {
    // この関数は、グローバルな si5351 オブジェクトを初期化する代わりに、
    // main2.cpp などで Si5351 クラスのインスタンスを作成し、その init/begin メソッドを呼ぶ形になる。
    // si5351.init(I2C1); のような形を想定 (mculibのSi5351クラスの仕様による)
}
#endif
