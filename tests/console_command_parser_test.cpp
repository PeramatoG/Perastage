#include "command/command_text_parser.h"

#include <cassert>
#include <cmath>
#include <variant>

namespace text = perastage::command::text;

namespace {

// Compares two float vectors with a small tolerance.
bool EqualValues(const std::vector<float> &actual,
                 const std::vector<float> &expected) {
  if (actual.size() != expected.size())
    return false;
  for (size_t index = 0; index < actual.size(); ++index)
    if (std::fabs(actual[index] - expected[index]) > 0.0001f)
      return false;
  return true;
}

// Returns the selection command stored at the requested result index.
const text::SelectionCommand &SelectionAt(const text::ParseResult &result,
                                          size_t index = 0) {
  return std::get<text::SelectionCommand>(result.commands.at(index));
}

// Returns the transform command stored at the requested result index.
const text::TransformCommand &TransformAt(const text::ParseResult &result,
                                          size_t index = 0) {
  return std::get<text::TransformCommand>(result.commands.at(index));
}

// Returns the sole diagnostic from a failed parser result.
const perastage::command::Diagnostic &
OnlyDiagnostic(const text::ParseResult &result) {
  assert(result.diagnostics.size() == 1);
  return result.diagnostics.front();
}

} // namespace

// Characterizes segment, selection, whole-line, and chaining grammar.
int main() {
  bool relative = false;
  assert(EqualValues(text::ParseTransformValues("-7 7", relative), {-7, 7}));
  assert(!relative);
  assert(EqualValues(text::ParseTransformValues("-7 t 7", relative), {-7, 7}));
  assert(!relative);
  assert(
      EqualValues(text::ParseTransformValues("-7 thru 7", relative), {-7, 7}));
  assert(!relative);
  assert(EqualValues(text::ParseTransformValues("++ 1.5 t 3.5", relative),
                     {1.5f, 3.5f}));
  assert(relative);
  assert(EqualValues(text::ParseTransformValues("-- 1.5 thru 3.5", relative),
                     {-1.5f, -3.5f}));
  assert(relative);

  auto segment = text::ParseTransformCommandSegment("++ 1 --local");
  assert(segment.relative && EqualValues(segment.values, {1.0f}));
  assert(segment.space == transform_space::TransformSpace::Local);
  segment = text::ParseTransformCommandSegment("++ 2 -l");
  assert(segment.relative && EqualValues(segment.values, {2.0f}));
  assert(segment.space == transform_space::TransformSpace::Local);
  segment = text::ParseTransformCommandSegment("++ 30 --group -l");
  assert(segment.group && segment.relative &&
         EqualValues(segment.values, {30.0f}));
  assert(segment.space == transform_space::TransformSpace::Local);
  segment = text::ParseTransformCommandSegment("-7 thru 7");
  assert(!segment.relative && EqualValues(segment.values, {-7.0f, 7.0f}));
  segment = text::ParseTransformCommandSegment("-- 1.5");
  assert(segment.relative && EqualValues(segment.values, {-1.5f}));
  segment = text::ParseTransformCommandSegment("++1.25");
  assert(segment.relative && EqualValues(segment.values, {1.25f}));
  segment = text::ParseTransformCommandSegment("--1.25");
  assert(segment.relative && EqualValues(segment.values, {-1.25f}));
  segment = text::ParseTransformCommandSegment("--1.5 thru 3.5 --local");
  assert(segment.relative && EqualValues(segment.values, {-1.5f, -3.5f}));
  assert(segment.space == transform_space::TransformSpace::Local);
  segment = text::ParseTransformCommandSegment("--group");
  assert(!segment.relative && segment.group && segment.values.empty() &&
         segment.remainder.empty());
  segment = text::ParseTransformCommandSegment("--wat");
  assert(!segment.relative && segment.values.empty() &&
         segment.remainder == "--wat");
  segment = text::ParseTransformCommandSegment("nan");
  assert(segment.values.empty() && segment.remainder == "nan");
  segment = text::ParseTransformCommandSegment("inf");
  assert(segment.values.empty() && segment.remainder == "inf");

  auto parsed = text::ParseCommandLine("f 1");
  assert(parsed.Success() && SelectionAt(parsed).operations.size() == 1);
  parsed = text::ParseCommandLine("f 5 thru 1 + 8 - 3t4");
  assert(parsed.Success());
  const auto &operations = SelectionAt(parsed).operations;
  assert(operations.size() == 3);
  assert(operations[0].firstId == 1 && operations[0].lastId == 5);
  assert(operations[2].kind == text::SelectionOperationKind::Remove);
  parsed = text::ParseCommandLine("t 1-5");
  assert(parsed.Success());
  assert(SelectionAt(parsed).target == text::SelectionTarget::Trusses);
  assert(SelectionAt(parsed).operations.size() == 1);
  for (const char *form : {"f 1t5", "f 1thru5", "f 1t 5", "f 1 thru5"})
    assert(text::ParseCommandLine(form).Success());
  parsed = text::ParseCommandLine("f nope");
  assert(!parsed.Success());
  assert(OnlyDiagnostic(parsed).phase ==
         perastage::command::DiagnosticPhase::Parse);
  assert(OnlyDiagnostic(parsed).code == "command_text.invalid_selection_id");
  assert(OnlyDiagnostic(parsed).message == "Invalid selection id: nope");

  assert(text::ParseCommandLine("clear").Success());
  parsed = text::ParseCommandLine("X ++1 --LOCAL");
  assert(parsed.Success() && TransformAt(parsed).components.front().axis == 0);
  parsed = text::ParseCommandLine("pos x -7 t 7");
  assert(parsed.Success() &&
         EqualValues(TransformAt(parsed).components.front().values, {-7, 7}));
  parsed = text::ParseCommandLine("pos 1,2,3");
  assert(parsed.Success() && TransformAt(parsed).components.size() == 3);
  parsed = text::ParseCommandLine("rot 1,2,3");
  assert(parsed.Success() &&
         TransformAt(parsed).kind == text::TransformKind::Rotation);
  parsed = text::ParseCommandLine("rot y ++45 --g --local -2.5,0,0");
  assert(parsed.Success() && TransformAt(parsed).pivotMm.has_value());
  assert(TransformAt(parsed).components.front().group);
  parsed = text::ParseCommandLine("1,2,3");
  assert(parsed.Success() && TransformAt(parsed).components.size() == 3);
  parsed = text::ParseCommandLine("F 1 POS X 2 ROT Z --10 CLEAR");
  assert(parsed.Success() && parsed.commands.size() == 4);
  assert(std::holds_alternative<text::SelectionCommand>(parsed.commands[0]));
  assert(std::holds_alternative<text::ClearCommand>(parsed.commands[3]));
  parsed = text::ParseCommandLine("unknown");
  assert(!parsed.Success());
  assert(OnlyDiagnostic(parsed).code == "command_text.unknown_command");
  parsed = text::ParseCommandLine("pos q 1");
  assert(!parsed.Success());
  assert(OnlyDiagnostic(parsed).code == "command_text.invalid_transform");
  parsed = text::ParseCommandLine("1,2");
  assert(!parsed.Success());
  assert(OnlyDiagnostic(parsed).code ==
         "command_text.invalid_transform_triplet");
  const auto repeatedFailure = text::ParseCommandLine("clear pos q 1");
  assert(!repeatedFailure.Success() && repeatedFailure.commands.size() == 1);
  assert(OnlyDiagnostic(repeatedFailure).code ==
         "command_text.invalid_transform");
  assert(OnlyDiagnostic(repeatedFailure).message ==
         OnlyDiagnostic(text::ParseCommandLine("clear pos q 1")).message);
  assert(text::ParseCommandLine("pos x 1").Success());
  return 0;
}
