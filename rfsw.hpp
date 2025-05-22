#ifndef RFSW_HPP
#define RFSW_HPP

#include <mculib/fastwiring.hpp> // Pad, digitalWrite のため
#include <array>                 // std::array のため
#include <cstdint>               // uint8_t のため

enum class RFSWState {
    THRU,
    REFL,
    LOAD
};

class RFSW {
public:
    Pad sw0, sw1; // mculib/fastwiring.hpp が Pad をグローバルに定義すると仮定

    RFSW(Pad p0, Pad p1) : sw0(p0), sw1(p1) {}

    void init() {
        pinMode(sw0, OUTPUT);
        pinMode(sw1, OUTPUT);
        setState(RFSWState::THRU); // 初期状態
    }

    void setState(RFSWState state) {
        uint8_t val = 0;
        switch (state) {
            case RFSWState::THRU: val = 0b01; break; // 例: SW0=1, SW1=0
            case RFSWState::REFL: val = 0b10; break; // 例: SW0=0, SW1=1
            case RFSWState::LOAD: val = 0b11; break; // 例: SW0=1, SW1=1 (または00)
            default: val = 0b01; // 安全なデフォルト
        }
        digitalWrite(sw0, (val & 1) != 0);
        digitalWrite(sw1, (val & 2) != 0);
    }
};

// グローバルオブジェクトのextern宣言は board.hpp に移動
// extern RFSW rfsw;

// static inline 関数はクラスメソッドに統合するか、mculib::Pad を使用する形に
// static inline void rfsw_control(Pad sw, int state) {
//     digitalWrite(sw, state);
// }

// static inline void rfsw_set(std::array<Pad, 2> sw, RFSWState state) {
//     uint8_t val = 0;
//     switch (state) {
//         case RFSWState::THRU: val = 0b01; break;
//         case RFSWState::REFL: val = 0b10; break;
//         case RFSWState::LOAD: val = 0b11; break;
//     }
//     digitalWrite(sw[0], (val & 1) != 0);
//     digitalWrite(sw[1], (val & 2) != 0);
// }

#endif // RFSW_HPP
