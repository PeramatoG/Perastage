#pragma once

#include "live/live_request_executor.h"
#include "local_ipc/local_ipc_contract.h"

namespace perastage::live {

// Adapts structured live arguments and serializes existing Query Core values.
ExecutionResult ExecuteQuery(const local_ipc::Request &request,
                             const command::ExecutionContext &context);

} // namespace perastage::live
