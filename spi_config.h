#ifndef SPI_CONFIG_H
#define SPI_CONFIG_H

// main.hpp が complexf, freqHz_t などを定義していることを期待
// もし main.hpp にない場合は、ここで適切な型定義 (例: typedef float freqHz_t;) が必要
#include "main.hpp" // Assuming main.hpp is in the root directory with spi_config.h


// Makefile の EXTRA_CFLAGS から SWEEP_POINTS_MAX がグローバルに定義されていない場合
#ifndef SWEEP_POINTS_MAX
#define SWEEP_POINTS_MAX 201 // デフォルト値
#endif

// SPIコマンドコード (マスター → スレーブ)
#define CMD_TRIGGER_SWEEP   0xA0 // 固定パラメータでVNA測定を開始
#define CMD_REQUEST_DATA    0xB0 // 最新の観測データセットを要求
#define CMD_REQUEST_STATUS  0xC0 // (オプション) VNAの状態を要求

// VNAステータスコード (スレーブ → マスター, CMD_REQUEST_STATUS用)
#define STATUS_IDLE         0x01 // VNAはアイドル状態、トリガ待機中
#define STATUS_MEASURING    0x02 // VNAは測定実行中
#define STATUS_DATA_READY   0x03 // 測定完了、データ取得準備完了
#define STATUS_BUSY         0x04 // 一般的なビジー状態 (例: 前のコマンド処理中)
#define STATUS_ERROR        0xFE // エラー発生
#define STATUS_UNKNOWN_CMD  0xFF // 未知のコマンド受信

// 固定VNA測定パラメータ
#define FIXED_START_FREQ    (freqHz_t)50000       // 例: 50kHz
#define FIXED_STOP_FREQ     (freqHz_t)900000000   // 例: 900MHz
#define FIXED_NUM_POINTS    (uint16_t)SWEEP_POINTS_MAX // スイープポイント数
#define FIXED_AVERAGE_N     (uint16_t)1           // 平均化回数
// #define FIXED_GAIN1      0 // 必要に応じて定義
// #define FIXED_GAIN2      0 // 必要に応じて定義

// データ転送関連
#define SPI_BYTES_PER_FLOAT         (4)
#define SPI_NUM_COMPLEX_PARAMS      (2) // S11, S21
#define SPI_FLOATS_PER_COMPLEX      (2) // 実部, 虚部
#define SPI_BYTES_PER_DATA_POINT    (SPI_BYTES_PER_FLOAT + (SPI_NUM_COMPLEX_PARAMS * SPI_FLOATS_PER_COMPLEX * SPI_BYTES_PER_FLOAT)) // 周波数 + S11(re,im) + S21(re,im)

// SPI送受信バッファサイズ
#define SPI_RX_BUFFER_SIZE  16
#define SPI_TX_CHUNK_SIZE   64

// globals.cpp や gain_cal.cpp で参照されていた定数
#ifndef MEASUREMENT_NPERIODS_NORMAL
#define MEASUREMENT_NPERIODS_NORMAL (100)
#endif
#ifndef MEASUREMENT_NPERIODS_CALIBRATING
#define MEASUREMENT_NPERIODS_CALIBRATING (200)
#endif
#ifndef MEASUREMENT_ECAL_INTERVAL
#define MEASUREMENT_ECAL_INTERVAL (1000)
#endif
#ifndef MEASUREMENT_NWAIT_SWITCH
#define MEASUREMENT_NWAIT_SWITCH (10)
#endif
#ifndef DEFAULT_FREQ
#define DEFAULT_FREQ (100000000UL) // 100 MHz
#endif

// RFSW関連の定数 (rfsw.hpp で定義されているべきだが、もし未定義エラーが出るならここで仮定義)
#ifndef RFSW_REFL
#define RFSW_REFL 0 // 仮の値
#endif
#ifndef RFSW_REFL_ON
#define RFSW_REFL_ON 1 // 仮の値
#endif
#ifndef RFSW_RECV
#define RFSW_RECV 1 // 仮の値
#endif
#ifndef RFSW_RECV_REFL
#define RFSW_RECV_REFL 0 // 仮の値
#endif
#ifndef RFSW_ECAL
#define RFSW_ECAL 2 // 仮の値
#endif
#ifndef RFSW_ECAL_OPEN
#define RFSW_ECAL_OPEN 0 // 仮の値
#endif
#ifndef RFSW_BBGAIN
#define RFSW_BBGAIN 3 // 仮の値
#endif
// RFSW_BBGAIN_GAIN はマクロかもしれないので、ここでは定義しない

// Flash layout related constants
// SAVETOTAL_BYTES と CONFIGAREA_BYTES は、実際のメモリマップに合わせて定義する必要があります。
// リンカスクリプトで USERFLASH_END が定義されていることを前提とします。
// extern const uint32_t USERFLASH_END; // リンカスクリプトで定義されるシンボル
// #define SAVETOTAL_BYTES ( (SAVEAREA_MAX) * (SWEEP_POINTS_MAX * sizeof(complexf) * 2 + sizeof(freqHz_t) * SWEEP_POINTS_MAX + sizeof(CalibrationData)) ) // 仮の計算例
// #define CONFIGAREA_BYTES (1024) // 仮

#endif // SPI_CONFIG_H
