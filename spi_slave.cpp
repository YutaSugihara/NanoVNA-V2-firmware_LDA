#include "spi_slave.hpp"
// board.hpp は spi_slave.hpp より後にインクルードされるか、
// spi_slave.hpp が依存する定義 (Padなど) を含まないようにする。
// ここでは board.hpp は不要と仮定し、必要なものは spi_slave.hpp 経由で解決する。
// #include "board_v2_plus4/board.hpp"

// libopencm3 STM32F1 specific headers (spi_slave.hppでインクルード済み)
// <libopencm3/stm32/rcc.h>  // rcc_periph_clock_enable(RCC_SPI1) のため
// <libopencm3/stm32/gpio.h> // gpio_set_mode のため
// <libopencm3/stm32/spi.h>  // spi_reset, spi_init_slave のため
// <libopencm3/cm3/nvic.h>   // nvic_enable_irq のため

#include <cstring> // For memcpy

// グローバルバッファとフラグ (spi_slave.hppでextern宣言されているものの実体)
VNADataPoint_t spiVnaDataBuffer[MAX_SWEEP_POINTS];
volatile uint16_t spiVnaDataBufferCount = 0;
volatile bool spiDataReadyFlag = false;
volatile bool spiMeasurementInProgressFlag = false;

static volatile uint8_t spi_rx_cmd_buffer[SPI_CMD_RX_BUFFER_SIZE];
static volatile uint8_t spi_rx_cmd_buffer_idx = 0;
static uint8_t spi_tx_chunk_buffer[SPI_DATA_TX_CHUNK_SIZE];
static volatile uint16_t spi_tx_chunk_buffer_len = 0;
static volatile uint16_t spi_tx_chunk_buffer_idx = 0;
static uint16_t current_tx_data_point_index = 0;

void spi_slave_init(void) {
    // SPI1 および GPIOA のクロックは board.cpp の rcc_config で有効化されている前提
    // rcc_periph_clock_enable(RCC_SPI1);
    // rcc_periph_clock_enable(RCC_GPIOA);

    // SPI1ピン設定 (STM32F103の場合)
    // PA4 (SPI1_NSS)  : Input floating (ハードウェアNSS)
    // PA5 (SPI1_SCK)  : Input floating
    // PA6 (SPI1_MISO) : Alternate function output push-pull
    // PA7 (SPI1_MOSI) : Input floating
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO4 | GPIO5 | GPIO7);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO6);

    spi_reset(SPI1);

    // SPI1をスレーブモードで初期化 (STM32F1 API)
    spi_init_slave(SPI1,
                   SPI_CR1_BAUDRATE_FPCLK_DIV_256, // スレーブでは無視される
                   SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,  // CPOL=0
                   SPI_CR1_CPHA_CLK_TRANSITION_2,  // CPHA=1 (SPIモード1)
                   SPI_CR1_DFF_8BIT,
                   SPI_CR1_MSBFIRST);

    spi_disable_software_slave_management(SPI1); // ハードウェアNSSを使用
    // SSOEビットはスレーブモードでは0に保つ (デフォルトのはず)

    spi_enable_rx_buffer_not_empty_interrupt(SPI1);
    // TXE割り込みは送信データがあるときに有効化する

    nvic_enable_irq(NVIC_SPI1_IRQ);
    nvic_set_priority(NVIC_SPI1_IRQ, 1); // 割り込み優先度を設定

    spi_enable(SPI1);

    // フラグとバッファの初期化
    spiDataReadyFlag = false;
    spiMeasurementInProgressFlag = false;
    spiVnaDataBufferCount = 0;
    spi_rx_cmd_buffer_idx = 0;
    spi_tx_chunk_buffer_len = 0;
    spi_tx_chunk_buffer_idx = 0;
    current_tx_data_point_index = 0;
}

// SPI1 割り込みハンドラ (名前はスタートアップファイルに合わせる)
extern "C" void spi1_isr(void) {
    // 受信バッファ非空フラグを確認
    if (spi_get_flag(SPI1, SPI_SR_RXNE)) {
        uint8_t received_byte = spi_read(SPI1); // 受信データを読み取り (RXNEフラグクリア)
        if (spi_rx_cmd_buffer_idx < SPI_CMD_RX_BUFFER_SIZE) {
            spi_rx_cmd_buffer[spi_rx_cmd_buffer_idx++] = received_byte;
        }
        // コマンド処理はメインループで行う (spi_slave_poll)
    }

    // 送信バッファ空フラグを確認
    if (spi_get_flag(SPI1, SPI_SR_TXE)) {
        if (spi_tx_chunk_buffer_idx < spi_tx_chunk_buffer_len) {
            spi_write(SPI1, spi_tx_chunk_buffer[spi_tx_chunk_buffer_idx++]);
        } else {
            // 現在のチャンクの送信が完了
            spi_write(SPI1, 0xFF); // ダミーバイトを送信しておく
            spi_disable_tx_buffer_empty_interrupt(SPI1); // これ以上送信するデータがなければTXE割り込みを無効化
        }
    }

    // オーバーランエラーフラグを確認
    if (spi_get_flag(SPI1, SPI_SR_OVR)) {
        (void)SPI_DR(SPI1); // DRを読む (OVRフラグクリアのためにはDRとSRの読み出しが必要な場合がある)
        (void)SPI_SR(SPI1); // SRを読む
        // オーバーランエラー処理 (例: エラーフラグを立てる、状態をリセットするなど)
    }
}

