#ifndef SPI_SLAVE_HPP
#define SPI_SLAVE_HPP

#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include "spi_config.h" // For buffer sizes and command definitions
#include "fifo.hpp"     // リングバッファ用 (SimpleFIFO を想定)

// SPIスレーブ通信を処理するクラス
class SPISlave {
public:
    SPISlave();

    // SPIスレーブの初期化
    void init();

    // SPI割り込みハンドラ (spi1_isr から呼び出される)
    void handle_interrupt();

    // 受信コマンドFIFOから1バイト読み出し
    // データがない場合は false を返す
    bool read_command(uint8_t& cmd);

    // 送信データバッファにデータを準備 (単一バイト)
    void prepare_tx_byte(uint8_t data);

    // 送信データバッファに複数バイトのデータを準備
    // len バイトのデータをバッファリングする
    // バッファオーバーフローに注意
    void prepare_tx_data(const uint8_t* data, uint16_t len);

    // 現在のVNAステータスを取得 (main2.cpp で管理される状態に基づく)
    // この関数はSPISlaveクラスの外部で実装され、SPISlaveから呼び出されることを想定
    // または、SPISlaveが状態を直接管理するように変更することも可能
    // uint8_t get_vna_status(); // このプロトタイプは例

private:
    // SPIピンの初期化
    void gpio_init();
    // SPIペリフェラルの初期化
    void spi_peripheral_init();
    // SPI割り込みの初期化
    void nvic_init();

    // 受信コマンドを格納するリングバッファ
    SimpleFIFO<uint8_t, SPI_RX_BUFFER_SIZE> rx_fifo;
    // 送信データを格納するリングバッファ
    SimpleFIFO<uint8_t, SPI_TX_CHUNK_SIZE * 2> tx_fifo; // 送信チャンクサイズより大きめに確保

    volatile bool tx_active; // 送信中フラグ
};

// グローバルなSPISlaveインスタンスへのポインタ (ISRからアクセスするため)
// extern SPISlave* g_spi_slave_ptr; // main2.cpp で実体を定義する場合

#endif // SPI_SLAVE_HPP
