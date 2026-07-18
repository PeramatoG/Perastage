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
#include "gdtfnet.h"

#include "diagnostics/DiagnosticLogger.h"
#include "json.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {
const char* kLoginUrl = "https://gdtf-share.com/apis/public/login.php";
const char* kListUrl = "https://gdtf-share.com/apis/public/getList.php";
const char* kDownloadUrl = "https://gdtf-share.com/apis/public/downloadFile.php?rid=";

// Appends libcurl response bytes to an in-memory string.
size_t WriteToString(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* s = static_cast<std::string*>(userp);
    const size_t total = size * nmemb;
    s->append(static_cast<char*>(contents), total);
    return total;
}

// Writes libcurl response bytes to an output stream.
size_t WriteToFile(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* ofs = static_cast<std::ofstream*>(userp);
    ofs->write(static_cast<char*>(contents), size * nmemb);
    return ofs->good() ? size * nmemb : 0;
}

struct DownloadProgressContext {
    std::function<void(const GdtfDownloadProgress&)> progressCallback;
    std::function<bool()> shouldCancelCallback;
};

// Relays download progress and cancellation state to callers.
int ReportDownloadProgress(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                           curl_off_t, curl_off_t) {
    auto* ctx = static_cast<DownloadProgressContext*>(clientp);
    if (!ctx)
        return 0;
    if (ctx->shouldCancelCallback && ctx->shouldCancelCallback())
        return 1;
    if (ctx->progressCallback) {
        GdtfDownloadProgress progress;
        progress.downloadedBytes = static_cast<long long>(dlnow);
        progress.totalBytes = static_cast<long long>(dltotal);
        if (dltotal > 0)
            progress.percentage = static_cast<double>(dlnow) * 100.0 / static_cast<double>(dltotal);
        ctx->progressCallback(progress);
    }
    return 0;
}

// Owns or references the cookie jar path used by one GDTF Share session.
class SessionCookieJar {
public:
    // Creates a unique temporary cookie path owned by this session.
    static SessionCookieJar CreateOwnedTemporary() {
        std::random_device rd;
        std::uniform_int_distribution<unsigned long long> dist;
        for (int attempt = 0; attempt < 64; ++attempt) {
            std::ostringstream token;
            token << std::chrono::steady_clock::now().time_since_epoch().count()
                  << '_' << std::hex << dist(rd) << '_' << dist(rd) << '_' << attempt;
            fs::path candidate = fs::temp_directory_path() /
                fs::path("perastage_gdtf_cookie_" + token.str() + ".txt");
            std::ofstream reserve(candidate, std::ios::binary | std::ios::out | std::ios::app);
            if (!reserve)
                continue;
            reserve.close();
#ifndef _WIN32
            ::chmod(candidate.c_str(), S_IRUSR | S_IWUSR);
#endif
            return SessionCookieJar(std::move(candidate), true);
        }
        throw std::runtime_error("Could not create a unique GDTF Share cookie file");
    }

    // References a cookie path owned by the caller.
    static SessionCookieJar UseExternalPath(fs::path path) {
        return SessionCookieJar(std::move(path), false);
    }

    // Removes only owned temporary cookie files.
    ~SessionCookieJar() { Cleanup(); }

    SessionCookieJar(const SessionCookieJar&) = delete;
    SessionCookieJar& operator=(const SessionCookieJar&) = delete;

    // Transfers cookie-file ownership exactly once.
    SessionCookieJar(SessionCookieJar&& other) noexcept
        : path_(std::move(other.path_)), owned_(other.owned_) { other.owned_ = false; }

    // Replaces this cookie jar, cleaning any currently owned path first.
    SessionCookieJar& operator=(SessionCookieJar&& other) noexcept {
        if (this != &other) {
            Cleanup();
            path_ = std::move(other.path_);
            owned_ = other.owned_;
            other.owned_ = false;
        }
        return *this;
    }

    // Returns the cookie file path without exposing its contents.
    const fs::path& Path() const { return path_; }

    // Reports whether this object owns and will delete the cookie path.
    bool IsOwned() const { return owned_; }

    // Applies owner-only permissions when the cookie file exists.
    void HardenExistingFilePermissions() const {
#ifndef _WIN32
        std::error_code ec;
        if (fs::exists(path_, ec))
            ::chmod(path_.c_str(), S_IRUSR | S_IWUSR);
#endif
    }

private:
    // Stores the path and its ownership flag.
    SessionCookieJar(fs::path path, bool owned) : path_(std::move(path)), owned_(owned) {}