// SPIコマンド処理 (メインループからポーリングで呼び出される)
void spi_process_command(uint8_t cmd) {
    switch (cmd) {
        case CMD_TRIGGER_SWEEP:
            if (!spiMeasurementInProgressFlag) {
                spi_slave_notify_measurement_start(); // 測定開始を通知 (フラグ設定など)
            }
            break;

        case CMD_REQUEST_DATA:
            spi_prepare_data_for_tx(); // 送信データ準備
            if (spi_tx_chunk_buffer_len > 0) { // 送信するデータがある場合
                 spi_enable_tx_buffer_empty_interrupt(SPI1); // TXE割り込みを有効にして送信開始
            }
            break;

        case CMD_REQUEST_STATUS:
            {
                uint8_t status = spi_get_status();
                // ステータスバイトを送信準備
                spi_tx_chunk_buffer[0] = status;
                spi_tx_chunk_buffer_len = 1;
                spi_tx_chunk_buffer_idx = 0;
                spi_enable_tx_buffer_empty_interrupt(SPI1); // TXE割り込みを有効にして送信開始
            }
            break;

        default:
            // 未知のコマンド
            break;
    }
}

// SPI送信用にデータを準備する
void spi_prepare_data_for_tx(void) {
    if (spiDataReadyFlag && (current_tx_data_point_index < spiVnaDataBufferCount)) {
        uint16_t points_in_chunk = SPI_DATA_TX_CHUNK_SIZE / sizeof(VNADataPoint_t);
        uint16_t remaining_points = spiVnaDataBufferCount - current_tx_data_point_index;
        uint16_t points_to_send_this_chunk = (remaining_points < points_in_chunk) ? remaining_points : points_in_chunk;

        if (points_to_send_this_chunk > 0) {
            memcpy(spi_tx_chunk_buffer,
                   &spiVnaDataBuffer[current_tx_data_point_index],
                   points_to_send_this_chunk * sizeof(VNADataPoint_t));
            spi_tx_chunk_buffer_len = points_to_send_this_chunk * sizeof(VNADataPoint_t);
            spi_tx_chunk_buffer_idx = 0; // 送信インデックスをリセット
            current_tx_data_point_index += points_to_send_this_chunk;
        } else {
            spi_tx_chunk_buffer_len = 0; // 送信するデータなし
        }
    } else {
        spi_tx_chunk_buffer_len = 0; // データ準備未完了または全データ送信済み
        if (spiDataReadyFlag && current_tx_data_point_index >= spiVnaDataBufferCount) {
            // 全データ送信完了後の処理 (例: 次のデータ要求に備えてインデックスをリセット)
            // current_tx_data_point_index = 0;
            // spiDataReadyFlag = false; // 必要に応じて
        }
    }
}

// VNAの現在のステータスを取得
uint8_t spi_get_status(void) {
    if (spiMeasurementInProgressFlag) {
        return STATUS_MEASURING;
    }
    if (spiDataReadyFlag) {
        // 送信すべきデータが残っているか確認
        if (current_tx_data_point_index < spiVnaDataBufferCount) {
            return STATUS_DATA_READY; // データ準備完了、送信可能
        } else {
            return STATUS_IDLE; // 全データ送信完了、アイドル状態に戻る
        }
    }
    return STATUS_IDLE;
}

// メインループから呼び出されるSPIポーリング関数
void spi_slave_poll(void) {
    if (spi_rx_cmd_buffer_idx > 0) {
        // 割り込み禁止でコマンドバッファを安全に処理
        __disable_irq();
        uint8_t cmd_to_process = spi_rx_cmd_buffer[0];
        // 簡単な単一バイトコマンドの場合、バッファをクリア
        // リングバッファなどを使用する場合は、より高度な処理が必要
        spi_rx_cmd_buffer_idx = 0;
        __enable_irq();

        spi_process_command(cmd_to_process);
    }
}

// 測定開始を通知する関数
void spi_slave_notify_measurement_start(void) {
    spiMeasurementInProgressFlag = true;
    spiDataReadyFlag = false;
    spiVnaDataBufferCount = 0;
    current_tx_data_point_index = 0; // 新しい測定のために送信インデックスをリセット
    // 必要であれば、送信バッファもクリア
    spi_tx_chunk_buffer_len = 0;
    spi_tx_chunk_buffer_idx = 0;
}

// 測定完了を通知する関数
void spi_slave_notify_measurement_complete(void) {
    spiMeasurementInProgressFlag = false;
    spiDataReadyFlag = true;
    // current_tx_data_point_index は spi_prepare_data_for_tx で管理されるのでここではリセットしない
}

// 測定データポイントをバッファリングする関数
void spi_slave_buffer_data_point(uint32_t freq_hz, complexf s11, complexf s21) {
    if (spiVnaDataBufferCount < MAX_SWEEP_POINTS) {
        spiVnaDataBuffer[spiVnaDataBufferCount].frequency = (float)freq_hz;
        spiVnaDataBuffer[spiVnaDataBufferCount].s11_real = s11.real;
        spiVnaDataBuffer[spiVnaDataBufferCount].s11_imag = s11.imag;
        spiVnaDataBuffer[spiVnaDataBufferCount].s21_real = s21.real;
        spiVnaDataBuffer[spiVnaDataBufferCount].s21_imag = s21.imag;
        spiVnaDataBufferCount++;
    }
}
