#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include "common.hpp"
#include "board_v2_plus4/board.hpp" // board.hpp をインクルード
#include "vna_measurement.hpp"    // SweepProperties, VNAMeasurement のため
#include "calibration.hpp"      // CalibrationData のため
// spi_config.h は board.hpp 経由でインクルードされるか、ここで直接インクルード

// spi_config.h で #define されているため、ここでは extern 宣言しない
// extern volatile int MEASUREMENT_NPERIODS_NORMAL;
// extern volatile int MEASUREMENT_NPERIODS_CALIBRATING;
// extern volatile int MEASUREMENT_ECAL_INTERVAL;
// extern volatile int MEASUREMENT_NWAIT_SWITCH;

extern SweepProperties current_props;
extern VNAMeasurement vnaMeasurement;

#if BOARD_REVISION >= 4 // HW_SWEEPが定義されているかはMakefileやプロジェクト設定による
struct SweepArgs; // vna_measurement.hpp で定義されているはずだが、前方宣言も可能
extern SweepArgs currSweepArgs;
#endif

// 他のグローバル変数があればここに

#endif // GLOBALS_HPP
