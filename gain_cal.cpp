#include "gain_cal.hpp"
#include "vna_measurement.hpp" // VNAMeasurement, SweepArgs のため
#include "globals.hpp"         // vnaMeasurement, current_props, system_millis のため
#include "common.hpp"          // complexf のため
#include "board_v2_plus4/board.hpp" // delay_ms のため (または common.hpp で提供)
#include "spi_config.h"        // DEFAULT_FREQ などのため (spi_config.h に定義を移動した場合)

// spi_config.h に以下の定義がない場合、ここで定義するか、適切な場所に移動
#ifndef DEFAULT_FREQ
#define DEFAULT_FREQ 100000000UL // 100MHz (仮の値)
#endif
#ifndef DEFAULT_NPOINTS
#define DEFAULT_NPOINTS 101
#endif
#ifndef DEFAULT_AVG_N
#define DEFAULT_AVG_N 1
#endif


// グローバルなvnaMeasurementオブジェクトを参照
// extern VNAMeasurement vnaMeasurement; // globals.hppで宣言済み
// extern volatile uint32_t system_millis; // globals.hppで宣言済み

namespace { // 匿名名前空間でコールバック関数を定義

// キャリブレーション用データポイント処理コールバック
static complexf cal_points_s11[SWEEP_POINTS_MAX]; // spi_config.h or project_defines
static complexf cal_points_s21[SWEEP_POINTS_MAX];
static int cal_point_idx = 0;
static bool cal_sweep_done = false;

void calibration_data_point_cb(int point_idx, uint32_t freq_hz, VNAObservation v, bool last_point) {
    (void)freq_hz; // 未使用
    if (point_idx < SWEEP_POINTS_MAX) {
        if (cabsf(v[1].real + v[1].imag * I) > 1e-9f) {
            cal_points_s11[point_idx].real = (v[0].real * v[1].real + v[0].imag * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);
            cal_points_s11[point_idx].imag = (v[0].imag * v[1].real - v[0].real * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);
            cal_points_s21[point_idx].real = (v[2].real * v[1].real + v[2].imag * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);
            cal_points_s21[point_idx].imag = (v[2].imag * v[1].real - v[2].real * v[1].imag) / (v[1].real * v[1].real + v[1].imag * v[1].imag);
        } else {
            cal_points_s11[point_idx] = {0,0};
            cal_points_s21[point_idx] = {0,0};
        }
    }
    if (last_point) {
        cal_sweep_done = true;
    }
}
} // anonymous namespace

void performGainCal(VNAMeasurement& vna, float* gain_array, int num_points) {
    (void)gain_array; // この引数は現在のロジックでは未使用のよう

    // SweepArgs構造体がvna_measurement.hppで定義されていると仮定
    SweepArgs args;
    args.f_start = DEFAULT_FREQ; // spi_config.h またはここで定義
    args.f_stop = DEFAULT_FREQ;  // シングルポイント測定
    args.n_points = DEFAULT_NPOINTS; // spi_config.h またはここで定義
    args.n_avg = DEFAULT_AVG_N;    // spi_config.h またはここで定義
    args.cal_on = false;       // キャリブレーション補正は行わない生データが必要

    vna.setEmitDataPointCallback(calibration_data_point_cb);
    cal_point_idx = 0;
    cal_sweep_done = false;

    vna.setSweep(args, true /* is_cal_sweep */); // is_cal_sweep を true に

    uint32_t start_time = system_millis;
    uint32_t timeout_ms = 5000; // 5秒タイムアウト

    // is_sweeping() と stop_sweep() が VNAMeasurement クラスに存在する必要がある
    while(!cal_sweep_done && (system_millis - start_time < timeout_ms)) {
        // vna.poll(); // VNAMeasurement にポーリング関数があれば呼び出す
        delay_ms(1); // 短い遅延
    }

    if (!cal_sweep_done) { // タイムアウトまたはエラー
        // vna.stop_sweep(); // 存在すれば呼び出す
        // エラー処理
        return;
    }

    // cal_points_s11 と cal_points_s21 にデータが収集された
    // これらを使ってゲインキャリブレーション計算を行う
    // ... (実際のゲイン計算ロジック) ...
    // 例: gain_array[i] = calculate_gain_from(cal_points_s11[i], cal_points_s21[i]);
    for (int i = 0; i < num_points && i < SWEEP_POINTS_MAX; ++i) {
        // ダミーのゲイン計算
        // gain_array[i] = 1.0f;
    }
}
