#include <string.h>
#include <stdio.h>
#include <math.h>
#include <complex>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include "main.hpp"
#include "board.hpp"
#include "mculib/fastwiring.hpp"
#include "mculib/dma_adc.hpp"
#include "mculib/si5351.hpp"
#include "mculib/adf4350.hpp"
#include "mculib/printk.hpp"
#include "mculib/softspi.hpp"
#include "mculib/small_function.hpp"
#include "mculib/usbserial.hpp"
#include "mculib/message_log.hpp"

#include "plot.hpp"
#include "ui.hpp"
#include "uihw.hpp"
#include "calibration.hpp"
#include "command_parser.hpp"
#include "vna_measurement.hpp"
#include "globals.hpp"
#include "fft.hpp"
#include "stream_fifo.hpp"

// <<< SPI Slave 追加 >>>
#include "spi_config.h"
#include "spi_slave.hpp" // SPISlaveクラスの定義
// <<< SPI Slave 追加終わり >>>


// --- グローバル変数 ---
SweepProperties current_props;
VNAMeasurement vnaMeasurement;
Plot plot;
UI ui;
CalibrationStore cal_store;
CommandParser cmdParser;
SweepArgs currSweepArgs; // USBコマンド等で使用される可能性のあるスイープ引数

// <<< SPI Slave 追加: グローバル変数 >>>
extern SPISlave spi_slave; // board.cpp で定義されたインスタンスを extern 宣言

// VNAの状態を管理する変数
enum class VNAState {
    IDLE,
    MEASURING,
    DATA_READY,
    BUSY // SPI送信中など
};
volatile VNAState current_vna_state = VNAState::IDLE;

// SPI送信用データバッファ
// 1データポイントあたり: 周波数(float), S11実部(float), S11虚部(float), S21実部(float), S21虚部(float)
float spi_tx_data_buffer[SWEEP_POINTS_MAX * (1 + 2 * 2)]; // 周波数 + S11(re,im) + S21(re,im)
uint16_t spi_tx_data_point_count = 0; // バッファに格納されたデータポイント数
uint16_t spi_tx_data_sent_offset = 0; // 送信済みデータのオフセット（バイト単位またはポイント単位）

bool spi_trigger_received = false;
// <<< SPI Slave 追加終わり >>>


// --- 関数のプロトタイプ宣言 ---
void process_spi_command(uint8_t cmd);
void start_fixed_sweep();
void prepare_spi_tx_data_chunk();
void setVNASweepToUI(void); // 元のmain2.cppから移動 (USBコマンドで使用)

// --- 割り込みサービスルーチン ---
// SPI1の割り込みハンドラ (ベクターテーブルに登録される)
// SPISlaveクラスのhandle_interruptを呼び出す
void spi1_isr(void) {
    spi_slave.handle_interrupt();
}


// --- メイン処理 ---
int main(void) {
    boardInitPre(); // クロック設定など、早期の初期化
    boardInit();    // GPIO, ペリフェラルなどの初期化 (SPIスレーブ初期化もここに含まれる)

    // USBシリアル初期化 (デバッグ用に残すか、最終的に削除するか検討)
#ifdef ENABLE_USB_SERIAL
    usbSerialInit();
    printk("NanoVNA Firmware Init (SPI Slave Mode)\n");
#endif

    // UI関連の初期化はLCD/タッチパネルがないため大幅に簡略化または削除
    // ui.init(&plot, &vnaMeasurement, &cal_store);
    // ui.loadSettings(); // 設定読み込みも不要になる可能性

    // キャリブレーションデータのロード (SPI経由での制御になるため、初期ロードの必要性を検討)
    // cal_store.loadDefault(); // または最後に保存されたデータをロード
    // vnaMeasurement.setCalibration(&cal_store.cal);

    cmdInit(); // USBシリアルコマンドパーサー初期化 (デバッグ用に残す場合)

    // <<< SPI Slave 追加: 初期状態設定 >>>
    current_vna_state = VNAState::IDLE;
    spi_trigger_received = false;
    spi_tx_data_point_count = 0;
    spi_tx_data_sent_offset = 0;
    // <<< SPI Slave 追加終わり >>>


    while (true) {
        // USBシリアルコマンド処理 (デバッグ用)
#ifdef ENABLE_USB_SERIAL
        if (usbSerialAvailable()) {
            char c = usbSerialGetchar();
            cmdReadFIFO(c);
        }
        cmdParserTick();
#endif

        // <<< SPI Slave 追加: コマンド処理 >>>
        uint8_t received_cmd;
        if (spi_slave.read_command(received_cmd)) {
            process_spi_command(received_cmd);
        }
        // <<< SPI Slave 追加終わり >>>


        // VNA測定トリガ処理
        if (spi_trigger_received && current_vna_state == VNAState::IDLE) {
            spi_trigger_received = false;
            start_fixed_sweep();
        }

        // メインループでの他の処理 (必要であれば)
        // 例: LED点滅、低電力モードへの移行など

        // UI処理は削除または大幅簡略化
        // ui.tick();
        // ui.draw();
    }
    return 0; // ここには到達しない
}

