#ifndef SPI_CONFIG_H
#define SPI_CONFIG_H

#include "main.hpp" // complexf, freqHz_t, SWEEP_POINTS_MAX のため
                    // もし main.hpp で定義されていない場合は、ここで直接 SWEEP_POINTS_MAX を定義

// Makefile の EXTRA_CFLAGS から SWEEP_POINTS_MAX がグローバルに定義されていない場合
#ifndef SWEEP_POINTS_MAX
#define SWEEP_POINTS_MAX 201 // ご提示の EXTRA_CFLAGS に基づく
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
// TODO: アプリケーションの要求に応じてこれらの値を定義してください
#define FIXED_START_FREQ    (freqHz_t)50000       // 例: 50kHz
#define FIXED_STOP_FREQ     (freqHz_t)900000000   // 例: 900MHz
#define FIXED_NUM_POINTS    (uint16_t)SWEEP_POINTS_MAX // スイープポイント数
#define FIXED_AVERAGE_N     (uint16_t)1           // 平均化回数
// 必要に応じて固定ゲイン設定などを追加
// #define FIXED_GAIN1      0
// #define FIXED_GAIN2      0


// データ転送関連
// 1データポイントの構成: 周波数(float), S11実部(float), S11虚部(float), S21実部(float), S21虚部(float)
// 各floatは4バイト
#define SPI_BYTES_PER_FLOAT         (4)
#define SPI_NUM_COMPLEX_PARAMS      (2) // S11, S21
#define SPI_FLOATS_PER_COMPLEX      (2) // 実部, 虚部
#define SPI_BYTES_PER_DATA_POINT    (SPI_BYTES_PER_FLOAT + (SPI_NUM_COMPLEX_PARAMS * SPI_FLOATS_PER_COMPLEX * SPI_BYTES_PER_FLOAT)) // 周波数 + S11(re,im) + S21(re,im)

// SPI送受信バッファサイズ
#define SPI_RX_BUFFER_SIZE  16  // 受信コマンドバッファサイズ (例: 16バイト)
#define SPI_TX_CHUNK_SIZE   64  // 一度に送信するデータチャンクサイズ (例: 64バイト、SPI FIFOサイズやパフォーマンスを考慮して調整)
                                // このサイズは SPI_BYTES_PER_DATA_POINT の倍数である必要はありませんが、
                                // データ送信ロジックで適切に処理する必要があります。

// データ送信完了通知 (オプション)
// マスターがデータ受信バイト数をカウントする場合、これは不要かもしれません
// #define SPI_TX_DONE_MARKER    0xFFFFFFFF

#endif // SPI_CONFIG_H
