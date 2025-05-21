#include "spi_slave.hpp"
#include "board.hpp" // board.hpp からピン定義を読み込む (例: PIN_SPI1_SCKなど)
                     // board_v2_plus4/board.hpp を想定
#include <libopencm3/stm32/rcc.h> // RCC制御関数 (クロック有効化など)

// SPISlave* g_spi_slave_ptr = nullptr; // main2.cpp で実体を定義し、ここに割り当てる場合

SPISlave::SPISlave() : tx_active(false) {
    // g_spi_slave_ptr = this; // ISRからこのインスタンスにアクセスできるようにする
}

void SPISlave::gpio_init() {
    // SPI1のクロックを有効化 (board.cppのboardInitでも行われるが念のため)
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_GPIOA); // PA4, PA5, PA6, PA7 を使用

    // SPI1ピン設定 (board_v2_plus4 向け)
    // PA4 (SPI1_NSS): ハードウェアNSS入力 (マスターが制御)
    //                 GD32では、スレーブモードでNSSハードウェア管理の場合、入力として設定
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, PIN_SLAVE_SPI_NSS); // PIN_SLAVE_SPI_NSS は board.hppで定義想定 (PA4)

    // PA5 (SPI1_SCK): スレーブモードでは入力
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, PIN_SLAVE_SPI_SCK); // PIN_SLAVE_SPI_SCK は board.hppで定義想定 (PA5)
    gpio_set_af(GPIOA, GPIO_AF0, PIN_SLAVE_SPI_SCK); // GD32F303ではAF0

    // PA6 (SPI1_MISO): スレーブモードではAF出力
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, PIN_SLAVE_SPI_MISO); // PIN_SLAVE_SPI_MISO は board.hppで定義想定 (PA6)
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, PIN_SLAVE_SPI_MISO);
    gpio_set_af(GPIOA, GPIO_AF0, PIN_SLAVE_SPI_MISO);

    // PA7 (SPI1_MOSI): スレーブモードでは入力
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, PIN_SLAVE_SPI_MOSI); // PIN_SLAVE_SPI_MOSI は board.hppで定義想定 (PA7)
    gpio_set_af(GPIOA, GPIO_AF0, PIN_SLAVE_SPI_MOSI);
}

void SPISlave::spi_peripheral_init() {
    spi_reset(SPI1); // SPI1をリセット

    // SPI1 をスレーブモードで初期化
    spi_set_slave_mode(SPI1);

    // SPIモード1 (CPOL=0, CPHA=1)
    spi_set_clock_polarity_0(SPI1); // CPOL = 0
    spi_set_clock_phase_1(SPI1);    // CPHA = 1 (立ち上がりエッジでデータキャプチャ)

    spi_set_data_size(SPI1, SPI_CR2_DS_8BIT); // 8ビットデータフォーマット
    spi_set_first_bit_msb_first(SPI1);      // MSBファースト

    // NSSピン管理: ハードウェアNSSを使用
    // スレーブモードでハードウェアNSSを使用する場合、SSMビットをクリアし、SSOEビットをクリア
    // (SSOE=0: NSSピンは入力として動作し、マスターによって駆動される)
    spi_disable_software_slave_management(SPI1);
    spi_disable_ss_output(SPI1); // NSSピンを入力として動作させる (マスターが制御)
                                 // GD32のドキュメントでは、スレーブモードでNSSをハードウェア入力として使用する場合、
                                 // SSM=0, SSI=0 (spi_disable_software_slave_managementがSSIを内部的に制御)
                                 // NSSPビットはNSSパルスモード用なのでここでは関係なし

    // Baudrate prescaler: スレーブモードではマスターのクロックに従うため通常不要だが、
    // libopencm3の `spi_init_slave` は設定を要求する場合がある。
    // `spi_init_slave` は内部で `spi_set_baudrate_prescaler` を呼ぶため、
    // ここでは直接 `spi_init_slave` を使わず、個別の設定関数を使用。
    // もし `spi_init_slave` を使うなら、ダミーのボーレートを設定する。
    // spi_set_baudrate_prescaler(SPI1, SPI_CR1_BR_FPCLK_DIV_256); // スレーブでは無視されるはず

    spi_enable_rx_buffer_not_empty_interrupt(SPI1); // 受信割り込み有効化
    // spi_enable_tx_buffer_empty_interrupt(SPI1);  // 送信割り込みは必要に応じて有効化 (handle_interrupt内で動的に)

    spi_enable(SPI1); // SPI1を有効化
}

