#pragma once

#include "live/live_request_executor.h"
#include "local_ipc/local_ipc_contract.h"

namespace perastage::live {

// Decodes the semantic selection contract and invokes Command Core once.
ExecutionResult ExecuteSelectionUpdate(const local_ipc::Request &request,
                                       command::ExecutionContext &context);

} // namespace perastage::live