// VNA測定データポイント受信コールバック
void measurementEmitDataPoint(int index, freqHz_t freq, const VNAObservation& v, bool last_point) {
    if (current_vna_state != VNAState::MEASURING) {
        return; // 測定中でなければ何もしない
    }

    if (index < 0 || index >= FIXED_NUM_POINTS) { // FIXED_NUM_POINTS は spi_config.h で定義
        // インデックス範囲外 (エラー)
        return;
    }

    // Sパラメータ計算
    complexf s11, s21;
    if (std::abs(v[1]) > 1e-9f) { // ゼロ除算を避ける
        s11 = v[0] / v[1];
        s21 = v[2] / v[1];
    } else {
        s11 = complexf(0,0); // エラーまたは無効値
        s21 = complexf(0,0);
    }

    // SPI送信用バッファにデータを格納
    int base_idx = index * (1 + 2 * 2);
    if (base_idx + 4 < (sizeof(spi_tx_data_buffer) / sizeof(spi_tx_data_buffer[0]))) { // バッファオーバーランチェック
        spi_tx_data_buffer[base_idx + 0] = static_cast<float>(freq);
        spi_tx_data_buffer[base_idx + 1] = s11.real();
        spi_tx_data_buffer[base_idx + 2] = s11.imag();
        spi_tx_data_buffer[base_idx + 3] = s21.real();
        spi_tx_data_buffer[base_idx + 4] = s21.imag();
        spi_tx_data_point_count = index + 1;
    }


    if (last_point) {
        current_vna_state = VNAState::DATA_READY;
        spi_tx_data_sent_offset = 0; // 送信オフセットをリセット
#ifdef ENABLE_USB_SERIAL
        printk("Sweep complete. Data ready for SPI transfer. Points: %d\n", spi_tx_data_point_count);
#endif
    }
}


// UIアクション (SPI制御に置き換えられるため、多くは不要になるか、内部的に使用される)
namespace UIActions {
    // set_sweep_frequency, set_sweep_points などは直接呼び出されなくなるか、
    // USBデバッグコマンド経由でのみ使用される。

    void apply_sweep_settings_to_hw() {
        SweepArgs sweep_args;
        sweep_args.f_start = current_props._frequency0;
        sweep_args.f_stop = current_props._frequency1;
        sweep_args.n_points = current_props._sweep_points;
        sweep_args.gain1 = current_props._gain1;
        sweep_args.gain2 = current_props._gain2;
        sweep_args.cal_on = current_props._cal_on;
        sweep_args.avg_n = current_props._avg;
        // sweep_args.port = current_props._port; // ポート設定も反映する場合

        vnaMeasurement.setSweep(&sweep_args, measurementEmitDataPoint);
    }

    // 元の set_sweep_frequency, set_sweep_points などは、
    // USBコマンドから current_props を変更するために残すことができる。
    // ただし、それらが呼び出された後に apply_sweep_settings_to_hw() が
    // 適切なタイミングで呼び出されるようにする必要がある。
    // SPI制御下では、これらの関数は直接使用されない。

