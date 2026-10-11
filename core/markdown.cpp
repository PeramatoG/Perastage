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
#include "markdown.h"

#include <string_view>
#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <vector>

namespace {
std::string Escape(std::string_view text) {
  std::string out;
  for (char c : text) {
    switch (c) {
    case '&': out += "&amp;"; break;
    case '<': out += "&lt;"; break;
    case '>': out += "&gt;"; break;
    case '"': out += "&quot;"; break;
    case '\'': out += "&#39;"; break;
    default: out += c;
    }
  }
  return out;
}

std::string Trim(std::string s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == s.npos) return {};
  return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

// A deliberately bounded inline grammar, not a full CommonMark parser.
std::string Inline(std::string_view text, unsigned depth = 0, bool plain = false) {
  if (depth > 16) return plain ? std::string(text) : Escape(text);
  std::string out;
  for (size_t i = 0; i < text.size();) {
    if (text[i] == '\\' && i + 1 < text.size()) {
      out += plain ? std::string(text.substr(i + 1, 1)) : Escape(text.substr(i + 1, 1)); i += 2; continue;
    }
    if (text[i] == '`') {
      const auto end = text.find('`', i + 1);
      if (end != text.npos) {
        out += plain ? std::string(text.substr(i + 1, end - i - 1))
                     : "<code>" + Escape(text.substr(i + 1, end - i - 1)) + "</code>";
        i = end + 1; continue;
      }
    }
    if (text.substr(i, 2) == "**") {
      const auto end = text.find("**", i + 2);
      if (end != text.npos) {
        const auto content = Inline(text.substr(i + 2, end - i - 2), depth + 1, plain);
        out += plain ? content : "<b>" + content + "</b>";
        i = end + 2; continue;
      }
    }
    if (text[i] == '[') {
      const auto labelEnd = text.find("](", i + 1);
      if (labelEnd != text.npos) {
        // Balance parentheses in targets such as repository URLs.
        size_t end = labelEnd + 2;
        unsigned balance = 1;
        for (; end < text.size(); ++end) {
          if (text[end] == '(') ++balance;
          if (text[end] == ')' && --balance == 0) break;
        }
        if (end < text.size()) {
          const auto label = Inline(text.substr(i + 1, labelEnd - i - 1), depth + 1, plain);
          const auto target = text.substr(labelEnd + 2, end - labelEnd - 2);
          out += plain ? label + " (" + std::string(target) + ")"
                       : "<a href=\"" + Escape(target) + "\">" + label + "</a>";
          i = end + 1; continue;
        }
      }
    }
    out += plain ? std::string(text.substr(i++, 1)) : Escape(text.substr(i++, 1));
  }
  return out;
}

std::string Anchor(std::string_view text) {
  std::string out;
  for (unsigned char c : text) {
    if (c >= 128 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-')
      out += static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    else if (c == ' ' || c == '\t') out += '-';
  }
  return out;
}

std::vector<std::string> Cells(const std::string &line) {
  std::vector<std::string> cells;
  std::string cell;
  bool code = false;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '\\' && i + 1 < line.size()) {
      // Tables escape delimiter pipes even inside inline code spans.
      if (line[i + 1] != '|') cell += line[i];
      cell += line[++i]; continue;
    }
    if (line[i] == '`') code = !code;
    if (line[i] == '|' && !code) { cells.push_back(Trim(cell)); cell.clear(); }
    else cell += line[i];
  }
  cells.push_back(Trim(cell));
  if (!cells.empty() && cells.front().empty()) cells.erase(cells.begin());
  if (!cells.empty() && cells.back().empty()) cells.pop_back();
  return cells;
}

bool Separator(const std::string &line) {
  const auto cells = Cells(line);
  return !cells.empty() && std::all_of(cells.begin(), cells.end(), [](const auto &c) {
    return c.find('-') != c.npos && c.find_first_not_of("-: ") == c.npos;
  });
}

struct ListItem { std::string type; size_t start = 0; std::string number; };
ListItem List(const std::string &line) {
  if (line.starts_with("- ") || line.starts_with("* ")) return {"ul", 2, {}};
  const auto pos = line.find_first_not_of("0123456789");
  if (pos > 0 && pos != line.npos && line.substr(pos, 2) == ". ")
    return {"ol", pos + 2, line.substr(0, pos)};
  return {};
}
} // namespace

