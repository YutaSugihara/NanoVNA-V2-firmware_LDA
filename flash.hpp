#ifndef FLASH_HPP
#define FLASH_HPP

#include <cstdint>
#include <cstddef> // For size_t
#include "board_v2_plus4/board.hpp" // For board::USERFLASH_END etc.
#include "calibration.hpp"      // For CalibrationData
#include "common.hpp"           // For complexf, freqHz_t

// SWEEP_POINTS_MAX と SAVEAREA_MAX は Makefile から渡される想定
// (PROJECT_DEFINES または EXTRA_CFLAGS 経由で)
#ifndef SWEEP_POINTS_MAX
#define SWEEP_POINTS_MAX 201 // フォールバック
#endif
#ifndef SAVEAREA_MAX
#define SAVEAREA_MAX 7       // フォールバック
#endif

// SINGLE_SAVE_AREA_SIZE は CalibrationData と complexf/freqHz_t に依存
#define SINGLE_SAVE_AREA_SIZE ( (SWEEP_POINTS_MAX * (sizeof(complexf) * 2 + sizeof(freqHz_t))) + sizeof(CalibrationData) + 256 )
#define SAVETOTAL_BYTES ( (SAVEAREA_MAX) * SINGLE_SAVE_AREA_SIZE )
#define CONFIGAREA_BYTES (1024)

// USERFLASH_BEGIN は board.hpp の board 名前空間内の定数を使用
// constexpr uint32_t USERFLASH_BEGIN_CALC = board::USERFLASH_END_ADDRESS - SAVETOTAL_BYTES - CONFIGAREA_BYTES + 1;
// ↑ USERFLASH_END_ADDRESS を使うと循環定義になる可能性があるので注意。
//   USERFLASH_START_ADDRESS を基準にする方が安全。
//   board.hpp で USERFLASH_START_ADDRESS が定義されている前提。


class FlashStore {
public:
    FlashStore();
    bool unlock();
    void lock();
    bool erasePage(uint32_t address);
    bool programHalfWord(uint32_t address, uint16_t data);
    bool programWord(uint32_t address, uint32_t data);
    bool programBlock(uint32_t address, const uint8_t *data, size_t len);
    void readBlock(uint32_t address, uint8_t *data, size_t len);

    uint32_t get_savearea_addr(int slot);
    uint32_t get_configarea_addr();
    bool erase_savearea(int slot);
    bool erase_configarea();
};

void flash_clear_user(void);

#endif // FLASH_HPP