    // 例: (元のコードから抜粋・調整)
    void set_sweep_frequency(int prop, freqHz_t val) {
        if (prop == 0) current_props._frequency0 = val;
        if (prop == 1) current_props._frequency1 = val;
        // apply_sweep_settings_to_hw(); // USBコマンドから呼び出す場合はここで適用するか、別途sweepコマンドで適用
    }

    void set_sweep_points(int val) {
        current_props._sweep_points = val;
        // apply_sweep_settings_to_hw();
    }
     void set_active_trace(int trace) {
        current_props._active_trace = trace;
        // ui.updateTraceInfo(); // UIがない場合は不要
    }
    // 他のUIActions関数も同様に、SPI制御との兼ね合いを考慮して調整または削除
}


// <<< SPI Slave 追加: コマンド処理関数 >>>
void process_spi_command(uint8_t cmd) {
#ifdef ENABLE_USB_SERIAL
    printk("SPI CMD RX: 0x%02X\n", cmd);
#endif
    switch (cmd) {
        case CMD_TRIGGER_SWEEP:
            if (current_vna_state == VNAState::IDLE) {
                spi_trigger_received = true;
                spi_slave.prepare_tx_byte(STATUS_IDLE);
            } else {
                spi_slave.prepare_tx_byte(STATUS_BUSY);
            }
            break;

        case CMD_REQUEST_DATA:
            if (current_vna_state == VNAState::DATA_READY || current_vna_state == VNAState::BUSY) { // BUSYは送信継続中
                if(current_vna_state == VNAState::DATA_READY) { // 初回のリクエスト
                     current_vna_state = VNAState::BUSY;
                     spi_tx_data_sent_offset = 0; // 送信オフセットをリセット
                }
                prepare_spi_tx_data_chunk();
            } else if (current_vna_state == VNAState::MEASURING) {
                spi_slave.prepare_tx_byte(STATUS_MEASURING);
            } else { // IDLE
                spi_slave.prepare_tx_byte(STATUS_IDLE);
            }
            break;

        case CMD_REQUEST_STATUS:
            {
                uint8_t status_to_send = STATUS_IDLE;
                // 状態読み出しと同時にTX FIFOをクリアする (マスターがステータスバイトのみ読むことを期待)
                // while(!spi_slave.tx_fifo_empty()) spi_slave.get_tx_byte(); // SPISlaveクラスに tx_fifo_empty と get_tx_byte を追加する必要あり

                switch(current_vna_state) {
                    case VNAState::IDLE:        status_to_send = STATUS_IDLE; break;
                    case VNAState::MEASURING:   status_to_send = STATUS_MEASURING; break;
                    case VNAState::DATA_READY:  status_to_send = STATUS_DATA_READY; break;
                    case VNAState::BUSY:        status_to_send = STATUS_BUSY; break; // BUSYは送信中も含む
                    default:                    status_to_send = STATUS_UNKNOWN_CMD; break;
                }
                spi_slave.prepare_tx_byte(status_to_send);
            }
            break;

        default:
            spi_slave.prepare_tx_byte(STATUS_UNKNOWN_CMD);
            break;
    }
}

void start_fixed_sweep() {
    if (current_vna_state != VNAState::IDLE) {
        return;
    }

#ifdef ENABLE_USB_SERIAL
    printk("Starting fixed sweep via SPI trigger.\n");
#endif
    current_vna_state = VNAState::MEASURING;
    spi_tx_data_point_count = 0;
    spi_tx_data_sent_offset = 0;

    current_props._frequency0 = FIXED_START_FREQ;
    current_props._frequency1 = FIXED_STOP_FREQ;
    current_props._sweep_points = FIXED_NUM_POINTS;
    current_props._avg = FIXED_AVERAGE_N;
    current_props._cal_on = cal_store.getActiveCalibrationValid();
    // current_props._port = 0; // 必要に応じてポートも固定

    // gain1, gain2 はボード設定や測定対象に応じて固定値を設定
    // current_props._gain1 = FIXED_GAIN1; // spi_config.h で定義
    // current_props._gain2 = FIXED_GAIN2; // spi_config.h で定義

#if BOARD_REVISION >= 4
    currSweepArgs.f_start = current_props._frequency0;
    currSweepArgs.f_stop = current_props._frequency1;
    currSweepArgs.n_points = current_props._sweep_points;
    currSweepArgs.gain1 = current_props._gain1;
    currSweepArgs.gain2 = current_props._gain2;
    currSweepArgs.cal_on = current_props._cal_on;
    currSweepArgs.avg_n = current_props._avg;
    currSweepArgs.port = current_props._port;
    currSweepArgs.callback = measurementEmitDataPoint;
    vnaMeasurement.setSweep(&currSweepArgs, measurementEmitDataPoint);
#else
    UIActions::apply_sweep_settings_to_hw();
#endif
}


