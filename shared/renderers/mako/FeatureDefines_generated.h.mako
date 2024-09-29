/*
 * FeatureDefines_generated.h
 *
 */

#pragma once

#define FEATURE_DISABLED 0
%for feature in features:
#define ${feature.upper()} ${features[feature]}
%endfor
