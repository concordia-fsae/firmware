/*
 * FeatureDefines.h
 */

#pragma once

#include "FeatureDefines_generated.h"

#define FEATURE_DISABLED 0U
#define FEATURE_ENABLED 1U
#define FEATURE_IS_ENABLED(x) ((x) == FEATURE_ENABLED)
#define FEATURE_IS_DISABLED(x) ((x) == FEATURE_DISABLED)
