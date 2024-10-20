/*
 * FeatureDefines_generated.h
 */

 #pragma once

#define FEATURE_DISABLED 0U
#define FEATURE_ENABLED 1U
#define FEATURE_IS_ENABLED(x) ((x) == FEATURE_ENABLED)
#define FEATURE_IS_DISABLED(x) ((x) == FEATURE_DISABLED)
<%
  i = 2 # Must start at 2 so no definition can be equal to FEATURE_ENABLED
%>
// Bring to a secondary file
%for values in features["discreteValues"]:
  %for value in features["discreteValues"][values]:
#define FDEFS_${values.upper()}_${value.upper()} ${i}U
<%
  i += 1
%>\
  %endfor
%endfor

%for feature in sorted(features["defs"]):
  %if type(features["defs"][feature]) is str:
#define ${feature.upper()} FDEFS_${features["defs"][feature].upper()}
  %elif type(features["defs"][feature]) is bool:
      %if features["defs"][feature]:
#define ${feature.upper()} FEATURE_ENABLED
      %else:
#define ${feature.upper()} FEATURE_DISABLED
      %endif
  %elif isinstance(features["defs"][feature], int):
      %if features["defs"][feature] >= 0:
#define ${feature.upper()} ${hex(features["defs"][feature])}
      %else:
#define ${feature.upper()} ${features["defs"][feature]}
      %endif
  %elif isinstance(features["defs"][feature], float):
#define ${feature.upper()} ${features["defs"][feature]}f
  %endif
%endfor
