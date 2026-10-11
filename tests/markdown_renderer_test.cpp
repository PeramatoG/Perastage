#include "markdown.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>

void Check(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}
void Has(const std::string &html, const std::string &expected) {
  Check(html.find(expected) != html.npos, "Missing HTML: " + expected);
}
int main(int argc, char **argv) {
  try {
    const auto html = MarkdownToHtml(
        "# Manual\n## Selection & Movement\n### Export MVR\n### Export MVR\n\n"
        "A multiline\nparagraph with **bold `code`**, [link](next.md#part) and café 中文.\n\n"
        "- one\n- two\n\n1. first\n2. second\n\n"
        "```text\n<raw> & **literal**\n<!-- literal -->\n```\n\n"
        "| Key | Value |\n| --- | :---: |\n| `x\\|y\\|z` | **bold** [link](#export-mvr) |\n\n"
        "<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->\n<!-- multiple\nlines -->Visible <b> & \"text\"\n");
    for (const auto &expected : {"<h1>Manual</h1>", "<h2>Selection &amp; Movement</h2>",
          "name=\"selection--movement\"", "name=\"export-mvr\"", "name=\"export-mvr-1\"",
          "A multiline paragraph", "<b>bold <code>code</code></b>",
          "href=\"next.md#part\"", "café 中文", "<ul>", "<ol start=\"1\">", "<li>first</li>",
          "<pre><code>&lt;raw&gt; &amp; **literal**", "&lt;!-- literal --&gt;",
          "<th>Key</th>", "<td><code>x|y|z</code></td>",
          "Visible &lt;b&gt; &amp; &quot;text&quot;"}) Has(html, expected);
    Check(html.find("PERASTAGE_CONSOLE_HELP") == html.npos, "Comments must be invisible");
    Check(html.find("multiple") == html.npos, "Multiline comments must be invisible");
    Has(MarkdownToHtml("[\"quoted\"](a\"b.md)"), "href=\"a&quot;b.md\"");
    Has(MarkdownToHtml("[URL](https://example.com/a(b))"), "href=\"https://example.com/a(b)\"");
    Has(MarkdownToHtml("- **Relative group** movement continues\n  on the next line."),
        "<li><b>Relative group</b> movement continues on the next line.</li>");
    // Canonical quick-start example, not an invented simplified grammar.
    Has(MarkdownToHtml("6. [Export MVR](opening-mvr-files.md#export-mvr) for exchange, or prepare [Layouts and PDF](layouts.md) for printing and sharing."),
        "<a href=\"opening-mvr-files.md#export-mvr\">Export MVR</a>");

    Check(argc == 2, "Supply canonical docs/user directory");
    size_t pages = 0;
    const std::regex rawLink(R"(\[[^\]\n]+\]\([^\)\n]+\))");
    for (const auto &entry : std::filesystem::directory_iterator(argv[1])) {
      if (entry.path().extension() != ".md") continue;
      std::ifstream in(entry.path(), std::ios::binary);
      const std::string source((std::istreambuf_iterator<char>(in)), {});
      const auto rendered = MarkdownToHtml(source);
      const auto name = entry.path().filename().string();
      Has(rendered, "<h1>");
      Check(!std::regex_search(rendered, rawLink), name + " leaves raw Markdown links");
      Check(rendered.find("**") == rendered.npos, name + " leaves raw bold markers");
      Check(rendered.find("```") == rendered.npos, name + " leaves raw fences");
      Check(rendered.find("<!--") == rendered.npos, name + " leaves HTML comments");
      Check(rendered.find("| ---") == rendered.npos, name + " leaves table delimiters");
      // Every canonical heading becomes a heading, never a raw paragraph.
      Check(!std::regex_search(rendered, std::regex(R"(<p>#{1,3} )")), name + " leaves headings");
      Check(!std::regex_search(rendered, std::regex(R"(<p>[0-9]+\. )")), name + " leaves ordered lists");
      Check(!std::regex_search(rendered, std::regex(R"(<p>- )")), name + " leaves unordered lists");
      // Check real manual fragments against rendered target headings.
      const std::regex fragmentLink(R"(\]\(([^)#]*)#([^)]*)\))");
      for (std::sregex_iterator it(source.begin(), source.end(), fragmentLink), end;
           it != end; ++it) {
        const std::string page = (*it)[1], fragment = (*it)[2];
        if (page.starts_with("../") || page.find(":") != page.npos) continue;
        const auto target = page.empty() ? entry.path() : entry.path().parent_path() / page;
        std::ifstream targetInput(target, std::ios::binary);
        Check(static_cast<bool>(targetInput), "Missing fragment target: " + target.string());
        const std::string targetSource((std::istreambuf_iterator<char>(targetInput)), {});
        Has(MarkdownToHtml(targetSource), "name=\"" + fragment + "\"");
      }
      ++pages;
    }
    Check(pages > 0, "Canonical manual must not be empty");
    std::cout << "Renderer and " << pages << " canonical pages passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