    // Removes the cookie path when this object owns it.
    void Cleanup() {
        if (!owned_ || path_.empty())
            return;
        std::error_code ec;
        fs::remove(path_, ec);
        owned_ = false;
    }

    fs::path path_;
    bool owned_ = false;
};

// Installs libcurl options shared by all GDTF Share requests.
void ConfigureCommonCurl(CURL* curl, const std::string& cookieFile,
                         std::string& response, char* errorBuffer) {
    errorBuffer[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
}

// Returns safe libcurl diagnostic text without request secrets.
std::string CurlErrorText(CURLcode code, const char* buffer) {
    if (buffer && buffer[0] != '\0')
        return SanitizeGdtfShareApiMessage(buffer, 180);
    return curl_easy_strerror(code);
}

// Stores response metadata returned by libcurl.
void FillResponseInfo(CURL* curl, GdtfShareResult& result) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.httpStatus);
    char* contentType = nullptr;
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType) == CURLE_OK && contentType)
        result.contentType = contentType;
}

// Logs a sanitized GDTF Share result for support diagnostics.
void LogResult(const std::string& operation, const GdtfShareResult& result) {
    diagnostics::DiagnosticLogger::Info(
        "GDTF Share " + operation + ": outcome=" + GdtfShareResultCategoryName(result.category) +
        " http=" + std::to_string(result.httpStatus) +
        " curl=" + std::to_string(result.transportCode) +
        " bytes=" + std::to_string(result.responseBytes) +
        " content_type=" + result.contentType +
        " elapsed_ms=" + std::to_string(result.elapsedMs));
}

// Returns true when a response looks like an API JSON error instead of a GDTF package.
bool IsJsonErrorPayload(const std::string& path, const std::string& contentType) {
    std::string lowerContentType = contentType;
    std::transform(lowerContentType.begin(), lowerContentType.end(), lowerContentType.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lowerContentType.find("json") != std::string::npos || lowerContentType.find("text/") != std::string::npos)
        return true;
    std::ifstream in(path, std::ios::binary);
    char first = 0;
    while (in.get(first)) {
        if (!std::isspace(static_cast<unsigned char>(first)))
            return first == '{' || first == '[';
    }
    return false;
}

// Returns true when a downloaded file starts with a ZIP-compatible signature.
bool HasZipSignature(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::array<unsigned char, 4> sig{};
    in.read(reinterpret_cast<char*>(sig.data()), static_cast<std::streamsize>(sig.size()));
    if (in.gcount() < 4)
        return false;
    return sig[0] == 0x50 && sig[1] == 0x4b &&
           ((sig[2] == 0x03 && sig[3] == 0x04) ||
            (sig[2] == 0x05 && sig[3] == 0x06) ||
            (sig[2] == 0x07 && sig[3] == 0x08));
}

// Returns a temporary download path next to the final destination.
fs::path MakeSiblingPath(const fs::path& dest, const std::string& tag) {
    std::random_device rd;
    std::uniform_int_distribution<unsigned long long> dist;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const fs::path candidate = dest.parent_path() / (dest.filename().string() + tag +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
            std::to_string(dist(rd)) + "_" + std::to_string(attempt));
        std::error_code ec;
        if (!fs::exists(candidate, ec))
            return candidate;
    }
    return dest.parent_path() / (dest.filename().string() + tag +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_fallback");
}

// Returns a temporary download path next to the final destination.
fs::path MakePartialPath(const fs::path& dest) { return MakeSiblingPath(dest, ".part."); }

