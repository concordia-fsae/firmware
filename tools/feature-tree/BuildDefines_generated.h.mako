/*
 * BuildDefines_generated.h
 */

#pragma once

%for key, value in configs.items():
#define ${key.upper()} ${value}
%endfor
