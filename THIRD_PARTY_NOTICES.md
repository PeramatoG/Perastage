# Third-Party Notices

Perastage is licensed separately under the GNU General Public License version 3 or later. This document records third-party software and assets that are committed to the repository or used directly at runtime. The corresponding legal texts are distributed in the [`licenses/`](licenses) directory.

## Source and assets committed to this repository

| Component | License | Notice |
|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | [License](licenses/nlohmann_json_LICENSE.txt) |
| [stb_easy_font](https://github.com/nothings/stb) | MIT or public domain | [License](licenses/stb_easy_font_LICENSE.txt) |
| [Noto Sans](https://fonts.google.com/noto/specimen/Noto+Sans) / bundled Perastage font asset | SIL Open Font License 1.1 | [License](licenses/noto_sans_LICENSE.txt) |
| [Lucide icons 0.562.0](https://github.com/lucide-icons/lucide/tree/0.562.0) | ISC for Lucide portions; MIT for Feather-derived portions | [Combined upstream license](licenses/lucide_LICENSE.txt) |

## Direct runtime libraries

The versions resolved by a package manager can vary for system-package builds. Official vcpkg builds use the repository's pinned baseline and also stage the package-generated copyright files described below.

| Component | License | Notice |
|---|---|---|
| [wxWidgets](https://www.wxwidgets.org/) | LGPL-2.0-or-later WITH WxWindows-exception-3.1 | [License](licenses/wxwidgets_LICENSE.txt) |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | Zlib | [License](licenses/tinyxml2_LICENSE.txt) |
| [libcurl](https://curl.se/libcurl/) | curl license, with an additional ISC notice in the pinned vcpkg package | [Primary license](licenses/curl_LICENSE.txt); [inet_ntop notice](licenses/curl_inet_ntop_NOTICE.txt) |
| [GLEW](https://github.com/nigels-com/glew) | BSD-3-Clause, MIT, and SGI-B-2.0 combined notices | [Combined upstream notices](licenses/glew_LICENSE.txt) |
| [zlib](https://www.zlib.net/) | Zlib | [License](licenses/zlib_LICENSE.txt) |
| [NanoVG](https://github.com/memononen/nanovg) | Zlib (zlib-style) | [License](licenses/nanovg_LICENSE.txt) |
| [PoDoFo](https://github.com/podofo/podofo) | LGPL-2.0-or-later OR MPL-2.0 | [LGPL terms](licenses/podofo_COPYING.LGPL.txt); [MPL terms](licenses/podofo_COPYING.MPL.txt) |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | MIT | [License](licenses/meshoptimizer_LICENSE.txt) |
| [backward-cpp](https://github.com/bombela/backward-cpp) | MIT | [License](licenses/backward-cpp_LICENSE.txt) |
| [mdns](https://github.com/mjansson/mdns) | Unlicense | [License](licenses/mdns_LICENSE.txt) |

## Package-manager and platform dependencies

Official builds made with vcpkg copy the available `share/*/copyright` metadata into `licenses/vcpkg/` in the staged application. Those generated files supplement this curated notice set and can include notices for transitive dependencies and build helpers. System and platform packages may supply additional notices through their own package metadata.

This curated document is not an exhaustive inventory of every operating-system library that a downstream packager may choose to bundle. Distributors remain responsible for preserving notices required by the exact dependency set they redistribute.