// Publishes a validated sibling file while preserving any previous destination.
bool PublishDownloadedFile(const fs::path& source, const fs::path& dest, std::string& message) {
    std::error_code ec;
    const std::uintmax_t expectedSize = fs::file_size(source, ec);
    if (ec) { message = "Could not inspect staged download"; return false; }
    fs::path publishSource = source;
    fs::path staged;
    fs::path backup;
    auto cleanupStaged = [&]() { std::error_code ignore; if (!staged.empty()) fs::remove(staged, ignore); };
    const bool existed = fs::exists(dest, ec);
    if (existed) {
        backup = MakeSiblingPath(dest, ".backup.");
        fs::rename(dest, backup, ec);
        if (ec) { message = "Could not create backup for existing download"; cleanupStaged(); return false; }
    }
    fs::rename(publishSource, dest, ec);
    if (ec) {
        staged = MakeSiblingPath(dest, ".stage.");
        ec.clear();
        fs::copy_file(source, staged, fs::copy_options::none, ec);
        if (!ec && fs::file_size(staged, ec) == expectedSize) {
            ec.clear(); fs::rename(staged, dest, ec);
        }
    }
    if (ec || !fs::exists(dest, ec) || fs::file_size(dest, ec) != expectedSize) {
        std::error_code removeDestEc;
        fs::remove(dest, removeDestEc);
        std::error_code restoreEc;
        if (!backup.empty())
            fs::rename(backup, dest, restoreEc);
        cleanupStaged();
        if (restoreEc) {
            message = "Could not publish download; rollback failed and backup was retained: " + backup.filename().string();
        } else {
            message = "Could not publish download; previous file was restored";
            backup.clear();
        }
        return false;
    }
    if (!backup.empty()) { std::error_code removeEc; fs::remove(backup, removeEc); }
    std::error_code removeEc; fs::remove(source, removeEc); cleanupStaged();
    return true;
}
} // namespace

// Builds the JSON body for a GDTF Share login request.
std::string BuildGdtfLoginRequestBody(const std::string& user,
                                      const std::string& password) {
    const nlohmann::json payload = {{"user", user}, {"password", password}};
    return payload.dump();
}

// Sanitizes API diagnostic text before it is displayed or logged.
std::string SanitizeGdtfShareApiMessage(const std::string& message, size_t maxLength) {
    std::string out;
    for (unsigned char ch : message) {
        if (ch < 0x20 || ch == 0x7f)
            out += ' ';
        else
            out += static_cast<char>(ch);
        if (out.size() >= maxLength)
            break;
    }
    if (message.size() > maxLength)
        out += "...";
    return out;
}

// Masks a configured username for diagnostics without revealing the full value.
std::string MaskGdtfShareUsernameForDiagnostics(const std::string& username) {
    const auto at = username.find('@');
    if (at != std::string::npos) {
        const std::string local = username.substr(0, at);
        const std::string domain = username.substr(at + 1);
        return local.substr(0, std::min<size_t>(2, local.size())) + "***@***" +
               (domain.size() > 3 ? domain.substr(domain.size() - 3) : domain);
    }
    if (username.size() <= 2)
        return "**";
    return username.substr(0, 2) + "***" + username.substr(username.size() - 1);
}

// Converts a GDTF Share result category to stable diagnostic text.
std::string GdtfShareResultCategoryName(GdtfShareResultCategory category) {
    switch (category) {
    case GdtfShareResultCategory::Success: return "Success";
    case GdtfShareResultCategory::TransportError: return "TransportError";
    case GdtfShareResultCategory::Timeout: return "Timeout";
    case GdtfShareResultCategory::HttpError: return "HttpError";
    case GdtfShareResultCategory::AuthenticationRejected: return "AuthenticationRejected";
    case GdtfShareResultCategory::ApiRejected: return "ApiRejected";
    case GdtfShareResultCategory::InvalidJsonResponse: return "InvalidJsonResponse";
    case GdtfShareResultCategory::InvalidResponseSchema: return "InvalidResponseSchema";
    case GdtfShareResultCategory::Cancelled: return "Cancelled";
    case GdtfShareResultCategory::LocalFileError: return "LocalFileError";
    }
    return "Unknown";
}

// Formats a concise user-facing GDTF Share failure message.
std::string FormatGdtfShareUserMessage(const GdtfShareResult& result, const std::string& operation) {
    switch (result.category) {
    case GdtfShareResultCategory::Success:
        return "GDTF Share " + operation + " completed.";
    case GdtfShareResultCategory::TransportError:
        return "Could not reach GDTF Share: " + result.transportMessage + ".";
    case GdtfShareResultCategory::Timeout:
        return "GDTF Share " + operation + " timed out.";
    case GdtfShareResultCategory::AuthenticationRejected:
        return "The GDTF Share username or password is invalid (HTTP " + std::to_string(result.httpStatus) + ").";
    case GdtfShareResultCategory::ApiRejected:
        return "GDTF Share rejected the " + operation + " request" +
               (result.httpStatus ? " (HTTP " + std::to_string(result.httpStatus) + ")" : "") + ".";
    case GdtfShareResultCategory::HttpError:
        return "GDTF Share returned HTTP " + std::to_string(result.httpStatus) + " for " + operation + ".";
    case GdtfShareResultCategory::InvalidJsonResponse:
    case GdtfShareResultCategory::InvalidResponseSchema:
        return "GDTF Share returned an invalid response. See the diagnostic log for details.";
    case GdtfShareResultCategory::Cancelled:
        return "GDTF Share " + operation + " was cancelled.";
    case GdtfShareResultCategory::LocalFileError:
        return "Could not save the GDTF Share download locally.";
    }
    return "GDTF Share " + operation + " failed.";
}

