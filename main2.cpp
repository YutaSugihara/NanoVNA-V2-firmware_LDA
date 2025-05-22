#include "main.hpp"        // common.hpp や board.hpp をインクルードしている想定
#include "globals.hpp"     // グローバル変数 (current_props, vnaMeasurement など)
#include "common.hpp"
#include "board_v2_plus4/board.hpp" // board.hpp をインクルード
// #include "plot.hpp"        // UI関連、今回は削除
// #include "ui.hpp"          // UI関連、今回は削除
// #include "uihw.hpp"        // UIハードウェア関連、今回は削除
#include "vna_measurement.hpp"
#include "calibration.hpp"
// #include "command_parser.hpp" // USBシリアルコマンドパーサー、今回は削除
#include "synthesizers.hpp"
#include "flash.hpp"

// SPIスレーブ通信関連
#include "spi_config.h"
#include "spi_slave.hpp"

#include <stdio.h>
#include <string.h>
#include <math.h>
// #include <complex.h> // C99 complex は common.hpp の complexf で代替
#include <algorithm> // std::min, std::max

// --- グローバル変数の実体定義 ---
// board.hpp で extern 宣言されたオブジェクトの実体をここで定義する
// (または、それぞれの機能を提供する .cpp ファイルで定義する)
SweepProperties current_props;
VNAMeasurement vnaMeasurement; // デフォルトコンストラクタで初期化

#if BOARD_REVISION >= 4 && defined(HW_SWEEP) // HW_SWEEP はMakefileで定義されているか確認
SweepArgs currSweepArgs;
#endif

// mculib を使用するオブジェクトの初期化
// mculib::Pad は board.hpp で const Pad として定義済み
// RFSW rfsw(PIN_SW_CTL0, PIN_SW_CTL1); // PIN_SW_CTL0, PIN_SW_CTL1 は board.hpp で定義
RFSW rfsw(Pad(PB0), Pad(PB1)); // 仮の初期化、正しいピン定義を使うこと

// SoftSPI のコンストラクタには遅延関数が必要。適切な遅延関数を渡す。
// void delay_us_for_softspi(uint32_t us) { delay_us(us); } // libopencm3のdelay_usを使う場合
// mculib::SoftSPI synthSPI(delay_us_for_softspi);
mculib::SoftSPI synthSPI([](uint32_t us){ delay_ms(us/1000); for(volatile int i=0; i < us%1000 * 72/4; ++i); }); // 簡易的なμs遅延

Si5351 si5351(si5351_i2c_dev); // si5351_i2c_dev は board.hpp で I2C1 に定義されている想定
ADF4350 adf4350(PIN_ADF_CS, PIN_ADF_LD, synthSPI); // synthSPI を参照渡しできるように修正が必要かも

// DMAADC の初期化。DMAチャネルとADCデバイスを指定する必要がある。
// libopencm3のSTM32F1では DMA1_CHANNEL1, ADC1 など。
// mculib::DMAADC dma_adc(DMA1_CHANNEL1, ADC1); // 仮の初期化
mculib::DMAADC dma_adc(DMA1, ADC1, DMA_CHANNEL1); // mculibのAPIに合わせる (要確認)


// --- プロトタイプ宣言 (ローカル関数) ---
static void initialize_system(void);
static void start_fixed_parameter_sweep(void);
static void process_vna_data(int point_idx, uint32_t freq_hz, VNAObservation v, bool last_point);

// --- システム初期化 ---
static void initialize_system(void) {
    boardInit(); // ボード初期化 (クロック、GPIO、SPIスレーブなど)

    // フラッシュから設定読み込み (必要であれば維持)
    // load_settings_from_flash();

    // VNA測定オブジェクト初期化
    vnaMeasurement.init(); // 内部状態の初期化
    vnaMeasurement.setEmitDataPointCallback(process_vna_data);

    // シンセサイザ初期化
    // synthSPI.init(); // SoftSPIのピン設定など
    // si5351.init();   // I2C経由での初期化
    // adf4350.init();  // SPI経由での初期化

    // RFスイッチ初期化
    rfsw.init();

    // ADC初期化 (DMA_ADCオブジェクトの初期化)
    // dma_adc.init(PIN_ADC_CH0, PIN_ADC_CH1, PIN_ADC_CH2); // board.cppのadcInit()から移動
}

