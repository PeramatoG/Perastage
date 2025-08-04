#include "markdown.h"

#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

namespace {
// Replace markdown bold markers with HTML tags.
std::string ProcessBold(const std::string &text) {
    std::string result = text;
    bool open = false;
    size_t pos;
    while ((pos = result.find("**")) != std::string::npos) {
        result.replace(pos, 2, open ? "</b>" : "<b>");
        open = !open;
    }
    return result;
}

// Trim leading and trailing whitespace from a string
std::string Trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    return s.substr(start, end - start);
}

// Split a markdown table row into individual cells
std::vector<std::string> SplitTableRow(const std::string &line) {
    std::vector<std::string> cells;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, '|')) {
        cells.push_back(Trim(cell));
    }
    if (!cells.empty() && cells.front().empty())
        cells.erase(cells.begin());
    if (!cells.empty() && cells.back().empty())
        cells.pop_back();
    return cells;
}

// Check if a row is a markdown table separator (--- or :---: etc)
bool IsSeparatorRow(const std::vector<std::string> &cells) {
    if (cells.empty())
        return false;
    for (const auto &c : cells) {
        if (c.empty())
            return false;
        if (c.find_first_not_of("-: ") != std::string::npos)
            return false;
    }
    return true;
}
} // namespace

std::string MarkdownToHtml(const std::string &markdown) {
    std::istringstream in(markdown);
    std::ostringstream out;
    std::string line;
    bool inList = false;
    bool inTable = false;
    bool headerParsed = false;
    std::vector<std::string> tableHeaders;
    std::vector<std::vector<std::string>> tableRows;

    auto flushTable = [&]() {
        if (!inTable)
            return;
        out << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" style=\"border-collapse:collapse;\">\n";
        if (!tableHeaders.empty()) {
            out << "<tr>";
            for (const auto &h : tableHeaders)
                out << "<th style=\"border:1px solid #ccc;padding:4px;\">" << ProcessBold(h) << "</th>";
            out << "</tr>\n";
        }
        for (const auto &row : tableRows) {
            out << "<tr>";
            for (const auto &c : row)
                out << "<td style=\"border:1px solid #ccc;padding:4px;\">" << ProcessBold(c) << "</td>";
            out << "</tr>\n";
        }
        out << "</table>\n";
        tableHeaders.clear();
        tableRows.clear();
        inTable = false;
        headerParsed = false;
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.front() == '|' && line.find('|', 1) != std::string::npos) {
            auto cells = SplitTableRow(line);
            if (!cells.empty()) {
                if (inList) { out << "</ul>\n"; inList = false; }
                if (!inTable) {
                    inTable = true;
                    tableHeaders = cells;
                } else if (!headerParsed && IsSeparatorRow(cells)) {
                    headerParsed = true;
                } else {
                    tableRows.push_back(cells);
                }
                continue;
            }
        }

        if (inTable)
            flushTable();

        if (line.rfind("### ", 0) == 0) {
            if (inList) { out << "</ul>\n"; inList = false; }
            out << "<h3>" << ProcessBold(line.substr(4)) << "</h3>\n";
        } else if (line.rfind("## ", 0) == 0) {
            if (inList) { out << "</ul>\n"; inList = false; }
            out << "<h2>" << ProcessBold(line.substr(3)) << "</h2>\n";
        } else if (line.rfind("# ", 0) == 0) {
            if (inList) { out << "</ul>\n"; inList = false; }
            out << "<h1>" << ProcessBold(line.substr(2)) << "</h1>\n";
        } else if (line.rfind("- ", 0) == 0) {
            if (!inList) { out << "<ul>\n"; inList = true; }
            out << "<li>" << ProcessBold(line.substr(2)) << "</li>\n";
        } else if (line.empty()) {
            if (inList) { out << "</ul>\n"; inList = false; }
        } else {
            if (inList) { out << "</ul>\n"; inList = false; }
            out << "<p>" << ProcessBold(line) << "</p>\n";
        }
    }
    if (inTable)
        flushTable();
    if (inList) { out << "</ul>\n"; }
    return out.str();
}