void prepare_spi_tx_data_chunk() {
    if (current_vna_state != VNAState::BUSY || spi_tx_data_point_count == 0) {
        if(current_vna_state == VNAState::BUSY && spi_tx_data_point_count == 0) { // データ送信完了後のBUSY状態
             current_vna_state = VNAState::IDLE;
        }
        return;
    }

    uint32_t total_bytes_to_send = (uint32_t)spi_tx_data_point_count * SPI_BYTES_PER_DATA_POINT;
    uint32_t remaining_bytes = total_bytes_to_send - spi_tx_data_sent_offset;
    uint16_t chunk_len = (remaining_bytes > SPI_TX_CHUNK_SIZE) ? SPI_TX_CHUNK_SIZE : (uint16_t)remaining_bytes;

    if (chunk_len > 0) {
        spi_slave.prepare_tx_data(reinterpret_cast<uint8_t*>(spi_tx_data_buffer) + spi_tx_data_sent_offset, chunk_len);
        spi_tx_data_sent_offset += chunk_len;

        if (spi_tx_data_sent_offset >= total_bytes_to_send) {
            // 全データ送信完了
            current_vna_state = VNAState::IDLE;
            spi_tx_data_point_count = 0;
            spi_tx_data_sent_offset = 0;
#ifdef ENABLE_USB_SERIAL
            printk("All SPI data sent. VNA Idle.\n");
#endif
        }
    } else {
        // 送信するデータがない (全データ送信完了済み)
        current_vna_state = VNAState::IDLE;
        spi_tx_data_point_count = 0;
        spi_tx_data_sent_offset = 0;
    }
}
// <<< SPI Slave 追加終わり >>>


// --- 元のmain2.cppからのUSBコマンド関連 ---
// (デバッグ用に残す。SPI制御と競合しないよう注意が必要)

// This function is called by USB command "sweep" or UI
// It sets up the VNA for a sweep based on current_props
void setVNASweepToUI(void) {
    if (current_vna_state != VNAState::IDLE) {
#ifdef ENABLE_USB_SERIAL
        printk("VNA busy, cannot start sweep from UI/USB command.\n");
#endif
        return;
    }
    current_vna_state = VNAState::MEASURING; // USBコマンド経由での測定も状態管理
    spi_tx_data_point_count = 0;
    spi_tx_data_sent_offset = 0;

#ifdef ENABLE_USB_SERIAL
    printk("Starting sweep from UI/USB command.\n");
#endif

#if BOARD_REVISION >= 4
    currSweepArgs.f_start = current_props._frequency0;
    currSweepArgs.f_stop = current_props._frequency1;
    currSweepArgs.n_points = current_props._sweep_points;
    currSweepArgs.gain1 = current_props._gain1;
    currSweepArgs.gain2 = current_props._gain2;
    currSweepArgs.cal_on = current_props._cal_on;
    currSweepArgs.avg_n = current_props._avg;
    currSweepArgs.port = current_props._port;
    currSweepArgs.callback = measurementEmitDataPoint;
    vnaMeasurement.setSweep(&currSweepArgs, measurementEmitDataPoint);
#else
    UIActions::apply_sweep_settings_to_hw();
#endif
}


// --- USB Command handlers ---
static void cmdInfo(int argc, char **argv) {
    (void)argc; (void)argv;
    printk("Board: NanoVNA V2 Plus4 (GD32F303CC)\n");
    printk("Firmware: Custom SPI Slave Version\n");
    // Add more info as needed
}

