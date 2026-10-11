#include "user_manual/manual_resolver.h"
#include "filesystem_path_utils.h"
#include "utf8_utils.h"
#include "localization/app_language.h"

#include <utility>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace user_manual {
namespace {
namespace fs = std::filesystem;

bool IsRelative(std::string_view path) {
  return !path.empty() && IsValidUtf8(path) && path.front() != '/' &&
         path.find_first_of("\\:?\0", 0, 4) == std::string_view::npos &&
         std::none_of(path.begin(), path.end(), [](unsigned char c) { return c < 32 || c == 127; });
}

bool IsContained(const fs::path &path) {
  return !path.empty() && !path.is_absolute() &&
         std::none_of(path.begin(), path.end(), [](const auto &part) {
           return part == "..";
         });
}

std::optional<std::string> Decode(std::string_view text) {
  std::string out;
  auto hex = [](unsigned char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = text[i];
    if (c == '%') {
      if (i + 2 >= text.size()) return {};
      const int a = hex(text[i + 1]), b = hex(text[i + 2]);
      if (a < 0 || b < 0) return {};
      c = static_cast<unsigned char>(a * 16 + b);
      i += 2;
    }
    if (c < 32 || c == 127) return {};
    out += static_cast<char>(c);
  }
  if (!IsValidUtf8(out)) return {};
  return out;
}

std::string EncodePath(std::string_view path) {
  const char *hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : path) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '/')
      out += static_cast<char>(c);
    else {
      out += '%'; out += hex[c >> 4]; out += hex[c & 15];
    }
  }
  return out;
}
} // namespace

ManualResolver::ManualResolver(fs::path root, std::string_view requestedLocale)
    : root_(std::move(root)), locale_(localization::AppLanguageCode(
          localization::ParseAppLanguageCode(requestedLocale))) {}

std::optional<Page> ManualResolver::Load(std::string_view page,
                                        std::string_view fragment) const {
  if (root_.empty() || !IsRelative(page)) return {};
  const fs::path name = PathUtils::PathFromUtf8(std::string(page)).lexically_normal();
  if (!IsContained(name) || name.extension() != ".md") return {};
  for (const std::string &locale : {locale_, std::string("en")}) {
    std::error_code ec;
    const fs::path root = fs::weakly_canonical(root_, ec);
    if (ec) continue;
    const fs::path localeRoot = fs::weakly_canonical(root / locale, ec);
    if (ec || !IsContained(localeRoot.lexically_relative(root))) continue;
    const fs::path candidate = fs::weakly_canonical(localeRoot / name, ec);
    if (ec || !IsContained(candidate.lexically_relative(localeRoot)) ||
        !fs::is_regular_file(candidate, ec) || ec) continue;
    std::ifstream in(candidate, std::ios::binary);
    if (!in) continue;
    std::string markdown((std::istreambuf_iterator<char>(in)), {});
    if (in.bad() || !IsValidUtf8(markdown)) continue;
    if (markdown.starts_with("\xEF\xBB\xBF")) markdown.erase(0, 3);
    return Page{PathUtils::PathToUtf8(name), std::string(fragment), locale,
                std::move(markdown), locale != locale_};
  }
  return {};
}

Link ManualResolver::ResolveLink(std::string_view currentPage,
                                 std::string_view href) const {
  // HTTP(S) is the only external scheme. Never pass file: or script: to a GUI.
  std::string lower(href);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lower.starts_with("http://") || lower.starts_with("https://"))
    return {LinkKind::Online, std::string(href), {}};
  const auto hash = href.find('#');
  const auto decoded = Decode(href.substr(0, hash));
  const auto fragment = Decode(hash == href.npos ? std::string_view{} : href.substr(hash + 1));
  if (!decoded || !fragment) return {};
  if (decoded->empty()) return {LinkKind::Fragment, std::string(currentPage), *fragment};
  if (!IsRelative(*decoded) || !IsRelative(currentPage)) return {};
  const fs::path current = PathUtils::PathFromUtf8(std::string(currentPage)).lexically_normal();
  if (!IsContained(current)) return {};
  const fs::path target = (current.parent_path() / PathUtils::PathFromUtf8(*decoded)).lexically_normal();
  if (IsContained(target) && target.extension() == ".md")
    return {LinkKind::Manual, PathUtils::PathToUtf8(target), *fragment};

  // Canonical docs/user links may leave user/ for developer/reference docs.
  // Construct an online repository URL only within these known docs scopes.
  const fs::path docsTarget = (fs::path("user") / target).lexically_normal();
  if (IsContained(docsTarget) && docsTarget.extension() == ".md" &&
      (*docsTarget.begin() == "developer" || *docsTarget.begin() == "reference")) {
    std::string url = "https://github.com/PeramatoG/Perastage/blob/main/docs/" +
                      EncodePath(PathUtils::PathToUtf8(docsTarget));
    if (!fragment->empty()) url += "#" + EncodePath(*fragment);
    return {LinkKind::Online, url, {}};
  }
  return {};
}
} // namespace user_manual
