/*
 * BuildDefines_generated.h
 */

#pragma once

%for key, value in variants.items():
#define ${key.upper()} ${value}
%endfor
