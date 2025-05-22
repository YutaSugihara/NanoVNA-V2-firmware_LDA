#include "flash.hpp"
#include <libopencm3/stm32/flash.h> // STM32F1用 Flash関数/レジスタ
#include <cstring>                  // For memcpy, memset

// board.hpp から board::USERFLASH_START_ADDRESS などが参照可能になっている前提

FlashStore::FlashStore() {
    // コンストラクタ
}

bool FlashStore::unlock() {
    flash_unlock(); // libopencm3 STM32F1 API
    return (FLASH_CR & FLASH_CR_LOCKBIT) == 0; // STM32F1 レジスタビット (FLASH_CR_LOCKではない可能性)
                                             // libopencm3/stm32/f1/flash.h を確認 -> FLASH_CR_LOCKBIT が正しい
}

void FlashStore::lock() {
    flash_lock(); // libopencm3 STM32F1 API
}

bool FlashStore::erasePage(uint32_t address) {
    // アドレス範囲チェック (board.hpp の board 名前空間の定数を使用)
    if (address < board::USERFLASH_START_ADDRESS || address >= board::USERFLASH_END_ADDRESS) {
        return false;
    }
    // ページアライメントチェックも必要に応じて行う
    // if ((address % board::FLASH_PAGE_SIZE) != 0) return false;

    // flash_unlock(); // erase/program の前にアンロックが必要な場合がある (FlashStore::unlock() で行う)
    flash_erase_page(address); // libopencm3 STM32F1 API

    // エラーチェック (libopencm3のflash_get_status_flags()などを使用)
    // uint32_t status = flash_get_status_flags();
    // return (status & (FLASH_SR_BSY | FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) == 0;
    // ここでは簡略化のため、成功したと仮定
    return true;
}

bool FlashStore::programHalfWord(uint32_t address, uint16_t data) {
    if (address < board::USERFLASH_START_ADDRESS || (address + sizeof(uint16_t) -1) >= board::USERFLASH_END_ADDRESS) {
        return false;
    }
    // flash_unlock();
    flash_program_half_word(address, data); // libopencm3 STM32F1 API
    // return flash_get_status_flags() == FLASH_SR_EOP && (*(volatile uint16_t*)address == data);
    return (*(volatile uint16_t*)address == data); // 書き込み後ベリファイ (仮)
}

bool FlashStore::programWord(uint32_t address, uint32_t data) {
    if (address < board::USERFLASH_START_ADDRESS || (address + sizeof(uint32_t) -1) >= board::USERFLASH_END_ADDRESS) {
        return false;
    }
    // flash_unlock();
    flash_program_word(address, data); // libopencm3 STM32F1 API
    // return flash_get_status_flags() == FLASH_SR_EOP && (*(volatile uint32_t*)address == data);
    return (*(volatile uint32_t*)address == data); // 書き込み後ベリファイ (仮)
}

bool FlashStore::programBlock(uint32_t address, const uint8_t *data, size_t len) {
    if (address < board::USERFLASH_START_ADDRESS || (address + len) > board::USERFLASH_END_ADDRESS) {
        return false;
    }
    // flash_unlock();
    for (size_t i = 0; i < len; i += 2) { // 16ビット(half-word)単位で書き込む
        if ((address + i + 1) < board::USERFLASH_END_ADDRESS) { // 境界チェック
            uint16_t half_word_data = data[i];
            if ((i + 1) < len) {
                half_word_data |= ((uint16_t)data[i+1] << 8);
            } else {
                half_word_data |= 0xFF00; // 奇数長の場合のパディング (またはエラー処理)
            }
            flash_program_half_word(address + i, half_word_data);
            // ここでもエラーチェックとベリファイが望ましい
            if (*(volatile uint16_t*)(address + i) != half_word_data) return false;
        } else if ((address + i) < board::USERFLASH_END_ADDRESS) { // 残り1バイトの場合
             // 1バイトのみの書き込みは通常サポートされないか、特殊な処理が必要
             // ここではエラーとするか、パディングしてhalf-wordにする
             return false; // または適切な処理
        }
    }
    // flash_lock();
    return true;
}

void FlashStore::readBlock(uint32_t address, uint8_t *data, size_t len) {
    if (address < board::USERFLASH_START_ADDRESS || (address + len) > board::USERFLASH_END_ADDRESS) {
        memset(data, 0xFF, len); // 範囲外ならデフォルト値 (またはエラー処理)
        return;
    }
    memcpy(data, (const void*)address, len);
}

uint32_t FlashStore::get_savearea_addr(int slot) {
    if (slot < 0 || slot >= SAVEAREA_MAX) return 0; // 無効なスロット
    return board::USERFLASH_START_ADDRESS + (slot * SINGLE_SAVE_AREA_SIZE);
}

uint32_t FlashStore::get_configarea_addr() {
    return board::USERFLASH_START_ADDRESS + SAVETOTAL_BYTES; // セーブエリアの直後
}

bool FlashStore::erase_savearea(int slot) {
    uint32_t addr = get_savearea_addr(slot);
    if (addr == 0) return false;

    bool success = true;
    // flash_unlock(); // erasePage内で行われるか、ここでまとめて行う
    for (uint32_t offset = 0; offset < SINGLE_SAVE_AREA_SIZE; offset += board::FLASH_PAGE_SIZE) {
        uint32_t current_page_addr = addr + offset;
        if (current_page_addr < board::USERFLASH_END_ADDRESS) {
            if (!erasePage(current_page_addr)) {
                success = false;
                break;
            }
        } else {
            break; // 領域外
        }
    }
    // flash_lock();
    return success;
}

bool FlashStore::erase_configarea() {
    uint32_t addr = get_configarea_addr();
    if (addr >= board::USERFLASH_END_ADDRESS) return false;

    bool success = true;
    // flash_unlock();
    for (uint32_t offset = 0; offset < CONFIGAREA_BYTES; offset += board::FLASH_PAGE_SIZE) {
        uint32_t current_page_addr = addr + offset;
        if (current_page_addr < board::USERFLASH_END_ADDRESS) {
            if (!erasePage(current_page_addr)) {
                success = false;
                break;
            }
        } else {
            break;
        }
    }
    // flash_lock();
    return success;
}

void flash_clear_user(void) {
    FlashStore store; // ローカルインスタンス
    if (!store.unlock()) return;
    for (uint32_t addr = board::USERFLASH_START_ADDRESS; addr < board::USERFLASH_END_ADDRESS; addr += board::FLASH_PAGE_SIZE) {
        // flash_erase_page(addr); // 直接API呼び出しではなく、クラスメソッドを使用
        store.erasePage(addr);
    }
    store.lock();
}
