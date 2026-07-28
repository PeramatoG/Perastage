/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pdftext.h"

#include <podofo/podofo.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string_view>

using namespace PoDoFo;

namespace {

constexpr double kFallbackCoordinateTolerance = 2.0;

// Returns the fragment coordinate used to order text horizontally.
double FragmentLeft(const PdfTextFragment &fragment) {
  return fragment.hasBoundingBox ? fragment.left : fragment.x;
}

// Returns the best available end position for measuring an inter-fragment gap.
double FragmentRight(const PdfTextFragment &fragment) {
  const double start = FragmentLeft(fragment);
  const double advanced = fragment.x + std::max(0.0, fragment.advance);
  if (fragment.hasBoundingBox && fragment.right > start)
    return std::max(fragment.right, advanced);
  return std::max(start, advanced);
}

// Derives a coordinate tolerance from fragment height with a conservative
// fallback.
double GeometryTolerance(const PdfTextFragment &fragment) {
  if (fragment.hasBoundingBox) {
    const double height = std::fabs(fragment.top - fragment.bottom);
    if (height > 0.0)
      return std::max(0.5, height * 0.2);
  }
  return kFallbackCoordinateTolerance;
}

// Removes embedded NUL bytes required by the historical string API contract.
void RemoveEmbeddedNul(std::string &text) {
  text.erase(std::remove(text.begin(), text.end(), '\0'), text.end());
}

#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
struct TextMatrix {
  double a = 1.0;
  double b = 0.0;
  double c = 0.0;
  double d = 1.0;
  double e = 0.0;
  double f = 0.0;
};

struct OperationTextState {
  const PdfFont *font = nullptr;
  PdfTextState text;
  TextMatrix matrix;
  TextMatrix lineMatrix;
  TextMatrix ctm;
  double leading = 0.0;
  double rise = 0.0;
  bool inText = false;
};

// Multiplies two PDF affine matrices in content-stream order.
TextMatrix Multiply(const TextMatrix &left, const TextMatrix &right) {
  return {left.a * right.a + left.c * right.b,
          left.b * right.a + left.d * right.b,
          left.a * right.c + left.c * right.d,
          left.b * right.c + left.d * right.d,
          left.a * right.e + left.c * right.f + left.e,
          left.b * right.e + left.d * right.f + left.f};
}

// Creates a PDF affine matrix from six content-stream operands.
TextMatrix ReadMatrix(const PdfVariantStack &stack) {
  return {stack[5].GetReal(), stack[4].GetReal(), stack[3].GetReal(),
          stack[2].GetReal(), stack[1].GetReal(), stack[0].GetReal()};
}

// Advances the current text matrix horizontally in text space.
void AdvanceText(OperationTextState &state, double amount) {
  state.matrix.e += amount * state.matrix.a;
  state.matrix.f += amount * state.matrix.b;
}

// Moves the text line matrix and resets the current text matrix.
void MoveTextLine(OperationTextState &state, double x, double y) {
  const TextMatrix translation{1.0, 0.0, 0.0, 1.0, x, y};
  state.lineMatrix = Multiply(state.lineMatrix, translation);
  state.matrix = state.lineMatrix;
}

// Decodes and positions one text-show string with public PoDoFo font metrics.
void EmitString(const PdfString &encoded, OperationTextState &state,
                std::vector<PdfTextFragment> &fragments) {
  if (!state.inText || state.font == nullptr)
    throw std::runtime_error(
        "PDF text-show operation has no active font metrics.");
  std::string decoded;
  std::vector<double> lengths;
  std::vector<unsigned> positions;
  if (!state.font->TryScanEncodedString(encoded, state.text, decoded, lengths,
                                        positions))
    throw std::runtime_error(
        "Unable to decode PDF text with active font metrics.");
  double advance = 0.0;
  for (const double length : lengths)
    advance += length;
  const TextMatrix rendered = Multiply(state.ctm, state.matrix);
  const double endX = rendered.e + advance * rendered.a;
  const double endY = rendered.f + advance * rendered.b;
  PdfTextFragment fragment;
  fragment.text = std::move(decoded);
  fragment.x = rendered.e;
  fragment.y = rendered.f + state.rise;
  fragment.advance = std::hypot(endX - rendered.e, endY - rendered.f);
  fragment.left = std::min(rendered.e, endX);
  fragment.right = std::max(rendered.e, endX);
  fragment.bottom = fragment.y;
  fragment.top = fragment.y + std::fabs(state.text.FontSize);
  fragment.hasBoundingBox = true;
  if (!fragment.text.empty())
    fragments.push_back(std::move(fragment));
  AdvanceText(state, advance);
}

struct GraphicsStateFrame {
  TextMatrix ctm;
  const PdfFont *font = nullptr;
  PdfTextState text;
  double leading = 0.0;
  double rise = 0.0;
  const PdfResources *resources = nullptr;
};

struct FormStateFrame {
  OperationTextState state;
  const PdfResources *resources = nullptr;
  size_t graphicsDepth = 0;
};

// Validates the operand count before an operation reads from its stack.
void RequireOperands(const PdfVariantStack &stack, size_t minimum,
                     const char *operation) {
  if (stack.GetSize() < minimum)
    throw std::runtime_error(std::string("PDF ") + operation +
                             " operation has too few operands.");
}

// Saves the graphics and text-state parameters controlled by q/Q.
GraphicsStateFrame SaveGraphicsState(const OperationTextState &state,
                                     const PdfResources *resources) {
  return {state.ctm,     state.font, state.text,
          state.leading, state.rise, resources};
}

// Restores q/Q state without incorrectly rewinding active text matrices.
void RestoreGraphicsState(OperationTextState &state,
                          const GraphicsStateFrame &frame,
                          const PdfResources *&resources) {
  state.ctm = frame.ctm;
  state.font = frame.font;
  state.text = frame.text;
  state.leading = frame.leading;
  state.rise = frame.rise;
  resources = frame.resources;
}

// Applies one validated content-stream operator to extraction state.
void ProcessOperator(PdfOperator operation, const PdfVariantStack &stack,
                     OperationTextState &state, const PdfResources *&resources,
                     std::vector<GraphicsStateFrame> &graphicsStack,
                     std::vector<PdfTextFragment> &fragments) {
  switch (operation) {
  case PdfOperator::q:
    graphicsStack.push_back(SaveGraphicsState(state, resources));
    break;
  case PdfOperator::Q:
    if (graphicsStack.empty())
      throw std::runtime_error(
          "PDF graphics-state restore has no saved state.");
    RestoreGraphicsState(state, graphicsStack.back(), resources);
    graphicsStack.pop_back();
    break;
  case PdfOperator::cm:
    RequireOperands(stack, 6, "cm");
    state.ctm = Multiply(state.ctm, ReadMatrix(stack));
    break;
  case PdfOperator::BT:
    state.matrix = state.lineMatrix = TextMatrix{};
    state.inText = true;
    break;
  case PdfOperator::ET:
    state.inText = false;
    break;
  case PdfOperator::Tf:
    RequireOperands(stack, 2, "Tf");
    state.text.FontSize = stack[0].GetReal();
    state.font = resources == nullptr
                     ? nullptr
                     : resources->GetFont(stack[1].GetName().GetString());
    state.text.Font = state.font;
    break;
  case PdfOperator::Tc:
    RequireOperands(stack, 1, "Tc");
    state.text.CharSpacing = stack[0].GetReal();
    break;
  case PdfOperator::Tw:
    RequireOperands(stack, 1, "Tw");
    state.text.WordSpacing = stack[0].GetReal();
    break;
  case PdfOperator::Tz:
    RequireOperands(stack, 1, "Tz");
    state.text.FontScale = stack[0].GetReal() / 100.0;
    break;
  case PdfOperator::TL:
    RequireOperands(stack, 1, "TL");
    state.leading = stack[0].GetReal();
    break;
  case PdfOperator::Ts:
    RequireOperands(stack, 1, "Ts");
    state.rise = stack[0].GetReal();
    break;
  case PdfOperator::Td:
  case PdfOperator::TD: {
    RequireOperands(stack, 2, operation == PdfOperator::Td ? "Td" : "TD");
    const double x = stack[1].GetReal();
    const double y = stack[0].GetReal();
    if (operation == PdfOperator::TD)
      state.leading = -y;
    MoveTextLine(state, x, y);
    break;
  }
  case PdfOperator::Tm:
    RequireOperands(stack, 6, "Tm");
    state.matrix = state.lineMatrix = ReadMatrix(stack);
    break;
  case PdfOperator::T_Star:
    MoveTextLine(state, 0.0, -state.leading);
    break;
  case PdfOperator::Quote:
    RequireOperands(stack, 1, "quote");
    MoveTextLine(state, 0.0, -state.leading);
    EmitString(stack[0].GetString(), state, fragments);
    break;
  case PdfOperator::DoubleQuote:
    RequireOperands(stack, 3, "double-quote");
    state.text.WordSpacing = stack[2].GetReal();
    state.text.CharSpacing = stack[1].GetReal();
    MoveTextLine(state, 0.0, -state.leading);
    EmitString(stack[0].GetString(), state, fragments);
    break;
  case PdfOperator::Tj:
    RequireOperands(stack, 1, "Tj");
    EmitString(stack[0].GetString(), state, fragments);
    break;
  case PdfOperator::TJ: {
    RequireOperands(stack, 1, "TJ");
    const auto &array = stack[0].GetArray();
    for (size_t i = 0; i < array.GetSize(); ++i) {
      if (array[i].IsString())
        EmitString(array[i].GetString(), state, fragments);
      else if (array[i].IsNumber())
        AdvanceText(state, -array[i].GetReal() / 1000.0 * state.text.FontSize *
                               state.text.FontScale);
    }
    break;
  }
  default:
    break;
  }
}

// Extracts text through the public PoDoFo 1.1.1 content-reader interface.
std::vector<PdfTextFragment> ExtractOperationFragments(PdfPage &page) {
  std::vector<PdfTextFragment> fragments;
  OperationTextState state;
  state.text.FontScale = 1.0;
  const PdfResources *resources = &page.GetResources();
  std::vector<GraphicsStateFrame> graphicsStack;
  std::vector<FormStateFrame> formStack;
  PdfContentReaderArgs args;
  args.Flags = PdfContentReaderFlags::SkipHandleNonFormXObjects |
               PdfContentReaderFlags::SkipFetchInlineImages;
  PdfContentStreamReader reader(page, args);
  PdfContent content;
  while (reader.TryReadNext(content)) {
    switch (content.GetType()) {
    case PdfContentType::Operator:
      if (content.HasErrors())
        throw std::runtime_error("PDF content reader rejected an operator.");
      ProcessOperator(content.GetOperator(), content.GetStack(), state,
                      resources, graphicsStack, fragments);
      break;
    case PdfContentType::BeginFormXObject: {
      const auto xobject = content.GetXObject();
      if (xobject == nullptr || xobject->GetType() != PdfXObjectType::Form)
        throw std::runtime_error("PDF form entry has no Form XObject.");
      formStack.push_back({state, resources, graphicsStack.size()});
      const auto *form = static_cast<const PdfXObjectForm *>(xobject.get());
      const auto &matrix = form->GetMatrix();
      const TextMatrix formMatrix{matrix[0], matrix[1], matrix[2],
                                  matrix[3], matrix[4], matrix[5]};
      state.ctm = Multiply(state.ctm, formMatrix);
      if (const auto *formResources = form->GetResources())
        resources = formResources;
      break;
    }
    case PdfContentType::EndFormXObject:
      if (formStack.empty())
        throw std::runtime_error("PDF form exit has no matching form entry.");
      if (graphicsStack.size() != formStack.back().graphicsDepth)
        throw std::runtime_error("PDF form has an unbalanced graphics state.");
      state = formStack.back().state;
      resources = formStack.back().resources;
      formStack.pop_back();
      break;
    case PdfContentType::ImageDictionary:
    case PdfContentType::ImageData:
    case PdfContentType::DoXObject:
      break;
    case PdfContentType::UnexpectedKeyword:
      if (content.HasErrors())
        throw std::runtime_error(
            "PDF content reader found an unexpected token.");
      break;
    default:
      throw std::runtime_error(
          "PDF content reader returned an unsupported type.");
    }
  }
  if (!formStack.empty())
    throw std::runtime_error("PDF form entry has no matching form exit.");
  if (!graphicsStack.empty())
    throw std::runtime_error(
        "PDF graphics-state save has no matching restore.");
  return fragments;
}

// Converts a PoDoFo entry into the dependency-neutral reconstruction model.
PdfTextFragment ConvertEntry(const PdfTextEntry &entry) {
  PdfTextFragment fragment;
  fragment.text = entry.Text;
  fragment.x = entry.X;
  fragment.y = entry.Y;
  fragment.advance = entry.Length;
  if (entry.BoundingBox.has_value()) {
    fragment.hasBoundingBox = true;
    fragment.left = entry.BoundingBox->GetLeft();
    fragment.right = entry.BoundingBox->GetRight();
    fragment.bottom = entry.BoundingBox->GetBottom();
    fragment.top = entry.BoundingBox->GetTop();
  }
  return fragment;
}
#endif

// Classifies a PoDoFo failure without exposing dependency types through the
// API.
std::string PdfErrorMessage(const PdfError &error) {
  const std::string detail = error.what();
  if (detail.find("Unsupported") != std::string::npos)
    return "Unsupported PDF content: " + detail;
  return "Malformed or unreadable PDF: " + detail;
}

} // namespace

