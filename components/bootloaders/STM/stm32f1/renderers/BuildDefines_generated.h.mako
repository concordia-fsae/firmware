/*
 * BuildDefines_generated.h
 *
 */

#pragma once

%if nodeId is not UNDEFINED:
#define NODE_ID          ${nodeId}U
%endif
#define UDS_REQUEST_ID   ${udsRequestId}U
#define UDS_RESPONSE_ID  ${udsResponseId}U