static void cmdVersion(int argc, char **argv) {
    (void)argc; (void)argv;
    printk("%s\n", FIRMWARE_VERSION);
}

static void cmdFreq(int argc, char **argv) {
    if(argc < 2) {
        printk("%lld\n", (long long)current_props._frequency0);
        return;
    }
    current_props._frequency0 = strtoull(argv[1], NULL, 0);
    setVNASweepToUI();
}

static void cmdSweep(int argc, char **argv) {
    if(argc < 2) {
        printk("Usage: sweep <start_freq> [stop_freq] [num_points]\n");
        return;
    }
    current_props._frequency0 = strtoull(argv[1], NULL, 0);
    if(argc >= 3)
        current_props._frequency1 = strtoull(argv[2], NULL, 0);
    else
        current_props._frequency1 = current_props._frequency0;

    if(argc >= 4)
        current_props._sweep_points = atoi(argv[3]);

    setVNASweepToUI();
}

static void cmdScan(int argc, char **argv) {
    if(argc < 2) {
        printk("Usage: scan <freq_hz> [avg_n]\n");
        return;
    }
    freqHz_t freq = strtoull(argv[1], NULL, 0);
    int avg_n = 1;
    if(argc >= 3) avg_n = atoi(argv[2]);

    if (current_vna_state != VNAState::IDLE) {
        printk("VNA busy, cannot scan.\n");
        return;
    }
    current_vna_state = VNAState::MEASURING;
    spi_tx_data_point_count = 0;
    spi_tx_data_sent_offset = 0;

    SweepArgs args;
    args.f_start = freq;
    args.f_stop = freq;
    args.n_points = 1;
    args.gain1 = current_props._gain1;
    args.gain2 = current_props._gain2;
    args.cal_on = current_props._cal_on;
    args.avg_n = avg_n;
    args.port = current_props._port;
    args.callback = measurementEmitDataPoint; // This will fill spi_tx_data_buffer
    vnaMeasurement.setSweep(&args, measurementEmitDataPoint);

    // Wait for measurement to complete (blocking for USB command)
    uint32_t timeout_ms = 1000 * avg_n; // Generous timeout
    uint32_t start_time = system_millis;
    while(current_vna_state == VNAState::MEASURING && (system_millis - start_time < timeout_ms)) {
        boardSleep(1); // Sleep briefly
    }

    if (current_vna_state == VNAState::DATA_READY && spi_tx_data_point_count == 1) {
        // Data is in spi_tx_data_buffer[0] to spi_tx_data_buffer[4]
        printk("%.0f %f %f %f %f\n",
               spi_tx_data_buffer[0], // freq
               spi_tx_data_buffer[1], // S11 real
               spi_tx_data_buffer[2], // S11 imag
               spi_tx_data_buffer[3], // S21 real
               spi_tx_data_buffer[4]  // S21 imag
        );
    } else {
        printk("Scan failed or timeout.\n");
    }
    current_vna_state = VNAState::IDLE; // Reset state
}


static void cmdData(int argc, char **argv) {
    int trace = 0;
    if (argc > 1) trace = atoi(argv[1]);
    if (trace < 0 || trace >= TRACES) {
        printk("Invalid trace number\n");
        return;
    }

    if (current_vna_state != VNAState::DATA_READY && current_vna_state != VNAState::IDLE) {
         // If measuring or busy with SPI, don't allow USB data command for now
         // or ensure it doesn't interfere.
        printk("VNA not ready or busy with SPI. Current state: %d\n", (int)current_vna_state);
        return;
    }
    if (spi_tx_data_point_count == 0 && current_vna_state == VNAState::IDLE) {
        printk("No data available. Perform a sweep first.\n");
        return;
    }


    // Output data from spi_tx_data_buffer if available from a previous SPI-triggered sweep
    // or from a USB-triggered sweep.
    for (uint16_t i = 0; i < spi_tx_data_point_count; i++) {
        int base_idx = i * (1 + 2 * 2);
        float freq_val = spi_tx_data_buffer[base_idx + 0];
        complexf s11(spi_tx_data_buffer[base_idx + 1], spi_tx_data_buffer[base_idx + 2]);
        complexf s21(spi_tx_data_buffer[base_idx + 3], spi_tx_data_buffer[base_idx + 4]);
        
        // The original cmdData outputted based on vnaMeasurement.data_points[trace][i]
        // We need to adapt this if we want to output specific traces or raw data.
        // For now, let's output S11 if trace is 0, S21 if trace is 1.
        if (trace == 0) { // S11
             printk("%f %f\n", s11.real(), s11.imag());
        } else if (trace == 1) { // S21
             printk("%f %f\n", s21.real(), s21.imag());
        } else {
            // Or provide raw data if trace > 1, etc.
            // This part needs to align with what the host expects from "data" command
            printk("Trace %d not supported for SPI data buffer output.\n", trace);
            break;
        }
    }
}