// Maps HTTP, transport, and API response details to a structured result.
GdtfShareResult MapGdtfShareResponse(long httpStatus, int transportCode,
                                     const std::string& transportMessage,
                                     const std::string& response,
                                     const std::string& contentType,
                                     long long elapsedMs,
                                     bool requireResultField) {
    GdtfShareResult result;
    result.httpStatus = httpStatus;
    result.transportCode = transportCode;
    result.transportMessage = transportMessage;
    result.contentType = contentType;
    result.responseBytes = response.size();
    result.elapsedMs = elapsedMs;
    if (transportCode != CURLE_OK)
        return transportCode == CURLE_OPERATION_TIMEDOUT ? MakeGdtfShareTimeoutResult(transportCode, transportMessage, elapsedMs)
                                                        : MakeGdtfShareTransportResult(transportCode, transportMessage, elapsedMs);
    const nlohmann::json root = nlohmann::json::parse(response, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        if (httpStatus >= 200 && httpStatus < 300 && !requireResultField) {
            result.category = GdtfShareResultCategory::Success;
            result.payload = response;
            return result;
        }
        result.category = GdtfShareResultCategory::InvalidJsonResponse;
        return result;
    }
    if (root.contains("error") && root["error"].is_string())
        result.apiMessage = SanitizeGdtfShareApiMessage(root["error"].get<std::string>());
    else if (root.contains("notice") && root["notice"].is_string())
        result.apiMessage = SanitizeGdtfShareApiMessage(root["notice"].get<std::string>());
    auto it = root.find("result");
    if (it == root.end() || !it->is_boolean()) {
        result.category = (httpStatus >= 200 && httpStatus < 300 && !requireResultField) ?
                          GdtfShareResultCategory::Success : GdtfShareResultCategory::InvalidResponseSchema;
        if (result.category == GdtfShareResultCategory::Success)
            result.payload = response;
        return result;
    }
    if (httpStatus == 200 && it->get<bool>()) {
        result.category = GdtfShareResultCategory::Success;
        result.payload = response;
        return result;
    }
    if (httpStatus == 401 || httpStatus == 403)
        result.category = GdtfShareResultCategory::AuthenticationRejected;
    else if (httpStatus >= 400 && httpStatus < 500)
        result.category = GdtfShareResultCategory::ApiRejected;
    else if (httpStatus >= 500)
        result.category = GdtfShareResultCategory::HttpError;
    else
        result.category = GdtfShareResultCategory::ApiRejected;
    return result;
}

// Creates a structured transport failure result.
GdtfShareResult MakeGdtfShareTransportResult(int transportCode, const std::string& transportMessage,
                                             long long elapsedMs) {
    GdtfShareResult result;
    result.category = GdtfShareResultCategory::TransportError;
    result.transportCode = transportCode;
    result.transportMessage = transportMessage;
    result.elapsedMs = elapsedMs;
    return result;
}

// Creates a structured timeout failure result.
GdtfShareResult MakeGdtfShareTimeoutResult(int transportCode, const std::string& transportMessage,
                                           long long elapsedMs) {
    GdtfShareResult result;
    result.category = GdtfShareResultCategory::Timeout;
    result.transportCode = transportCode;
    result.transportMessage = transportMessage;
    result.elapsedMs = elapsedMs;
    return result;
}

struct GdtfShareClient::Impl {
    SessionCookieJar cookie = SessionCookieJar::CreateOwnedTemporary();
    std::optional<std::string> authenticatedUsername;
};

// Creates a GDTF Share client with an isolated temporary cookie jar.
GdtfShareClient::GdtfShareClient() : impl(std::make_unique<Impl>()) {}

