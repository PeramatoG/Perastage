#include "system_fonts.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite.h>
#include <wrl/client.h>
#elif defined(__APPLE__)
#include <CoreText/CoreText.h>
#else
#include <fontconfig/fontconfig.h>
#endif
#include <algorithm>
#include <memory>
#include <set>
#include <tuple>

namespace typography {
namespace {
#ifdef _WIN32
using Microsoft::WRL::ComPtr;

std::string Utf8(const std::wstring &value) {
  if (value.empty()) return {};
  const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                       static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  std::string bytes(size, '\0');
  if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                               static_cast<int>(value.size()), bytes.data(), size, nullptr, nullptr);
  return bytes;
}

std::string Name(IDWriteLocalizedStrings *strings) {
  if (!strings || !strings->GetCount()) return {};
  UINT32 index = 0, size = 0;
  BOOL exists = false;
  strings->FindLocaleName(L"en-us", &index, &exists);
  if (!exists) index = 0;
  if (FAILED(strings->GetStringLength(index, &size)) || size > 8192) return {};
  std::wstring value(size + 1, L'\0');
  if (FAILED(strings->GetString(index, value.data(), size + 1))) return {};
  value.resize(size);
  return Utf8(value);
}

std::string Info(IDWriteFont &font, DWRITE_INFORMATIONAL_STRING_ID field) {
  ComPtr<IDWriteLocalizedStrings> names;
  BOOL present = false;
  return SUCCEEDED(font.GetInformationalStrings(field, &names, &present)) && present ?
      Name(names.Get()) : std::string{};
}

std::filesystem::path Path(IDWriteFontFace &face) {
  UINT32 count = 0;
  if (FAILED(face.GetFiles(&count, nullptr)) || count != 1) return {};
  ComPtr<IDWriteFontFile> file;
  if (FAILED(face.GetFiles(&count, file.GetAddressOf()))) return {};
  ComPtr<IDWriteFontFileLoader> loader;
  ComPtr<IDWriteLocalFontFileLoader> local;
  if (FAILED(file->GetLoader(&loader)) || FAILED(loader.As(&local))) return {};
  const void *key = nullptr;
  UINT32 keySize = 0, size = 0;
  if (FAILED(file->GetReferenceKey(&key, &keySize)) ||
      FAILED(local->GetFilePathLengthFromKey(key, keySize, &size)) || size > 32768) return {};
  std::wstring path(size + 1, L'\0');
  if (FAILED(local->GetFilePathFromKey(key, keySize, path.data(), size + 1))) return {};
  path.resize(size);
  return path;
}

void Enumerate(std::vector<SystemFontRecord> &records) {
  ComPtr<IDWriteFactory> factory;
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
          reinterpret_cast<IUnknown **>(factory.GetAddressOf())))) return;
  ComPtr<IDWriteFontCollection> fonts;
  if (FAILED(factory->GetSystemFontCollection(&fonts, TRUE))) return;
  for (UINT32 i = 0; i < fonts->GetFontFamilyCount() && records.size() < 16384; ++i) {
    ComPtr<IDWriteFontFamily> family;
    ComPtr<IDWriteLocalizedStrings> familyNames;
    if (FAILED(fonts->GetFontFamily(i, &family)) || FAILED(family->GetFamilyNames(&familyNames))) continue;
    const auto name = Name(familyNames.Get());
    for (UINT32 j = 0; j < family->GetFontCount() && records.size() < 16384; ++j) {
      ComPtr<IDWriteFont> font;
      ComPtr<IDWriteFontFace> face;
      if (FAILED(family->GetFont(j, &font)) || font->GetSimulations() ||
          FAILED(font->CreateFontFace(&face))) continue;
      auto path = Path(*face.Get());
      if (path.empty()) continue;
      ComPtr<IDWriteLocalizedStrings> faceNames;
      font->GetFaceNames(&faceNames);
      auto preferred = Info(*font.Get(), DWRITE_INFORMATIONAL_STRING_PREFERRED_FAMILY_NAMES);
      records.push_back({std::move(path), face->GetIndex(), preferred.empty() ? name : std::move(preferred),
          Name(faceNames.Get()), Info(*font.Get(), DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME),
          static_cast<int>(font->GetWeight()), static_cast<int>(font->GetStretch()),
          font->GetStyle() != DWRITE_FONT_STYLE_NORMAL});
    }
  }
}
#elif defined(__APPLE__)
template<typename Ref> auto Owned(Ref ref) {
  return std::unique_ptr<std::remove_pointer_t<Ref>, decltype(&CFRelease)>(ref, CFRelease);
}

