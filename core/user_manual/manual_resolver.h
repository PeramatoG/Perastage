#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace user_manual {
inline constexpr const char *kOnlineDocumentation =
    "https://perastage.luismaperamato.com/documentation.html";

struct Page {
  std::string name;
  std::string fragment;
  std::string locale;
  std::string markdown;
  bool englishFallback = false;
};

enum class LinkKind { Manual, Fragment, Online, Rejected };
struct Link {
  LinkKind kind = LinkKind::Rejected;
  std::string target;
  std::string fragment;
};

// All paths and contents at this boundary are UTF-8. root is the packaged help/.
class ManualResolver {
public:
  ManualResolver(std::filesystem::path root, std::string_view requestedLocale);
  std::optional<Page> Load(std::string_view page = "index.md",
                           std::string_view fragment = {}) const;
  Link ResolveLink(std::string_view currentPage, std::string_view href) const;
  const std::string &RequestedLocale() const { return locale_; }

private:
  std::filesystem::path root_;
  std::string locale_;
};
} // namespace user_manual
