#pragma once

#include "live/live_request_executor.h"
#include "local_ipc/local_ipc_contract.h"

namespace perastage::live {

// Projects the flat wire arguments into the typed atomic transform request.
ExecutionResult ExecuteTransformBatch(const local_ipc::Request &request,
                                     command::ExecutionContext &context);

} // namespace perastage::live