// Reconstructs deterministic lines and spaces from positioned text fragments.
std::string ReconstructPdfText(std::vector<PdfTextFragment> fragments) {
  std::stable_sort(fragments.begin(), fragments.end(),
                   [](const PdfTextFragment &a, const PdfTextFragment &b) {
                     return a.y > b.y;
                   });
  for (auto lineBegin = fragments.begin(); lineBegin != fragments.end();) {
    auto lineEnd = lineBegin + 1;
    while (lineEnd != fragments.end() &&
           std::fabs(lineEnd->y - lineBegin->y) <=
               std::max(GeometryTolerance(*lineBegin),
                        GeometryTolerance(*lineEnd))) {
      ++lineEnd;
    }
    std::stable_sort(lineBegin, lineEnd,
                     [](const PdfTextFragment &a, const PdfTextFragment &b) {
                       return FragmentLeft(a) < FragmentLeft(b);
                     });
    lineBegin = lineEnd;
  }

  std::string text;
  const PdfTextFragment *previous = nullptr;
  for (const auto &fragment : fragments) {
    if (previous != nullptr) {
      const double tolerance =
          std::max(GeometryTolerance(*previous), GeometryTolerance(fragment));
      if (std::fabs(fragment.y - previous->y) > tolerance) {
        text += '\n';
      } else {
        const double gap = FragmentLeft(fragment) - FragmentRight(*previous);
        const bool alreadySeparated =
            (!text.empty() && text.back() == ' ') ||
            (!fragment.text.empty() && fragment.text.front() == ' ');
        if (gap > tolerance && !alreadySeparated)
          text += ' ';
      }
    }
    text += fragment.text;
    previous = &fragment;
  }
  RemoveEmbeddedNul(text);
  return text;
}