static void cmdSparams(int argc, char **argv) {
    int port = 0; // Default to S11
    if (argc > 1) port = atoi(argv[1]);

    if (current_vna_state != VNAState::DATA_READY && current_vna_state != VNAState::IDLE) {
        printk("VNA not ready or busy with SPI. Current state: %d\n", (int)current_vna_state);
        return;
    }
     if (spi_tx_data_point_count == 0 && current_vna_state == VNAState::IDLE) {
        printk("No data available. Perform a sweep first.\n");
        return;
    }

    for (uint16_t i = 0; i < spi_tx_data_point_count; i++) {
        int base_idx = i * (1 + 2 * 2);
        float freq_val = spi_tx_data_buffer[base_idx + 0];
        complexf s11(spi_tx_data_buffer[base_idx + 1], spi_tx_data_buffer[base_idx + 2]);
        complexf s21(spi_tx_data_buffer[base_idx + 3], spi_tx_data_buffer[base_idx + 4]);

        if (port == 0) { // S11
            printk("%.0f %f %f\n", freq_val, s11.real(), s11.imag());
        } else if (port == 1) { // S21
            printk("%.0f %f %f\n", freq_val, s21.real(), s21.imag());
        } else {
            printk("Invalid port for sparameters. Use 0 for S11, 1 for S21.\n");
            return;
        }
    }
}


static void cmdCal(int argc, char **argv) {
    (void)argc; (void)argv;
    if (argc == 1) { // "cal" without args: print status
        printk("Calibration status: %s\n", cal_store.getActiveCalibrationValid() ? "ON" : "OFF");
        printk("Available slots: 0-%d\n", SAVEAREA_MAX -1);
        printk("Active slot: %d\n", cal_store.getActiveSlot());
        return;
    }

    if (strcmp(argv[1], "on") == 0) {
        current_props._cal_on = true;
        // ui.updateCalStatus(); // No UI
        printk("Calibration ON\n");
    } else if (strcmp(argv[1], "off") == 0) {
        current_props._cal_on = false;
        // ui.updateCalStatus(); // No UI
        printk("Calibration OFF\n");
    } else if (strcmp(argv[1], "save") == 0) {
        int slot = (argc > 2) ? atoi(argv[2]) : cal_store.getActiveSlot();
        if (cal_store.saveCalibration(slot)) {
            printk("Calibration saved to slot %d\n", slot);
        } else {
            printk("Failed to save calibration to slot %d\n", slot);
        }
    } else if (strcmp(argv[1], "recall") == 0) {
        int slot = (argc > 2) ? atoi(argv[2]) : 0; // Default to slot 0
        if (cal_store.loadCalibration(slot)) {
            vnaMeasurement.setCalibration(&cal_store.cal);
            current_props._cal_on = true; // Automatically turn on cal after recall
            // ui.updateCalStatus(); // No UI
            // ui.updateTraceInfo(); // No UI
            printk("Calibration recalled from slot %d and applied.\n", slot);
        } else {
            printk("Failed to recall calibration from slot %d\n", slot);
        }
    } else if (strcmp(argv[1], "reset") == 0) {
        cal_store.resetCalibration();
        vnaMeasurement.setCalibration(&cal_store.cal); // Apply reset calibration
        current_props._cal_on = false; // Turn off cal after reset
        // ui.updateCalStatus(); // No UI
        printk("Calibration reset.\n");
    } else if (strcmp(argv[1], "thru") == 0 || strcmp(argv[1], "open") == 0 || strcmp(argv[1], "short") == 0 || strcmp(argv[1], "load") == 0) {
         if (current_vna_state != VNAState::IDLE) {
            printk("VNA must be idle to perform calibration step.\n");
            return;
        }
        Calibration::CalType type;
        if(strcmp(argv[1], "thru") == 0) type = Calibration::THRU;
        else if(strcmp(argv[1], "open") == 0) type = Calibration::OPEN;
        else if(strcmp(argv[1], "short") == 0) type = Calibration::SHORT;
        else type = Calibration::LOAD; // LOAD

        printk("Performing %s calibration step...\n", argv[1]);
        current_vna_state = VNAState::MEASURING; // Indicate busy
        cal_store.measureCalibrationStep(type, &vnaMeasurement, measurementEmitDataPoint);
        // measurementEmitDataPoint will eventually set state to DATA_READY
        // For calibration, we might need a different callback or a way to know it's a cal sweep.
        // For now, assume it uses the same mechanism and we wait for DATA_READY.
        uint32_t timeout_ms = 5000; // Timeout for calibration sweep
        uint32_t start_time = system_millis;
        while(current_vna_state == VNAState::MEASURING && (system_millis - start_time < timeout_ms)) {
            boardSleep(1);
        }
        if(current_vna_state == VNAState::DATA_READY) {
             cal_store.applyLastMeasurementToCalibration(type);
             printk("%s calibration step complete.\n", argv[1]);
        } else {
             printk("%s calibration step failed or timed out.\n", argv[1]);
        }
        current_vna_state = VNAState::IDLE; // Reset state
    } else {
        printk("Usage: cal [on|off|save [slot]|recall [slot]|reset|thru|open|short|load]\n");
    }
}

