#include "command/command_text_parser.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace perastage::command::text {
namespace {

// Trims ASCII whitespace from both sides of text.
std::string Trim(const std::string &text) {
  const size_t start = text.find_first_not_of(" \t\n\r");
  const size_t end = text.find_last_not_of(" \t\n\r");
  return start == std::string::npos ? std::string()
                                    : text.substr(start, end - start + 1);
}

// Converts ASCII command text to lowercase.
std::string ToLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

// Splits text on a delimiter while trimming each component.
std::vector<std::string> Split(const std::string &text, char delimiter) {
  std::vector<std::string> parts;
  std::stringstream stream(text);
  std::string part;
  while (std::getline(stream, part, delimiter))
    parts.push_back(Trim(part));
  return parts;
}

// Parses a finite floating-point token in full.
bool TryParseFloat(const std::string &token, float &value) {
  if (token.empty())
    return false;
  char *end = nullptr;
  errno = 0;
  const float parsed = std::strtof(token.c_str(), &end);
  if (errno != 0 || end != token.c_str() + token.size() ||
      !std::isfinite(parsed))
    return false;
  value = parsed;
  return true;
}

// Parses an integer token in full.
bool TryParseInteger(const std::string &token, int &value) {
  if (token.empty())
    return false;
  const auto result =
      std::from_chars(token.data(), token.data() + token.size(), value);
  return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

// Appends a deterministic parse error.
void AddError(ParseResult &result, const std::string &code,
              const std::string &message) {
  result.diagnostics.push_back({DiagnosticSeverity::Error,
                                DiagnosticPhase::Parse, code, message,
                                std::nullopt});
}

// Normalizes the compact range spellings accepted by the legacy Console.
std::vector<std::string>
NormalizeRangeTokens(const std::vector<std::string> &tokens) {
  std::vector<std::string> output;
  for (const auto &token : tokens) {
    if (token == "+" || token == "-") {
      output.push_back(token);
      continue;
    }
    const std::string lower = ToLower(token);
    if (lower == "t" || lower == "thru")
      continue;
    const auto emitOne = [&](size_t start, size_t count) {
      const std::string value = token.substr(start, count);
      int ignored = 0;
      if (TryParseInteger(value, ignored)) {
        output.push_back(value);
        return true;
      }
      return false;
    };
    if (lower.size() > 4 && lower.rfind("thru", 0) == 0 &&
        emitOne(4, std::string::npos))
      continue;
    if (lower.size() > 1 && lower.rfind("t", 0) == 0 &&
        emitOne(1, std::string::npos))
      continue;
    if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, "thru") == 0 &&
        emitOne(0, token.size() - 4))
      continue;
    if (lower.size() > 1 && lower.back() == 't' && emitOne(0, token.size() - 1))
      continue;
    const size_t thru = lower.find("thru");
    const size_t shortThru = lower.find('t');
    const size_t dash = token.find('-');
    const auto emitPair = [&](size_t separator, size_t length) {
      if (separator == std::string::npos || separator == 0 ||
          separator + length >= token.size())
        return false;
      int first = 0;
      int last = 0;
      const std::string before = token.substr(0, separator);
      const std::string after = token.substr(separator + length);
      if (!TryParseInteger(before, first) || !TryParseInteger(after, last))
        return false;
      output.push_back(before);
      output.push_back(after);
      return true;
    };
    if (emitPair(thru, 4) || emitPair(shortThru, 1) ||
        (dash != std::string::npos &&
         token.find('-', dash + 1) == std::string::npos && emitPair(dash, 1)))
      continue;
    output.push_back(token);
  }
  return output;
}

// Identifies a top-level command boundary using the legacy token rules.
bool IsCommandBoundary(const std::string &token, bool allowAxis,
                       bool allowRangeSeparator) {
  if (token.empty())
    return false;
  const std::string lower = ToLower(token);
  if (allowRangeSeparator && (lower == "t" || lower == "thru"))
    return false;
  if (lower == "clear" || lower == "pos" || lower == "rot" ||
      lower.front() == 'f' || lower.front() == 't')
    return true;
  return allowAxis && (lower == "x" || lower == "y" || lower == "z");
}