// Extracts PDF text and returns parser status separately from extracted
// content.
PdfTextExtractionResult ExtractPdfTextWithResult(const std::string &path) {
  PdfTextExtractionResult result;
  std::error_code filesystemError;
  if (!std::filesystem::is_regular_file(path, filesystemError)) {
    result.error = "Unable to read PDF file: " + path;
    return result;
  }

  try {
    PdfMemDocument document;
    document.Load(path.c_str());
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0, 10, 0)
    auto &pages = document.GetPages();
    for (unsigned i = 0; i < pages.GetCount(); ++i) {
      auto &page = pages.GetPageAt(i);
      PdfTextExtractParams params;
      params.Flags = PdfTextExtractFlags::ComputeBoundingBox;
      std::vector<PdfTextEntry> entries;
      page.ExtractTextTo(entries, std::string_view{}, params);
      std::vector<PdfTextFragment> fragments;
      fragments.reserve(entries.size());
      std::transform(entries.begin(), entries.end(),
                     std::back_inserter(fragments), ConvertEntry);
      result.pages.push_back(fragments);
      // The public high-level API is retained for diagnostics, while operation
      // extraction preserves Standard 14 text-show boundaries before grouping.
      const std::string pageText =
          ReconstructPdfText(ExtractOperationFragments(page));
      if (i != 0 && (!result.text.empty() || !pageText.empty()))
        result.text += '\n';
      result.text += pageText;
    }
#else
    for (int i = 0; i < document.GetPageCount(); ++i) {
      PdfPage *page = document.GetPage(i);
      PdfContentsTokenizer tokenizer(page);
      EPdfContentsType type;
      const char *token = nullptr;
      PdfVariant value;
      std::vector<PdfVariant> operands;
      PdfFont *font = nullptr;
      double fontSize = 0.0;
      double currentX = 0.0;
      double lastRight = 0.0;
      bool firstOnLine = true;
      while (tokenizer.ReadNext(type, token, value)) {
        if (type == ePdfContentsType_Variant) {
          operands.push_back(value);
        } else if (type == ePdfContentsType_Keyword) {
          if (std::strcmp(token, "BT") == 0) {
            currentX = lastRight = 0.0;
            firstOnLine = true;
          } else if (std::strcmp(token, "ET") == 0) {
            if (!result.text.empty() && result.text.back() != '\n')
              result.text += '\n';
          } else if (std::strcmp(token, "Tf") == 0 && operands.size() >= 2) {
            fontSize = operands.back().GetReal();
            operands.pop_back();
            const PdfName fontName = operands.back().GetName();
            operands.pop_back();
            PdfObject *fontObject =
                page->GetFromResources(PdfName("Font"), fontName);
            if (fontObject)
              font = document.GetFont(fontObject);
          } else if ((std::strcmp(token, "Td") == 0 ||
                      std::strcmp(token, "TD") == 0) &&
                     operands.size() >= 2) {
            const double y = operands.back().GetReal();
            operands.pop_back();
            currentX += operands.back().GetReal();
            operands.pop_back();
            if (y != 0.0)
              firstOnLine = true;
          } else if ((std::strcmp(token, "Tj") == 0 ||
                      std::strcmp(token, "'") == 0 ||
                      std::strcmp(token, "\"") == 0) &&
                     !operands.empty() && font) {
            const PdfString string = operands.back().GetString();
            operands.pop_back();
            if (!firstOnLine && currentX - lastRight > fontSize * 0.2)
              result.text += ' ';
            result.text += string.GetStringUtf8();
            lastRight = currentX + font->GetFontMetrics()->StringWidth(string) *
                                       fontSize / 1000.0;
            currentX = lastRight;
            firstOnLine = false;
            if (std::strcmp(token, "'") == 0 || std::strcmp(token, "\"") == 0) {
              result.text += '\n';
              firstOnLine = true;
            }
          } else if (std::strcmp(token, "TJ") == 0 && !operands.empty() &&
                     font) {
            const PdfArray array = operands.back().GetArray();
            operands.pop_back();
            for (size_t j = 0; j < array.GetSize(); ++j) {
              if (array[j].IsString()) {
                const PdfString string = array[j].GetString();
                if (!firstOnLine && currentX - lastRight > fontSize * 0.2)
                  result.text += ' ';
                result.text += string.GetStringUtf8();
                lastRight =
                    currentX + font->GetFontMetrics()->StringWidth(string) *
                                   fontSize / 1000.0;
                currentX = lastRight;
                firstOnLine = false;
              } else if (array[j].IsNumber()) {
                currentX -= array[j].GetReal() * fontSize / 1000.0;
              }
            }
          }
        }
      }
    }
    while (!result.text.empty() && result.text.back() == '\n')
      result.text.pop_back();
    RemoveEmbeddedNul(result.text);
#endif
    result.success = true;
    return result;
  } catch (const PdfError &error) {
    result.error = PdfErrorMessage(error);
  } catch (const std::exception &error) {
    result.error = std::string("Unable to extract PDF text: ") + error.what();
  }
  return result;
}

// Preserves the legacy empty-string-on-failure extraction interface.
std::string ExtractPdfText(const std::string &path) {
  return ExtractPdfTextWithResult(path).text;
}