static void cmdSave(int argc, char **argv) {
    int slot = 0;
    if(argc > 1) slot = atoi(argv[1]);
    if (cal_store.saveCalibration(slot)) {
        printk("Calibration saved to slot %d\n", slot);
    } else {
        printk("Failed to save calibration to slot %d\n", slot);
    }
}

static void cmdRecall(int argc, char **argv) {
    int slot = 0;
    if(argc > 1) slot = atoi(argv[1]);
    if (cal_store.loadCalibration(slot)) {
        vnaMeasurement.setCalibration(&cal_store.cal);
        current_props._cal_on = true;
        // ui.updateCalStatus();
        // ui.updateTraceInfo();
        printk("Calibration recalled from slot %d and applied.\n", slot);
    } else {
        printk("Failed to recall calibration from slot %d\n", slot);
    }
}


static void cmdReset(int argc, char **argv) {
    (void)argc; (void)argv;
    // Perform a software reset
    printk("Resetting device...\n");
    boardSleep(100); // Allow printk to flush
    scb_reset_system();
}

static void cmdGain(int argc, char **argv) {
    if (argc < 3) {
        printk("Usage: gain <channel 0/1> <value 0-63>\n");
        printk("Current gain1: %d, gain2: %d\n", current_props._gain1, current_props._gain2);
        return;
    }
    int channel = atoi(argv[1]);
    int value = atoi(argv[2]);

    if (channel == 0) current_props._gain1 = value;
    else if (channel == 1) current_props._gain2 = value;
    else {
        printk("Invalid channel. Use 0 or 1.\n");
        return;
    }
    // Apply settings if VNA is idle. If sweeping, this might take effect on next sweep.
    if (current_vna_state == VNAState::IDLE) {
        // setVNASweepToUI(); // Or a more direct way to update gains if possible
    }
    printk("Gain set. Ch%d = %d\n", channel, value);
}

static void cmdPower(int argc, char **argv) {
    if (argc < 2) {
        printk("ADF4350 power level: %d (0-3)\n", adf4350.getPower()); // Assuming getPower exists
        return;
    }
    int power = atoi(argv[1]);
    synthSetPower(power); // Directly sets power via board.cpp function
    printk("ADF4350 power set to %d\n", power);
}

