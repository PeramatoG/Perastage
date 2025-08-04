#include "markdown.h"

#include <sstream>

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
} // namespace

std::string MarkdownToHtml(const std::string &markdown) {
    std::istringstream in(markdown);
    std::ostringstream out;
    std::string line;
    bool inList = false;

    while (std::getline(in, line)) {
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
    if (inList) { out << "</ul>\n"; }
    return out.str();
}