std::string Utf8(CFStringRef value) {
  if (!value) return {};
  const auto size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8);
  if (size < 0 || size > 32768) return {};
  std::string bytes(size + 1, '\0');
  if (!CFStringGetCString(value, bytes.data(), bytes.size(), kCFStringEncodingUTF8)) return {};
  bytes.resize(std::char_traits<char>::length(bytes.data()));
  return bytes;
}

std::string Name(CTFontDescriptorRef descriptor, CFStringRef key) {
  const auto value = Owned(CTFontDescriptorCopyAttribute(descriptor, key));
  if (!value || CFGetTypeID(value.get()) != CFStringGetTypeID()) return {};
  return Utf8(static_cast<CFStringRef>(value.get()));
}

void Enumerate(std::vector<SystemFontRecord> &records) {
  const auto collection = Owned(CTFontCollectionCreateFromAvailableFonts(nullptr));
  const auto fonts = collection ? Owned(CTFontCollectionCreateMatchingFontDescriptors(collection.get()))
                                : Owned(static_cast<CFArrayRef>(nullptr));
  if (!fonts) return;
  for (CFIndex i = 0; i < CFArrayGetCount(fonts.get()) && records.size() < 16384; ++i) {
    auto descriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fonts.get(), i));
    const auto url = Owned(CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute));
    if (!url || CFGetTypeID(url.get()) != CFURLGetTypeID()) continue;
    const auto path = Owned(CFURLCopyFileSystemPath(static_cast<CFURLRef>(url.get()), kCFURLPOSIXPathStyle));
    auto name = Name(descriptor, kCTFontFamilyNameAttribute);
    auto ps = Name(descriptor, kCTFontNameAttribute);
    if (name.empty() || ps.empty()) continue;
    const auto traits = Owned(CTFontDescriptorCopyAttribute(descriptor, kCTFontTraitsAttribute));
    int symbolic = 0;
    if (traits && CFGetTypeID(traits.get()) == CFDictionaryGetTypeID()) {
      const auto value = static_cast<CFNumberRef>(CFDictionaryGetValue(
          static_cast<CFDictionaryRef>(traits.get()), kCTFontSymbolicTrait));
      if (value && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(value, kCFNumberIntType, &symbolic);
    }
    records.push_back({std::filesystem::u8path(Utf8(path.get())), kFindFaceByPostScriptName,
        std::move(name), Name(descriptor, kCTFontStyleNameAttribute), std::move(ps),
        symbolic & kCTFontBoldTrait ? 700 : 400, 5, (symbolic & kCTFontItalicTrait) != 0});
  }
}
#else
std::string Name(FcPattern *font, const char *key) {
  FcChar8 *value = nullptr;
  return FcPatternGetString(font, key, 0, &value) == FcResultMatch ?
      reinterpret_cast<const char *>(value) : "";
}

int Number(FcPattern *font, const char *key, int fallback) {
  int value = 0;
  return FcPatternGetInteger(font, key, 0, &value) == FcResultMatch ? value : fallback;
}

