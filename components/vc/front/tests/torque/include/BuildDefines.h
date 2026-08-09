#pragma once

#define FDEFS_COMPONENT_ID_VCFRONT          1
#define FDEFS_COMPONENT_ID_VCREAR           2
#define APP_COMPONENT_ID                    FDEFS_COMPONENT_ID_VCFRONT

#define FDEFS_MODE_FOLLOWER                 0
#define FDEFS_MODE_LEADER                   1

#define FEATURE_VEHICLESTATE_MODE           FDEFS_MODE_FOLLOWER
#define FEATURE_VEHICLESPEED_LEADER         0
#define FEATURE_VEHICLESPEED_USEODOMETER    1

#define FEATURE_TRACTION_CONTROL            1
#define FEATURE_REVERSE                     0
#define FEATURE_LAUNCH_CONTROL              0
#define FEATURE_REGEN                       0
#define FEATURE_LIMIT_LON_TIRE_ACCEL        0

#define MCU_STM32_USE_HAL                   0
#define NVM_LIB_ENABLED                     0

#define FEATURE_IS_ENABLED(feature)         (feature)
#define FEATURE_IS_DISABLED(feature)        (!(feature))
