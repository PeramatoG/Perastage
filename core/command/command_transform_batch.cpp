#include "command_transform_batch.h"

#include "command/command_mutation_transaction.h"
#include "command_transform_apply.h"
#include "command_transform_batch_state.h"
#include "command_transform_batch_validation.h"

#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

namespace perastage::command::transform {

// Previews ordered exact-target transforms and publishes one complete mutation.
Result ExecuteBatch(const Request &request, ExecutionContext &context) {
  Result result;
  result.request = request;
  std::vector<detail::BatchRow> rows;
  if (!detail::ValidateBatch(request, context.scene, rows, result))
    return result;

  MutationTransaction transaction(context);
  try {
    MvrScene preview = context.scene;
    std::vector<scene_grouping::SceneTransformTarget> targets;
    targets.reserve(rows.size());
    for (const auto &row : rows) {
      if (std::fabs(row.component.values.front()) >
          std::numeric_limits<float>::max())
        throw std::overflow_error("Batch value exceeds finite matrix precision.");
      targets.push_back(row.target);
      const auto affected =
          detail::CollectAffectedTargets(preview, {row.target});
      detail::ApplyComponent(preview, {row.target}, row.kind, row.component);
      detail::RequireFiniteTransforms(preview, affected);
    }
    const auto affected = detail::CollectAffectedTargets(preview, targets);
    if (!detail::BatchTransformsChanged(context.scene, preview, affected)) {
      transaction.Rollback();
      result.diagnostics.push_back({DiagnosticSeverity::Information,
                                    DiagnosticPhase::Execution,
                                    "scene.transform.batch.noop",
                                    "Batch transforms are already in the requested state."});
      return result;
    }
    context.scene = std::move(preview);
    MutationSummary changes;
    changes.sceneChanged = true;
    result.mutation = transaction.Commit(changes, "batch transform");
  } catch (const std::exception &error) {
    transaction.Rollback();
    result.outcome = Outcome::ExecutionError;
    result.mutation = {};
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Execution,
         "scene.transform.batch.execution_failed",
         std::string("Batch transform execution failed: ") + error.what()});
  } catch (...) {
    transaction.Rollback();
    result.outcome = Outcome::ExecutionError;
    result.mutation = {};
    result.diagnostics.push_back({DiagnosticSeverity::Error,
                                  DiagnosticPhase::Execution,
                                  "scene.transform.batch.execution_failed",
                                  "Batch transform execution failed."});
  }
  return result;
}

} // namespace perastage::command::transform
