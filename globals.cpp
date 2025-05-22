#include "globals.hpp"
#include "board.hpp"      // For extern declarations of global objects like current_props
#include "spi_config.h" // For MEASUREMENT_... constants

// Global variables
SweepProperties current_props; // Definition

// These are likely defined in main2.cpp or board.cpp as global instances
// extern VNAMeasurement vnaMeasurement;
// extern Plot plot;
// extern UI ui;
// extern CalibrationStore cal_store;
// extern CommandParser cmdParser;
// extern SweepArgs currSweepArgs;


// These variables are initialized using #defines from spi_config.h
volatile int cfg_measurement_nperiods_normal = MEASUREMENT_NPERIODS_NORMAL;
volatile int cfg_measurement_nperiods_calibrating = MEASUREMENT_NPERIODS_CALIBRATING;
volatile int cfg_measurement_ecal_interval = MEASUREMENT_ECAL_INTERVAL;
volatile int cfg_measurement_nwait_switch = MEASUREMENT_NWAIT_SWITCH;

// Ensure that if these are also declared as extern in other .hpp files,
// they are defined (not extern) in exactly one .cpp file (this one).
// current_props is defined above.