std::string MarkdownToHtml(const std::string &markdown) {
  std::vector<std::string> lines;
  std::istringstream input(markdown);
  std::string line;
  bool comment = false, fence = false;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // Comments are invisible outside literal fenced examples, even across lines.
    if (Trim(line).starts_with("```") && !comment) fence = !fence;
    if (!fence && !Trim(line).starts_with("```")) {
      std::string visible;
      size_t pos = 0;
      while (pos < line.size()) {
        const auto marker = line.find(comment ? "-->" : "<!--", pos);
        if (!comment) visible += line.substr(pos, marker == line.npos ? marker : marker - pos);
        if (marker == line.npos) break;
        pos = marker + (comment ? 3 : 4); comment = !comment;
      }
      line = visible;
    }
    lines.push_back(line);
  }

  std::ostringstream out;
  std::string paragraph, listType;
  bool itemOpen = false, inCode = false;
  std::map<std::string, unsigned> anchors;
  auto flushParagraph = [&] {
    if (!paragraph.empty()) { out << "<p>" << Inline(paragraph) << "</p>\n"; paragraph.clear(); }
  };
  auto closeList = [&] {
    if (!listType.empty()) {
      if (itemOpen) out << "</li>\n";
      out << "</" << listType << ">\n";
      listType.clear(); itemOpen = false;
    }
  };
  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string text = Trim(lines[i]);
    if (text.starts_with("```")) {
      flushParagraph(); closeList();
      out << (inCode ? "</code></pre>\n" : "<pre><code>");
      inCode = !inCode; continue;
    }
    if (inCode) { out << Escape(lines[i]) << '\n'; continue; }
    if (text.empty()) { flushParagraph(); continue; }
    size_t heading = text.find_first_not_of('#');
    if (heading >= 1 && heading <= 3 && text[heading] == ' ') {
      flushParagraph(); closeList();
      const std::string title = text.substr(heading + 1);
      std::string id = Anchor(title);
      const unsigned occurrence = anchors[id]++;
      if (occurrence) id += "-" + std::to_string(occurrence);
      // wxHtmlWindow supports named anchors; id also serves other consumers.
      out << "<a name=\"" << Escape(id) << "\" id=\"" << Escape(id) << "\"></a><h"
          << heading << ">" << Inline(title) << "</h" << heading << ">\n";
      continue;
    }
    if (text.starts_with('|') && i + 1 < lines.size() && Separator(Trim(lines[i + 1]))) {
      flushParagraph(); closeList();
      out << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\">\n";
      auto row = [&](const std::string &source, const char *tag) {
        out << "<tr>";
        for (const auto &cell : Cells(source)) out << '<' << tag << '>' << Inline(cell) << "</" << tag << '>';
        out << "</tr>\n";
      };
      row(text, "th"); ++i;
      while (i + 1 < lines.size() && Trim(lines[i + 1]).starts_with('|')) row(Trim(lines[++i]), "td");
      out << "</table>\n"; continue;
    }
    const auto item = List(text);
    if (!item.type.empty()) {
      flushParagraph();
      if (listType != item.type) {
        closeList(); listType = item.type;
        out << '<' << listType;
        if (listType == "ol") out << " start=\"" << item.number << "\"";
        out << ">\n";
      }
      if (itemOpen) out << "</li>\n";
      out << "<li>" << Inline(text.substr(item.start)); itemOpen = true; continue;
    }
    if (!listType.empty() && lines[i].starts_with("  ")) {
      out << ' ' << Inline(text); continue;
    }
    closeList();
    if (!paragraph.empty()) paragraph += ' ';
    paragraph += text;
  }
  flushParagraph(); closeList();
  if (inCode) out << "</code></pre>\n";
  return out.str();
}

std::string MarkdownToPlainText(const std::string &markdown) {
  std::istringstream input(markdown);
  std::ostringstream out;
  std::string line;
  bool fence = false, comment = false, table = false;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!comment && Trim(line).starts_with("```")) {
      fence = !fence;
      table = false;
      continue;
    }
    // Literal command examples bypass inline formatting and preserve whitespace.
    if (fence) { out << line << '\n'; continue; }
    std::string visible;
    size_t pos = 0;
    while (pos < line.size()) {
      const auto marker = line.find(comment ? "-->" : "<!--", pos);
      if (!comment) visible += line.substr(pos, marker == line.npos ? marker : marker - pos);
      if (marker == line.npos) break;
      pos = marker + (comment ? 3 : 4);
      comment = !comment;
    }
    std::string text = Trim(visible);
    if (text.starts_with('|')) {
      const auto cells = Cells(text);
      if (table && Separator(text)) continue;
      table = true;
      for (size_t i = 0; i < cells.size(); ++i) {
        if (i) out << "  |  ";
        out << Inline(cells[i], 0, true);
      }
      out << '\n';
      continue;
    }
    table = false;
    const auto heading = text.find_first_not_of('#');
    if (heading >= 1 && heading <= 6 && heading < text.size() && text[heading] == ' ')
      text = text.substr(heading + 1);
    const auto item = List(text);
    if (!item.type.empty()) {
      out << (item.type == "ul" ? "• " : item.number + ". ");
      text = text.substr(item.start);
    }
    out << Inline(text, 0, true) << '\n';
  }
  if (fence || comment) return {};
  const auto text = out.str();
  if (text.find_first_not_of(" \t\r\n") == text.npos) return {};
  const auto first = text.find_first_not_of('\n');
  return text.substr(first, text.find_last_not_of('\n') - first + 1);
}
