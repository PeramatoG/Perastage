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

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

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

// Removes a temporary file when the session object leaves scope.
class ScopedTempCookieFile {
public:
    explicit ScopedTempCookieFile(const std::string& requestedPath = {}) {
        if (!requestedPath.empty()) {
            path = requestedPath;
            return;
        }
        const fs::path temp = fs::temp_directory_path() /
            fs::path("perastage_gdtf_cookie_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".txt");
        path = temp.string();
    }
    ~ScopedTempCookieFile() { std::error_code ec; fs::remove(path, ec); }
    std::string path;
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
    if (contentType.find("json") != std::string::npos)
        return true;
    std::ifstream in(path, std::ios::binary);
    char first = 0;
    while (in.get(first)) {
        if (!std::isspace(static_cast<unsigned char>(first)))
            return first == '{' || first == '[';
    }
    return false;
}

// Returns a temporary download path next to the final destination.
fs::path MakePartialPath(const fs::path& dest) {
    return dest.parent_path() / (dest.filename().string() + ".part." +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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
    ScopedTempCookieFile cookie;
};

// Creates a GDTF Share client with an isolated temporary cookie jar.
GdtfShareClient::GdtfShareClient() : impl(std::make_unique<Impl>()) {}

// Creates a GDTF Share client with a caller-provided cookie jar.
GdtfShareClient::GdtfShareClient(const std::string& cookieFile) : impl(std::make_unique<Impl>()) { impl->cookie.path = cookieFile; }

// Cleans up the temporary GDTF Share session cookie.
GdtfShareClient::~GdtfShareClient() = default;

// Authenticates against GDTF Share and stores the session cookie internally.
GdtfShareResult GdtfShareClient::Login(const std::string& user, const std::string& password) {
    CURL* curl = curl_easy_init();
    if (!curl)
        return MakeGdtfShareTransportResult(CURLE_FAILED_INIT, "Could not initialize libcurl", 0);
    std::string response;
    char errorBuffer[CURL_ERROR_SIZE];
    ConfigureCommonCurl(curl, impl->cookie.path, response, errorBuffer);
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
    ConfigureCommonCurl(curl, impl->cookie.path, response, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_URL, kListUrl);
    const auto start = std::chrono::steady_clock::now();
    const CURLcode code = curl_easy_perform(curl);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    GdtfShareResult result = MapGdtfShareResponse(0, code, CurlErrorText(code, errorBuffer), response, "", elapsed, true);
    FillResponseInfo(curl, result);
    if (code == CURLE_OK)
        result = MapGdtfShareResponse(result.httpStatus, code, "", response, result.contentType, elapsed, true);
    curl_easy_cleanup(curl);
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
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, impl->cookie.path.c_str());
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
    else if (result.responseBytes == 0 || IsJsonErrorPayload(partial.string(), result.contentType)) result.category = GdtfShareResultCategory::ApiRejected;
    else {
        fs::rename(partial, dest, ec);
        if (ec) { fs::copy_file(partial, dest, fs::copy_options::overwrite_existing, ec); fs::remove(partial, ec); }
        result.category = ec ? GdtfShareResultCategory::LocalFileError : GdtfShareResultCategory::Success;
        LogResult("download", result);
        return result;
    }
    fs::remove(partial, ec);
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