// Parses a selection command without consulting scene state.
bool ParseSelection(const std::vector<std::string> &tokens,
                    SelectionTarget target, ParseResult &result) {
  SelectionCommand command{target, {}};
  const auto normalized = NormalizeRangeTokens(tokens);
  SelectionOperationKind operation = SelectionOperationKind::Add;
  for (size_t index = 0; index < normalized.size();) {
    if (normalized[index] == "+" || normalized[index] == "-") {
      operation = normalized[index] == "+" ? SelectionOperationKind::Add
                                           : SelectionOperationKind::Remove;
      ++index;
      continue;
    }
    int first = 0;
    if (!TryParseInteger(normalized[index], first)) {
      AddError(result, "command_text.invalid_selection_id",
               "Invalid selection id: " + normalized[index]);
      return false;
    }
    int last = first;
    if (index + 1 < normalized.size() && normalized[index + 1] != "+" &&
        normalized[index + 1] != "-") {
      if (!TryParseInteger(normalized[index + 1], last)) {
        AddError(result, "command_text.invalid_selection_id",
                 "Invalid selection id: " + normalized[index + 1]);
        return false;
      }
      index += 2;
    } else {
      ++index;
    }
    if (first > last)
      std::swap(first, last);
    command.operations.push_back({operation, first, last});
  }
  result.commands.push_back(std::move(command));
  return true;
}

// Parses the explicit rotation pivot in meters and stores millimeters.
bool TryParsePivot(const std::string &token, std::array<float, 3> &pivotMm) {
  const auto parts = Split(token, ',');
  if (parts.size() != 3)
    return false;
  for (size_t index = 0; index < 3; ++index) {
    float meters = 0.0f;
    if (!TryParseFloat(parts[index], meters))
      return false;
    pivotMm[index] = meters * 1000.0f;
  }
  return true;
}

// Joins a token interval with single spaces.
std::string Join(const std::vector<std::string> &tokens, size_t begin,
                 size_t end) {
  std::string joined;
  for (size_t index = begin; index < end; ++index) {
    if (!joined.empty())
      joined += ' ';
    joined += tokens[index];
  }
  return joined;
}

// Parses a transform component and assigns its positional axis.
bool ParseComponent(const std::string &text, int axis,
                    TransformComponent &component) {
  const auto segment = ParseTransformCommandSegment(text);
  if (segment.values.empty() || !segment.remainder.empty())
    return false;
  component = {axis, segment.values, segment.relative, segment.group,
               segment.space};
  return true;
}

// Parses a position or rotation command segment.
bool ParseTransform(const std::string &keyword,
                    std::vector<std::string> segmentTokens,
                    ParseResult &result) {
  TransformCommand command;
  command.kind =
      keyword == "rot" ? TransformKind::Rotation : TransformKind::Position;
  if (command.kind == TransformKind::Rotation && segmentTokens.size() > 1) {
    std::array<float, 3> pivot{};
    if (TryParsePivot(segmentTokens.back(), pivot)) {
      command.pivotMm = pivot;
      segmentTokens.pop_back();
    }
  }
  const std::string remainder = Join(segmentTokens, 0, segmentTokens.size());
  if (remainder.find(',') != std::string::npos) {
    const auto parts = Split(remainder, ',');
    for (size_t axis = 0; axis < parts.size() && axis < 3; ++axis) {
      TransformComponent component;
      if (!ParseComponent(parts[axis], static_cast<int>(axis), component)) {
        AddError(result, "command_text.invalid_transform",
                 "Transform components require finite numeric values and valid "
                 "modifiers.");
        return false;
      }
      command.components.push_back(std::move(component));
    }
  } else {
    std::stringstream stream(remainder);
    std::string axisToken;
    stream >> axisToken;
    const int axis = axisToken == "x"   ? 0
                     : axisToken == "y" ? 1
                     : axisToken == "z" ? 2
                                        : -1;
    std::string values;
    std::getline(stream, values);
    TransformComponent component;
    if (axis < 0 || !ParseComponent(Trim(values), axis, component)) {
      AddError(result, "command_text.invalid_transform",
               "Transform commands require an axis and finite numeric values.");
      return false;
    }
    command.components.push_back(std::move(component));
  }
  if (command.components.empty()) {
    AddError(result, "command_text.invalid_transform",
             "Transform commands require at least one component.");
    return false;
  }
  result.commands.push_back(std::move(command));
  return true;
}

} // namespace

// Reports whether parsing completed without diagnostics.
bool ParseResult::Success() const { return diagnostics.empty(); }

