#include "main.hpp"        // main.hpp は common.hpp や board.hpp をインクルードしている想定
#include "globals.hpp"     // グローバル変数 (current_props, vnaMeasurement など)
#include "common.hpp"
#include "board.hpp"
#include "plot.hpp"        // UI関連だが、一部データ構造を使っている可能性あり。不要なら削除。
#include "ui.hpp"          // UI関連。大部分が不要になる。
#include "uihw.hpp"        // UIハードウェア関連。不要になる。
#include "vna_measurement.hpp"
#include "calibration.hpp"
#include "command_parser.hpp" // USBシリアルコマンドパーサー。不要なら削除。
#include "synthesizers.hpp"
#include "flash.hpp"

// SPIスレーブ通信関連
#include "spi_config.h"
#include "spi_slave.hpp"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <complex.h> // C99 complex
#include <algorithm> // std::min, std::max

// --- グローバル変数 (globals.hpp で宣言されているもの) ---
// Properties current_props; // 現在の測定設定 (SPI経由で固定値設定)
// VNAMeasurement vnaMeasurement; // VNA測定オブジェクト
// SweepArgs currSweepArgs; // ハードウェアスイープ用引数 (BOARD_REVISION >= 4)
// ... その他必要なグローバル変数 ...


// --- プロトタイプ宣言 (ローカル関数) ---
static void initialize_system(void);
static void start_fixed_parameter_sweep(void);
static void process_vna_data(int point_idx, uint32_t freq_hz, VNAObservation v, bool last_point);

// --- システム初期化 ---
static void initialize_system(void) {
    boardInit(); // ボード初期化 (クロック、GPIO、SPIスレーブなど)
    
    // UI関連の初期化は削除
    // uihw_init();
    // ui_init();
    // plot_init();

    // フラッシュから設定読み込み (必要であれば維持)
    // load_settings_from_flash(); // 仮の関数名

    // USBシリアルコマンドパーサー初期化 (不要なら削除)
    // cmdInit();

    // VNA測定オブジェクト初期化
    vnaMeasurement.init();
    vnaMeasurement.setEmitDataPointCallback(process_vna_data); // データポイント処理コールバックを設定

    // その他必要な初期化
    // synthesizers_init(); // シンセサイザ初期化 (boardInit内で呼ばれる場合あり)
}


// --- 固定パラメータでのスイープ開始 ---
static void start_fixed_parameter_sweep(void) {
    if (spiMeasurementInProgressFlag) { // spi_slave.cpp で CMD_TRIGGER_SWEEP 受信時に true になる
        // 測定パラメータをSPI経由の固定値に設定
        current_props._frequency0 = SPI_SLAVE_START_FREQ;
        current_props._frequency1 = SPI_SLAVE_STOP_FREQ;
        current_props._sweep_points = SPI_SLAVE_NUM_POINTS;
        current_props._avg = SPI_SLAVE_AVERAGE_N;
        // current_props.cal_slot = 0; // 必要であればキャリブレーションスロットも設定
        // current_props.tdr_enabled = false; // TDRが無効な場合
        // ... その他 current_props の設定 ...

        // VNA測定オブジェクトに設定を適用
#if BOARD_REVISION >= 4 && defined(HW_SWEEP) // HW_SWEEP は通常ビルドフラグで定義
        // ハードウェアスイープの場合
        currSweepArgs.f_start = current_props._frequency0;
        currSweepArgs.f_stop = current_props._frequency1;
        currSweepArgs.n_points = current_props._sweep_points;
        currSweepArgs.n_avg = current_props._avg;
        currSweepArgs.cal_on = false; // SPI経由の測定では通常キャリブレーション補正は別途考慮
        // currSweepArgs.port = 0; // 測定ポート (S11 or S21)に応じて設定が必要な場合
        
        // sys_syscall を使わずに直接 vnaMeasurement を制御する方が良いかもしれない
        // setHWSweep(&currSweepArgs); // これは syscall 経由。
        vnaMeasurement.setSweep(currSweepArgs, false /* is_cal_sweep */);
#else
        // ソフトウェアスイープの場合
        vnaMeasurement.setSweep(
            current_props._frequency0,
            current_props._frequency1,
            current_props._sweep_points,
            current_props._avg,
            false /* is_cal_sweep */
        );
#endif
        // spi_slave_notify_measurement_start() は spi_process_command 内で既に呼ばれている想定
        // なので、ここでは vnaMeasurement.setSweep() を呼び出すだけで良い。
    }
}