// Creates a GDTF Share client with a caller-provided cookie jar.
GdtfShareClient::GdtfShareClient(const std::string& cookieFile) : impl(std::make_unique<Impl>()) { impl->cookie = SessionCookieJar::UseExternalPath(cookieFile); }

// Cleans up the temporary GDTF Share session cookie.
GdtfShareClient::~GdtfShareClient() = default;

// Moves a GDTF Share client and its session ownership.
GdtfShareClient::GdtfShareClient(GdtfShareClient&&) noexcept = default;

// Move-assigns a GDTF Share client and its session ownership.
GdtfShareClient& GdtfShareClient::operator=(GdtfShareClient&&) noexcept = default;

// Resets the cookie jar and clears authentication state.
void GdtfShareClient::ResetSession() {
    impl->cookie = SessionCookieJar::CreateOwnedTemporary();
    impl->authenticatedUsername.reset();
}

// Reports whether the current session has authenticated successfully.
bool GdtfShareClient::IsAuthenticated() const { return impl->authenticatedUsername.has_value(); }

// Returns the authenticated username for the current session when known.
std::optional<std::string> GdtfShareClient::AuthenticatedUsername() const { return impl->authenticatedUsername; }

// Returns the session cookie path for tests without exposing cookie contents.
const fs::path& GdtfShareClient::CookiePathForTesting() const { return impl->cookie.Path(); }

// Reports whether this client owns the session cookie for tests.
bool GdtfShareClient::OwnsCookieForTesting() const { return impl->cookie.IsOwned(); }

// Authenticates against GDTF Share and stores the session cookie internally.
GdtfShareResult GdtfShareClient::Login(const std::string& user, const std::string& password) {
    CURL* curl = curl_easy_init();
    if (!curl)
        return MakeGdtfShareTransportResult(CURLE_FAILED_INIT, "Could not initialize libcurl", 0);
    std::string response;
    char errorBuffer[CURL_ERROR_SIZE];
    const std::string cookiePath = impl->cookie.Path().string();
    ConfigureCommonCurl(curl, cookiePath, response, errorBuffer);
    const std::string body = BuildGdtfLoginRequestBody(user, password);
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, kLoginUrl);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    const auto start = std::chrono::steady_clock::now();
    const CURLcode code = curl_easy_perform(curl);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    GdtfShareResult result = MapGdtfShareResponse(0, code, CurlErrorText(code, errorBuffer), response, "", elapsed, true);
    FillResponseInfo(curl, result);
    if (code == CURLE_OK)
        result = MapGdtfShareResponse(result.httpStatus, code, "", response, result.contentType, elapsed, true);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    if (result.Succeeded())
        impl->authenticatedUsername = user;
    else if (result.category == GdtfShareResultCategory::AuthenticationRejected)
        impl->authenticatedUsername.reset();
    impl->cookie.HardenExistingFilePermissions();
    LogResult("login", result);
    return result;
}

// Retrieves the authenticated GDTF Share catalog through this session.
GdtfShareResult GdtfShareClient::GetCatalog() {
    CURL* curl = curl_easy_init();
    if (!curl)
        return MakeGdtfShareTransportResult(CURLE_FAILED_INIT, "Could not initialize libcurl", 0);
    std::string response;
    char errorBuffer[CURL_ERROR_SIZE];
    const std::string cookiePath = impl->cookie.Path().string();
    ConfigureCommonCurl(curl, cookiePath, response, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_URL, kListUrl);
    const auto start = std::chrono::steady_clock::now();
    const CURLcode code = curl_easy_perform(curl);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    GdtfShareResult result = MapGdtfShareResponse(0, code, CurlErrorText(code, errorBuffer), response, "", elapsed, true);
    FillResponseInfo(curl, result);
    if (code == CURLE_OK)
        result = MapGdtfShareResponse(result.httpStatus, code, "", response, result.contentType, elapsed, true);
    curl_easy_cleanup(curl);
    if (result.category == GdtfShareResultCategory::AuthenticationRejected)
        impl->authenticatedUsername.reset();
    impl->cookie.HardenExistingFilePermissions();
    LogResult("catalog", result);
    return result;
}

