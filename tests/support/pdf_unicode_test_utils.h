#pragma once

#include <cassert>
#include <cstdint>
#include <map>
#include <regex>
#include <sstream>
#include <string>

namespace pdf_test {
inline std::string Object(const std::string &pdf, const std::string &id) {
  const auto marker = "\n" + id + " 0 obj\n";
  const auto pos = pdf.find(marker);
  assert(pos != std::string::npos);
  const auto start = pos + marker.size();
  // Font and page dictionaries and ToUnicode streams contain only ASCII.
  return pdf.substr(start, pdf.find("\nendobj", start) - start);
}

inline std::string Reference(const std::string &dictionary, const std::string &name) {
  const auto pos = dictionary.find("/" + name + " ");
  assert(pos != std::string::npos);
  return dictionary.substr(pos + name.size() + 2,
      dictionary.find(' ', pos + name.size() + 2) - pos - name.size() - 2);
}

inline std::string Utf8(std::string hex) {
  uint32_t cp = std::stoul(hex.substr(0, 4), nullptr, 16);
  if (hex.size() == 8)
    cp = 0x10000 + ((cp - 0xd800) << 10) + std::stoul(hex.substr(4), nullptr, 16) - 0xdc00;
  if (cp < 0x80) return {static_cast<char>(cp)};
  if (cp < 0x800)
    return {static_cast<char>(0xc0 | (cp >> 6)), static_cast<char>(0x80 | (cp & 63))};
  std::string text;
  if (cp >= 0x10000) text += static_cast<char>(0xf0 | (cp >> 18));
  text += static_cast<char>((cp < 0x10000 ? 0xe0 : 0x80) | ((cp >> 12) & 63));
  text += static_cast<char>(0x80 | ((cp >> 6) & 63));
  text += static_cast<char>(0x80 | (cp & 63));
  return text;
}

inline std::string PageStream(const std::string &pdf, bool decode = false) {
  const auto pagePos = pdf.find("/Type /Page /Parent");
  assert(pagePos != std::string::npos);
  const auto page = pdf.substr(pagePos, pdf.find("endobj", pagePos) - pagePos);
  const auto raw = Object(pdf, Reference(page, "Contents"));
  const auto start = raw.find("stream\n") + 7;
  const auto stream = raw.substr(start, raw.rfind("endstream") - start);
  if (!decode) return stream;

  struct Font {
    std::string name;
    std::map<std::string, std::string> codepoints;
  };
  std::map<std::string, Font> fonts;
  const std::regex resource(R"(/(F[0-9]+) ([0-9]+) 0 R)");
  const std::regex mapping(R"(<([0-9A-F]{4})> <([0-9A-F]{4,8})>)");
  for (std::sregex_iterator it(page.begin(), page.end(), resource), end; it != end; ++it) {
    auto &font = fonts[(*it)[1]];
    const auto dictionary = Object(pdf, (*it)[2]);
    font.name = Reference(dictionary, "BaseFont");
    const auto cmap = Object(pdf, Reference(dictionary, "ToUnicode"));
    for (std::sregex_iterator code(cmap.begin(), cmap.end(), mapping); code != end; ++code)
      font.codepoints[(*code)[1]] = Utf8((*code)[2]);
  }
  const Font *font = nullptr;
  std::istringstream lines(stream);
  std::ostringstream normalized;
  std::string line;
  while (std::getline(lines, line)) {
    if (line.starts_with("/F") && line.ends_with(" Tf")) {
      const auto space = line.find(' ');
      font = &fonts.at(line.substr(1, space - 1));
      line = font->name + line.substr(space);
    } else if (line.size() == 9 && line.front() == '<' && line.ends_with("> Tj")) {
      assert(font);
      line = "glyph " + font->codepoints.at(line.substr(1, 4));
    }
    normalized << line << '\n';
  }
  return normalized.str();
}

inline std::string Text(const std::string &normalized) {
  std::istringstream lines(normalized);
  std::string text, line;
  while (std::getline(lines, line))
    if (line.starts_with("glyph ")) text += line.substr(6);
  return text;
}
}
