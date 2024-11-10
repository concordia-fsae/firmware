/*
 * BuildDefines_generated.h
 *
 */

#pragma once

%if nvmLibEnabled is not UNDEFINED or nvmBlockSize is not UNDEFINED:
  %if nvmLibEnabled is not UNDEFINED and nvmBlockSize is not UNDEFINED:
#include "FeatureDefines_generated.h"
#define NVM_LIB_ENABLED  FEATURE_ENABLED
#define NVM_BLOCK_SIZE   ${nvmBlockSize}
  %else:
#error "NVM lib must be anabled with a defined NVM block size"
  %endif
%else:
#define NVM_LIB_ENABLED  FEATURE_DISABLED
%endif
%if nodeId is not UNDEFINED:
#define NODE_ID          ${nodeId}U
%endif
#define UDS_REQUEST_ID   ${udsRequestId}U
#define UDS_RESPONSE_ID  ${udsResponseId}U