void SPISlave::nvic_init() {
    // SPI1割り込みを有効化
    nvic_enable_irq(NVIC_SPI1_IRQ);
    nvic_set_priority(NVIC_SPI1_IRQ, 1); // 割り込み優先度を設定 (必要に応じて調整)
}

void SPISlave::init() {
    gpio_init();
    spi_peripheral_init();
    nvic_init();
    tx_active = false;
}

void SPISlave::handle_interrupt() {
    // 受信バッファ非エンプティ (RXNE) フラグ確認
    if (spi_is_rx_nonempty(SPI1)) {
        uint8_t received_byte = spi_read(SPI1);
        if (!rx_fifo.isFull()) {
            rx_fifo.put(received_byte);
        } else {
            // RX FIFOオーバーフロー処理 (例: エラーフラグを立てる、ログ記録など)
        }
    }

    // 送信バッファエンプティ (TXE) フラグ確認
    if (spi_is_tx_empty(SPI1)) {
        if (!tx_fifo.isEmpty()) {
            spi_write(SPI1, tx_fifo.get());
            tx_active = true;
            spi_enable_tx_buffer_empty_interrupt(SPI1); // 次のバイト送信のためにTXE割り込みを維持
        } else {
            // 送信すべきデータがない場合
            spi_write(SPI1, 0xFF); // ダミーバイトを送信 (マスターがクロックを供給し続ける場合)
            tx_active = false;
            spi_disable_tx_buffer_empty_interrupt(SPI1); // 送信データがないのでTXE割り込みを無効化
        }
    }

    // エラーフラグ確認 (例: オーバーランエラー)
    if (spi_get_error_flag(SPI1, SPI_SR_OVR)) {
        // SPI_SR_OVRビットは読み取り、その後SPI_DRとSPI_SRを読むことでクリアされる
        (void)SPI_DR(SPI1); // データレジスタを読む
        (void)SPI_SR(SPI1); // ステータスレジスタを読む
        // オーバーランエラー処理
    }
    // 他のエラーフラグ (MODF, CRCERRなど) も必要に応じて確認
}

bool SPISlave::read_command(uint8_t& cmd) {
    if (!rx_fifo.isEmpty()) {
        cmd = rx_fifo.get();
        return true;
    }
    return false;
}

void SPISlave::prepare_tx_byte(uint8_t data) {
    // TODO: 割り込み保護が必要な場合がある
    // __disable_irq();
    if (!tx_fifo.isFull()) {
        tx_fifo.put(data);
    }
    // __enable_irq();

    // 送信がアクティブでなく、TX FIFOにデータがある場合、送信開始を試みる
    // (マスターからのクロック供給とNSSデアサート->アサートが必要)
    if (!tx_active && !tx_fifo.isEmpty()) {
         // 最初のバイトを送信するためにTXE割り込みを有効にする
         // マスターがクロックを提供すると、TXE割り込みが発生して送信が開始される
        spi_enable_tx_buffer_empty_interrupt(SPI1);
    }
}

void SPISlave::prepare_tx_data(const uint8_t* data, uint16_t len) {
    // TODO: 割り込み保護が必要な場合がある
    // __disable_irq();
    for (uint16_t i = 0; i < len; ++i) {
        if (!tx_fifo.isFull()) {
            tx_fifo.put(data[i]);
        } else {
            // TX FIFOオーバーフロー処理
            break;
        }
    }
    // __enable_irq();

    if (!tx_active && !tx_fifo.isEmpty()) {
        spi_enable_tx_buffer_empty_interrupt(SPI1);
    }
}

// SPI1の割り込みハンドラ (ベクターテーブルに登録される関数)
// この関数はグローバルスコープに配置するか、クラスの静的メンバとして宣言し、
// ポインタ経由でクラスインスタンスのメソッドを呼び出す必要がある。
// 今回は main2.cpp に SPISlave のインスタンスを作成し、
// そのインスタンスの handle_interrupt() を呼び出すようにする。
// extern SPISlave spi_slave_instance; // main2.cpp で定義されるインスタンス
// void spi1_isr(void) {
//     spi_slave_instance.handle_interrupt();
// }
// または、g_spi_slave_ptr を使用する
// void spi1_isr(void) {
//    if (g_spi_slave_ptr) {
//        g_spi_slave_ptr->handle_interrupt();
//    }
// }
// → main2.cpp で SPISlave のグローバルインスタンスを作成し、
//    そのインスタンスのメソッドを直接呼び出すように ISR を main2.cpp に配置する方が良い。