static void cmdPause(int argc, char **argv) {
    (void)argc; (void)argv;
    vnaMeasurement.pause();
    printk("Sweep paused.\n");
}

static void cmdResume(int argc, char **argv) {
    (void)argc; (void)argv;
    vnaMeasurement.resume();
    printk("Sweep resumed.\n");
}

static void cmdDmesg(int argc, char **argv) {
    (void)argc; (void)argv;
    messageLogDump();
}

static void cmdOffset(int argc, char **argv) {
    if (argc < 2) {
        printk("Current electrical delay offset: %d ps\n", (int)(current_props._edelay_s * 1e12f));
        return;
    }
    current_props._edelay_s = atof(argv[1]) * 1e-12f;
    // ui.updateTraceInfo(); // No UI
    printk("Electrical delay set to %s ps\n", argv[1]);
}

static void cmdPort(int argc, char **argv) {
    if (argc < 2) {
        printk("Current port: %d\n", current_props._port);
        return;
    }
    int port = atoi(argv[1]);
    if (port == 0 || port == 1) {
        current_props._port = port;
        rfsw.setPort(port); // Update RF switch immediately
        // setVNASweepToUI(); // Update sweep if VNA is idle
        printk("Port set to %d\n", port);
    } else {
        printk("Invalid port. Use 0 or 1.\n");
    }
}

static void cmdAverage(int argc, char **argv) {
    if (argc < 2) {
        printk("Current averaging: %d\n", current_props._avg);
        return;
    }
    int avg = atoi(argv[1]);
    if (avg > 0 && avg <= 1024) { // Add a reasonable upper limit for averaging
        current_props._avg = avg;
        // setVNASweepToUI(); // Update sweep if VNA is idle
        printk("Averaging set to %d\n", avg);
    } else {
        printk("Invalid averaging value (1-1024).\n");
    }
}


// Initialize command parser with available commands
void cmdInit(void) {
    cmdParser.add("info", cmdInfo);
    cmdParser.add("version", cmdVersion);
    cmdParser.add("freq", cmdFreq);
    cmdParser.add("sweep", cmdSweep);
    cmdParser.add("scan", cmdScan);
    cmdParser.add("data", cmdData);
    cmdParser.add("sparameters", cmdSparams); // Alias for consistency
    cmdParser.add("cal", cmdCal);
    cmdParser.add("save", cmdSave);
    cmdParser.add("recall", cmdRecall);
    cmdParser.add("reset", cmdReset);
    cmdParser.add("gain", cmdGain);
    cmdParser.add("power", cmdPower);
    cmdParser.add("pause", cmdPause);
    cmdParser.add("resume", cmdResume);
    cmdParser.add("dmesg", cmdDmesg);
    cmdParser.add("offset", cmdOffset);
    cmdParser.add("port", cmdPort);
    cmdParser.add("average", cmdAverage);

    // Add other commands as needed
}

// System call interface (used by some commands, e.g. for DFU mode)
// Keep this if USB DFU functionality is desired.
int sys_syscall(int fn, void *arg1, void *arg2, void *arg3)
{
    (void)arg2; (void)arg3;
    switch(fn)
    {
        case SYS_REBOOT_TO_DFU:
            // Code to trigger DFU bootloader
            // This is highly MCU specific. For GD32F303, it might involve
            // setting a flag in backup RAM and then a software reset.
            // Example for STM32 (needs GD32 equivalent):
            // *(uint32_t*)0x2001FFFC = 0xDEADBEEF; // Magic value in RAM
            // scb_reset_system();
            printk("DFU Reboot requested. (Implementation specific)\n");
            // For now, just reset. Actual DFU entry needs specific implementation.
            scb_reset_system();
            break;

        case SYS_SET_HW_SWEEP:
            if (current_vna_state == VNAState::IDLE) {
                current_vna_state = VNAState::MEASURING;
                spi_tx_data_point_count = 0;
                spi_tx_data_sent_offset = 0;
                vnaMeasurement.setSweep((SweepArgs*)arg1, measurementEmitDataPoint);
            } else {
                return -1; // Busy
            }
            break;
        default:
            return -1; // Unknown syscall
    }
    return 0;
}