// --- VNAデータポイント処理コールバック ---
// vnaMeasurement から各測定ポイントデータが得られるたびに呼び出される
static void process_vna_data(int point_idx, uint32_t freq_hz, VNAObservation v, bool last_point) {
    if (spiMeasurementInProgressFlag) {
        complexf s11 = {0,0};
        complexf s21 = {0,0};

        // v[1] (前方参照) がゼロでないことを確認
        if (cabsf(v[1].real + v[1].imag * I) > 1e-9f) { // ほぼゼロでないかチェック
             // S11 = 反射(v[0]) / 前方参照(v[1])
            s11.real = (v[0].real * v[1].real + v[0].imag * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);
            s11.imag = (v[0].imag * v[1].real - v[0].real * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);

            // S21 = 透過(v[2]) / 前方参照(v[1])
            s21.real = (v[2].real * v[1].real + v[2].imag * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);
            s21.imag = (v[2].imag * v[1].real - v[2].real * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);
        } else {
            // 前方参照がゼロに近い場合、エラーまたは無効データとして扱う
            // s11, s21 は {0,0} のまま
        }

        // 計算されたSパラメータと周波数をSPI送信用バッファに格納
        spi_slave_buffer_data_point(freq_hz, s11, s21);
    }

    if (last_point && spiMeasurementInProgressFlag) {
        spi_slave_notify_measurement_complete(); // 全ポイントのデータ収集完了を通知
    }

    // UI関連の処理は削除
    // plotData(point_idx, freq_hz, v, last_point);
    // ui_event_data(point_idx, freq_hz, v, last_point);
}


// --- メイン関数 ---
int main(void) {
    initialize_system();
    led_set(true); // 初期化完了を示す（デバッグ用）
    delay_ms(100); // 少し待つ
    led_set(false);

    bool measurement_requested_by_spi = false;

    while (true) {
        // SPIコマンド処理 (ポーリング)
        spi_slave_poll(); // spi_slave.cpp 内でコマンド受信・処理を行う

        // SPIからの測定トリガを確認
        // spiMeasurementInProgressFlag は CMD_TRIGGER_SWEEP を受信すると true になる
        // measurement_requested_by_spi は、一度トリガされたら測定開始処理を行うためのフラグ
        if (spiMeasurementInProgressFlag && !measurement_requested_by_spi && !vnaMeasurement.isSweeping()) {
             measurement_requested_by_spi = true;
             start_fixed_parameter_sweep(); // 固定パラメータでスイープ開始
        }
        
        // VNA測定処理のポーリング (vnaMeasurement.poll() がある場合)
        // vnaMeasurement.poll(); // スイープの進行などを処理
        // または、スイープ完了はコールバック(process_vna_data の last_point)で検知

        if (measurement_requested_by_spi && !vnaMeasurement.isSweeping() && spiDataReadyFlag) {
            // 1回のスイープが完了し、データ準備も完了した
            measurement_requested_by_spi = false; 
            // spiMeasurementInProgressFlag は spi_slave_notify_measurement_complete() で false になっている
            // spiDataReadyFlag は true のまま。次の CMD_REQUEST_DATA で送信される。
        }


        // USBシリアルコマンド処理 (不要なら削除)
        // cmdReadFIFO(); // USBからのコマンドを読み込み・処理
        // cmdPoll();     // 定期的なコマンド処理 (例: 'version' コマンドへの応答など)

        // UI関連の処理は削除
        // ui_poll();
        // ui_update();
        // if (menu_redraw_request()) {
        //     menu_redraw();
        // }
        
        // バッテリー管理など、他のバックグラウンドタスク (必要であれば維持)
        // battery_update();

        // 簡易的な遅延やWFI (Wait For Interrupt)
        // __WFI(); // 省電力のために割り込みを待つ
        delay_ms(1); // CPU負荷を少し下げるための短い遅延
    }
    return 0; // ここには到達しない
}

// --- ユーティリティ関数 (delay_msなど) ---
// (common.cpp などに既にあれば不要)
// void delay_ms(uint32_t ms) {
//     uint32_t start = system_millis;
//     while((system_millis - start) < ms);
// }