// --- 固定パラメータでのスイープ開始 ---
static void start_fixed_parameter_sweep(void) {
    // spiMeasurementInProgressFlag は CMD_TRIGGER_SWEEP 受信時に true になる
    // この関数が呼ばれる時点で spiMeasurementInProgressFlag は true のはず
    // current_props はグローバル変数

    current_props._frequency0 = SPI_SLAVE_START_FREQ;
    current_props._frequency1 = SPI_SLAVE_STOP_FREQ;
    current_props._sweep_points = SPI_SLAVE_NUM_POINTS;
    current_props._avg = SPI_SLAVE_AVERAGE_N;
    // current_props.cal_slot = 0; // 必要に応じて
    // current_props.tdr_enabled = false;

#if BOARD_REVISION >= 4 && defined(HW_SWEEP)
    currSweepArgs.f_start = current_props._frequency0;
    currSweepArgs.f_stop = current_props._frequency1;
    currSweepArgs.n_points = current_props._sweep_points;
    currSweepArgs.n_avg = current_props._avg;
    currSweepArgs.cal_on = false;
    // currSweepArgs.port = 0;
    vnaMeasurement.setSweep(currSweepArgs, false /* is_cal_sweep */);
#else
    vnaMeasurement.setSweep(
        current_props._frequency0,
        current_props._frequency1,
        current_props._sweep_points,
        current_props._avg,
        false /* is_cal_sweep */
    );
#endif
    // spi_slave_notify_measurement_start() は spi_process_command 内で既に呼ばれている
}

// --- VNAデータポイント処理コールバック ---
static void process_vna_data(int point_idx, uint32_t freq_hz, VNAObservation v, bool last_point) {
    if (spiMeasurementInProgressFlag) { // 測定中のみデータをバッファリング
        complexf s11 = {0,0};
        complexf s21 = {0,0};

        float v1_mag_sq = v[1].real * v[1].real + v[1].imag * v[1].imag;
        if (v1_mag_sq > 1e-18f) { // ほぼゼロでないかチェック (分母が小さすぎないか)
            s11.real = (v[0].real * v[1].real + v[0].imag * v[1].imag) / v1_mag_sq;
            s11.imag = (v[0].imag * v[1].real - v[0].real * v[1].imag) / v1_mag_sq;
            s21.real = (v[2].real * v[1].real + v[2].imag * v[1].imag) / v1_mag_sq;
            s21.imag = (v[2].imag * v[1].real - v[2].real * v[1].imag) / v1_mag_sq;
        }
        spi_slave_buffer_data_point(freq_hz, s11, s21);
    }

    if (last_point && spiMeasurementInProgressFlag) {
        spi_slave_notify_measurement_complete(); // 全ポイントのデータ収集完了を通知
    }
}

// --- メイン関数 ---
int main(void) {
    initialize_system();
    led_set(true); // 初期化完了を示す（デバッグ用）
    delay_ms(100);
    led_set(false);

    bool measurement_requested_by_spi_trigger = false;

    while (true) {
        spi_slave_poll(); // SPIコマンド受信処理

        // SPIからの測定トリガを確認し、まだ開始されていなければ開始する
        if (spiMeasurementInProgressFlag && !measurement_requested_by_spi_trigger && !vnaMeasurement.isSweeping()) {
             measurement_requested_by_spi_trigger = true; // 一度だけスイープを開始するためのフラグ
             start_fixed_parameter_sweep();
        }
        
        // VNA測定処理のポーリング (vnaMeasurement.poll() があると仮定)
        // または、スイープ完了はコールバック(process_vna_data の last_point)で検知
        vnaMeasurement.poll(); // スイープの進行などを処理

        if (measurement_requested_by_spi_trigger && !vnaMeasurement.isSweeping()) {
            // スイープが完了した (または開始できなかったが isSweeping が false になった)
            // spiDataReadyFlag は process_vna_data の last_point で true になっているはず
            if (spiDataReadyFlag) {
                // データ準備完了、次のSPIコマンド (データ要求など) を待つ
            }
            measurement_requested_by_spi_trigger = false; // 次のトリガに備える
            // spiMeasurementInProgressFlag は spi_slave_notify_measurement_complete() で false になっている
        }
        delay_ms(1); // CPU負荷軽減
    }
    return 0;
}