// Parses transform values and modifiers from one command segment.
TransformCommandSegment ParseTransformCommandSegment(const std::string &text) {
  TransformCommandSegment result;
  std::string remaining = Trim(text);
  float sign = 1.0f;
  if (remaining.rfind("++", 0) == 0) {
    result.relative = true;
    remaining = Trim(remaining.substr(2));
  } else if (remaining.rfind("--", 0) == 0) {
    const std::string compact = remaining.substr(2);
    const size_t end = compact.find_first_of(" \t\n\r");
    float ignored = 0.0f;
    if (remaining.size() == 2 ||
        std::isspace(static_cast<unsigned char>(remaining[2])) ||
        TryParseFloat(compact.substr(0, end), ignored)) {
      result.relative = true;
      sign = -1.0f;
      remaining = Trim(remaining.substr(2));
    }
  }
  std::stringstream stream(remaining);
  std::string token;
  bool collectingRemainder = false;
  while (stream >> token) {
    const std::string lower = ToLower(token);
    if (!collectingRemainder && (lower == "--local" || lower == "-l")) {
      result.space = transform_space::TransformSpace::Local;
      continue;
    }
    if (!collectingRemainder && (lower == "--group" || lower == "--g")) {
      result.group = true;
      continue;
    }
    if (!collectingRemainder && (lower == "t" || lower == "thru"))
      continue;
    float value = 0.0f;
    if (!collectingRemainder && TryParseFloat(token, value)) {
      result.values.push_back(sign * value);
      continue;
    }
    if (!result.remainder.empty())
      result.remainder += ' ';
    result.remainder += token;
    collectingRemainder = true;
  }
  return result;
}

// Parses transform values while retaining the legacy relative flag API.
std::vector<float> ParseTransformValues(const std::string &text,
                                        bool &relative) {
  const auto segment = ParseTransformCommandSegment(text);
  relative = segment.relative;
  return segment.values;
}

// Parses one complete Console input line into ordered neutral syntax.
ParseResult ParseCommandLine(const std::string &text) {
  ParseResult result;
  std::stringstream stream(ToLower(Trim(text)));
  std::vector<std::string> tokens;
  std::string token;
  while (stream >> token)
    tokens.push_back(token);
  size_t index = 0;
  while (index < tokens.size()) {
    const std::string &word = tokens[index];
    const bool allowAxis = word != "pos" && word != "rot";
    const bool allowRange = word == "pos" || word == "rot" || word == "x" ||
                            word == "y" || word == "z" || word.front() == 'f' ||
                            word.front() == 't';
    size_t end = index + 1;
    while (end < tokens.size() &&
           !IsCommandBoundary(tokens[end], allowAxis, allowRange))
      ++end;
    if (word == "clear") {
      result.commands.push_back(ClearCommand{});
    } else if (word == "pos" || word == "rot") {
      if (!ParseTransform(
              word, {tokens.begin() + index + 1, tokens.begin() + end}, result))
        return result;
    } else if (word == "x" || word == "y" || word == "z") {
      TransformCommand command;
      TransformComponent component;
      const int axis = word == "x" ? 0 : word == "y" ? 1 : 2;
      if (!ParseComponent(Join(tokens, index + 1, end), axis, component)) {
        AddError(result, "command_text.invalid_transform",
                 "Axis shorthand requires finite numeric values and valid "
                 "modifiers.");
        return result;
      }
      command.components.push_back(std::move(component));
      result.commands.push_back(std::move(command));
    } else if ((std::isdigit(static_cast<unsigned char>(word.front())) ||
                word.front() == '-' || word.front() == '+') &&
               word.find(',') != std::string::npos) {
      const auto parts = Split(word, ',');
      TransformCommand command;
      if (parts.size() != 3) {
        AddError(result, "command_text.invalid_transform_triplet",
                 "Bare position transforms require three finite components.");
        return result;
      }
      for (size_t axis = 0; axis < 3; ++axis) {
        TransformComponent component;
        if (!ParseComponent(parts[axis], static_cast<int>(axis), component) ||
            component.group) {
          AddError(result, "command_text.invalid_transform_triplet",
                   "Bare position transforms require three finite components.");
          return result;
        }
        command.components.push_back(std::move(component));
      }
      result.commands.push_back(std::move(command));
    } else if (word.front() == 'f' || word.front() == 't') {
      if (!ParseSelection({tokens.begin() + index + 1, tokens.begin() + end},
                          word.front() == 'f' ? SelectionTarget::Fixtures
                                              : SelectionTarget::Trusses,
                          result))
        return result;
    } else {
      AddError(result, "command_text.unknown_command",
               "The command token is not recognized.");
      return result;
    }
    index = end;
  }
  return result;
}

} // namespace perastage::command::text