// Downloads a GDTF revision safely through this session.
GdtfShareResult GdtfShareClient::DownloadRevision(const std::string& rid, const std::string& destFile,
    std::function<void(const GdtfDownloadProgress&)> progressCallback,
    std::function<bool()> shouldCancelCallback) {
    const fs::path dest(destFile);
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    const fs::path partial = MakePartialPath(dest);
    std::ofstream ofs(partial, std::ios::binary);
    if (!ofs.is_open()) {
        GdtfShareResult result;
        result.category = GdtfShareResultCategory::LocalFileError;
        result.transportMessage = "Could not open temporary download file";
        return result;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        ofs.close(); fs::remove(partial, ec);
        return MakeGdtfShareTransportResult(CURLE_FAILED_INIT, "Could not initialize libcurl", 0);
    }
    std::string responseProbe;
    char errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = '\0';
    const std::string url = std::string(kDownloadUrl) + rid;
    const std::string cookiePath = impl->cookie.Path().string();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiePath.c_str());
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 512L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);
    DownloadProgressContext ctx{std::move(progressCallback), std::move(shouldCancelCallback)};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ReportDownloadProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    const auto start = std::chrono::steady_clock::now();
    const CURLcode code = curl_easy_perform(curl);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    ofs.close();
    GdtfShareResult result;
    result.elapsedMs = elapsed;
    result.transportCode = code;
    result.transportMessage = CurlErrorText(code, errorBuffer);
    FillResponseInfo(curl, result);
    curl_easy_cleanup(curl);
    result.responseBytes = fs::exists(partial, ec) ? static_cast<size_t>(fs::file_size(partial, ec)) : 0;
    const bool cancelled = code == CURLE_ABORTED_BY_CALLBACK;
    if (cancelled) result.category = GdtfShareResultCategory::Cancelled;
    else if (code == CURLE_OPERATION_TIMEDOUT) result.category = GdtfShareResultCategory::Timeout;
    else if (code != CURLE_OK) result.category = GdtfShareResultCategory::TransportError;
    else if (result.httpStatus == 401 || result.httpStatus == 403) result.category = GdtfShareResultCategory::AuthenticationRejected;
    else if (result.httpStatus < 200 || result.httpStatus >= 300) result.category = result.httpStatus >= 500 ? GdtfShareResultCategory::HttpError : GdtfShareResultCategory::ApiRejected;
    else if (result.responseBytes == 0 || IsJsonErrorPayload(partial.string(), result.contentType) || !HasZipSignature(partial)) { result.category = GdtfShareResultCategory::ApiRejected; result.transportMessage = "Downloaded payload is not a valid GDTF ZIP archive"; }
    else {
        std::string publishMessage;
        if (!PublishDownloadedFile(partial, dest, publishMessage)) {
            result.category = GdtfShareResultCategory::LocalFileError;
            result.transportMessage = publishMessage;
        } else {
            result.category = GdtfShareResultCategory::Success;
        }
        if (result.category == GdtfShareResultCategory::AuthenticationRejected)
            impl->authenticatedUsername.reset();
        impl->cookie.HardenExistingFilePermissions();
        LogResult("download", result);
        return result;
    }
    if (result.category == GdtfShareResultCategory::AuthenticationRejected)
        impl->authenticatedUsername.reset();
    fs::remove(partial, ec);
    impl->cookie.HardenExistingFilePermissions();
    LogResult("download", result);
    return result;
}

// Legacy wrapper for authenticating with a caller-provided cookie file.
bool GdtfLogin(const std::string& user, const std::string& password,
               const std::string& cookieFile, long& httpCode) {
    GdtfShareClient client(cookieFile);
    const GdtfShareResult result = client.Login(user, password);
    httpCode = result.httpStatus;
    return result.Succeeded();
}

// Legacy wrapper for fetching the catalog with a caller-provided cookie file.
bool GdtfGetList(const std::string& cookieFile, std::string& listData, long* httpCode) {
    GdtfShareClient client(cookieFile);
    const GdtfShareResult result = client.GetCatalog();
    if (httpCode) *httpCode = result.httpStatus;
    listData = result.payload;
    return result.Succeeded();
}

// Legacy wrapper for downloading a revision with a caller-provided cookie file.
bool GdtfDownload(const std::string& rid, const std::string& destFile,
                  const std::string& cookieFile, long& httpCode,
                  std::function<void(const GdtfDownloadProgress&)> progressCallback,
                  std::function<bool()> shouldCancelCallback) {
    GdtfShareClient client(cookieFile);
    const GdtfShareResult result = client.DownloadRevision(rid, destFile, std::move(progressCallback), std::move(shouldCancelCallback));
    httpCode = result.httpStatus;
    return result.Succeeded();
}
