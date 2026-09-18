#pragma once

#include <cmath>
#include <cstdlib>
#include <cwctype>
#include <optional>
#include <string>
#include <wx/variant.h>

namespace DataViewSort {

enum class Mode { Text, NaturalText, Integer, Decimal, Boolean };

// Parses the leading number and an optional recognized display-unit suffix.
inline std::optional<double> ParseNumericText(const wxString &input) {
  wxString value = input;
  value.Trim(true).Trim(false);
  if (value.empty())
    return std::nullopt;
  size_t numericEnd = 0;
  while (numericEnd < value.length()) {
    const wxUniChar character = value[numericEnd];
    if (!(character >= '0' && character <= '9') && character != '+' &&
        character != '-' && character != '.' && character != ',')
      break;
    ++numericEnd;
  }
  std::string normalized = value.Left(numericEnd).ToStdString();
  const size_t comma = normalized.find(',');
  const size_t dot = normalized.find('.');
  if (comma != std::string::npos) {
    if (dot != std::string::npos ||
        normalized.find(',', comma + 1) != std::string::npos)
      return std::nullopt;
    normalized[comma] = '.';
  }
  char *end = nullptr;
  const double parsed = std::strtod(normalized.c_str(), &end);
  if (end == normalized.c_str() || !std::isfinite(parsed))
    return std::nullopt;
  if (end != normalized.c_str() + normalized.size())
    return std::nullopt;
  wxString suffix = value.Mid(numericEnd);
  suffix.Trim(true).Trim(false).MakeLower();
  static const wxString suffixes[] = {"",   wxString::FromUTF8("\xC2\xB0"),
                                      "m",  "mm",
                                      "cm", "ft",
                                      "in", "kg",
                                      "lb", "w",
                                      "kw", "n"};
  for (const auto &recognized : suffixes)
    if (suffix == recognized)
      return parsed;
  return std::nullopt;
}

// Compares text by alternating case-insensitive text and integer runs.
inline int CompareNaturalText(const wxString &lhs, const wxString &rhs) {
  size_t left = 0;
  size_t right = 0;
  while (left < lhs.length() && right < rhs.length()) {
    if (std::iswdigit(lhs[left].GetValue()) &&
        std::iswdigit(rhs[right].GetValue())) {
      size_t leftEnd = left;
      size_t rightEnd = right;
      while (leftEnd < lhs.length() && std::iswdigit(lhs[leftEnd].GetValue()))
        ++leftEnd;
      while (rightEnd < rhs.length() && std::iswdigit(rhs[rightEnd].GetValue()))
        ++rightEnd;
      wxString leftDigits = lhs.Mid(left, leftEnd - left);
      wxString rightDigits = rhs.Mid(right, rightEnd - right);
      while (leftDigits.length() > 1 && leftDigits[0] == '0')
        leftDigits.erase(0, 1);
      while (rightDigits.length() > 1 && rightDigits[0] == '0')
        rightDigits.erase(0, 1);
      if (leftDigits.empty())
        leftDigits = "0";
      if (rightDigits.empty())
        rightDigits = "0";
      if (leftDigits.length() != rightDigits.length())
        return leftDigits.length() < rightDigits.length() ? -1 : 1;
      const int digitResult = leftDigits.Cmp(rightDigits);
      if (digitResult != 0)
        return digitResult;
      left = leftEnd;
      right = rightEnd;
      continue;
    }
    const wchar_t leftChar = std::towlower(lhs[left].GetValue());
    const wchar_t rightChar = std::towlower(rhs[right].GetValue());
    if (leftChar != rightChar)
      return leftChar < rightChar ? -1 : 1;
    ++left;
    ++right;
  }
  if (left != lhs.length() || right != rhs.length())
    return left == lhs.length() ? -1 : 1;
  return lhs.Cmp(rhs);
}

// Extracts numeric wxVariant types without converting them through text.
inline std::optional<long double> NativeNumber(const wxVariant &value) {
  const wxString type = value.GetType();
  if (type == "long")
    return value.GetLong();
  if (type == "longlong")
    return value.GetLongLong().GetValue();
  if (type == "ulonglong")
    return value.GetULongLong().GetValue();
  if (type == "double")
    return value.GetDouble();
  return std::nullopt;
}

// Compares variants according to an explicit semantic mode.
inline int Compare(const wxVariant &lhs, const wxVariant &rhs, Mode mode) {
  if (lhs.GetType() == "bool" && rhs.GetType() == "bool")
    return static_cast<int>(lhs.GetBool()) - static_cast<int>(rhs.GetBool());
  const auto leftNative = NativeNumber(lhs);
  const auto rightNative = NativeNumber(rhs);
  if (leftNative && rightNative)
    return *leftNative < *rightNative ? -1
                                      : (*leftNative > *rightNative ? 1 : 0);
  if (mode == Mode::Boolean)
    return static_cast<int>(lhs.GetBool()) - static_cast<int>(rhs.GetBool());
  if (mode == Mode::Integer || mode == Mode::Decimal) {
    const auto leftNumber = ParseNumericText(lhs.GetString());
    const auto rightNumber = ParseNumericText(rhs.GetString());
    if (leftNumber && rightNumber)
      return *leftNumber < *rightNumber ? -1
                                        : (*leftNumber > *rightNumber ? 1 : 0);
    // Valid numbers precede invalid text; two invalid values use plain text.
    if (leftNumber != rightNumber)
      return leftNumber ? -1 : 1;
    return lhs.GetString().Cmp(rhs.GetString());
  }
  if (mode == Mode::NaturalText)
    return CompareNaturalText(lhs.GetString(), rhs.GetString());
  return lhs.GetString().Cmp(rhs.GetString());
}

} // namespace DataViewSort