// Fontconfig caches may retain the same directory timestamp across consecutive
// installs and removals. Force the provider to rebuild its registered directories
// only when the user requests a refresh.
using DirectoryCache = std::unique_ptr<FcCache, decltype(&FcDirCacheUnload)>;
std::vector<DirectoryCache> RefreshDirectories(FcConfig *config) {
  const std::unique_ptr<FcStrList, decltype(&FcStrListDone)> dirs(FcConfigGetFontDirs(config), FcStrListDone);
  std::vector<std::string> pending;
  if (dirs) {
    while (const auto *dir = FcStrListNext(dirs.get()))
      pending.emplace_back(reinterpret_cast<const char *>(dir));
  }
  std::vector<DirectoryCache> caches;
  std::set<std::filesystem::path> visited;
  constexpr size_t maxDirectories = 8192;
  for (size_t i = 0; i < pending.size() && i < maxDirectories; ++i) {
    const auto requested = pending[i];
    std::error_code error;
    const auto canonical = std::filesystem::canonical(requested, error);
    if (error || !visited.insert(canonical).second) continue;
    DirectoryCache cache(FcDirCacheRead(reinterpret_cast<const FcChar8 *>(requested.c_str()),
                                        FcTrue, config), FcDirCacheUnload);
    if (!cache) continue;
    for (int j = 0; j < FcCacheNumSubdir(cache.get()) && pending.size() < maxDirectories; ++j) {
      if (const auto *child = FcCacheSubdir(cache.get(), j))
        pending.emplace_back(reinterpret_cast<const char *>(child));
    }
    caches.push_back(std::move(cache));
  }
  return caches;
}

void Enumerate(std::vector<SystemFontRecord> &records, bool refresh) {
  const std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)> config(
      refresh ? FcInitLoadConfig() : FcInitLoadConfigAndFonts(), FcConfigDestroy);
  if (!config) return;
  const auto refreshed = refresh ? RefreshDirectories(config.get()) : std::vector<DirectoryCache>{};
  if (refresh && !FcConfigBuildFonts(config.get())) return;
  const std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> query(FcPatternCreate(), FcPatternDestroy);
  const std::unique_ptr<FcObjectSet, decltype(&FcObjectSetDestroy)> fields(
      FcObjectSetBuild(FC_FILE, FC_INDEX, FC_FAMILY, FC_STYLE, FC_POSTSCRIPT_NAME,
                       FC_WEIGHT, FC_SLANT, FC_WIDTH, static_cast<const char *>(nullptr)), FcObjectSetDestroy);
  if (!query || !fields) return;
  const std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fonts(
      FcFontList(config.get(), query.get(), fields.get()), FcFontSetDestroy);
  if (!fonts) return;
  for (int i = 0; i < fonts->nfont && records.size() < 16384; ++i) {
    auto *font = fonts->fonts[i];
    const auto index = Number(font, FC_INDEX, 0);
    if (index < 0 || index > 0xffff) continue; // Fontconfig's high bits encode named variation instances.
    records.push_back({Name(font, FC_FILE), static_cast<unsigned>(index), Name(font, FC_FAMILY),
        Name(font, FC_STYLE), Name(font, FC_POSTSCRIPT_NAME),
        FcWeightToOpenType(Number(font, FC_WEIGHT, FC_WEIGHT_REGULAR)),
        std::clamp((Number(font, FC_WIDTH, FC_WIDTH_NORMAL) + 6) / 25 + 1, 1, 9), Number(font, FC_SLANT, FC_SLANT_ROMAN) != FC_SLANT_ROMAN});
  }
}
#endif
}

std::vector<SystemFontRecord> DiscoverSystemFonts(bool refresh) {
  std::vector<SystemFontRecord> records;
#if defined(_WIN32) || defined(__APPLE__)
  (void)refresh;  // Native registration APIs provide a current snapshot.
  Enumerate(records);
#else
  Enumerate(records, refresh);
#endif
  std::erase_if(records, [](const auto &record) { return record.path.empty() || record.family.empty(); });
  std::stable_sort(records.begin(), records.end(), [](const auto &a, const auto &b) {
    return std::tie(a.family, a.italic, a.weight, a.width, a.postScriptName, a.path, a.index) <
           std::tie(b.family, b.italic, b.weight, b.width, b.postScriptName, b.path, b.index);
  });
  records.erase(std::unique(records.begin(), records.end()), records.end());
  return records;
}
}
