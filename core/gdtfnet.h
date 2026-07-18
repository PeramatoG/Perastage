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
#ifndef GDTFNET_H
#define GDTFNET_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>

struct GdtfDownloadProgress {
    long long downloadedBytes = 0;
    long long totalBytes = 0;
    double percentage = 0.0;
};

enum class GdtfShareResultCategory {
    Success,
    TransportError,
    Timeout,
    HttpError,
    AuthenticationRejected,
    ApiRejected,
    InvalidJsonResponse,
    InvalidResponseSchema,
    Cancelled,
    LocalFileError
};

struct GdtfShareResult {
    GdtfShareResultCategory category = GdtfShareResultCategory::TransportError;
    long httpStatus = 0;
    int transportCode = 0;
    std::string transportMessage;
    std::string apiMessage;
    std::string contentType;
    size_t responseBytes = 0;
    long long elapsedMs = 0;
    std::string payload;

    bool Succeeded() const { return category == GdtfShareResultCategory::Success; }
};

std::string BuildGdtfLoginRequestBody(const std::string& user,
                                      const std::string& password);
std::string SanitizeGdtfShareApiMessage(const std::string& message,
                                        size_t maxLength = 240);
std::string MaskGdtfShareUsernameForDiagnostics(const std::string& username);
std::string GdtfShareResultCategoryName(GdtfShareResultCategory category);
std::string FormatGdtfShareUserMessage(const GdtfShareResult& result,
                                       const std::string& operation);
GdtfShareResult MapGdtfShareResponse(long httpStatus,
                                     int transportCode,
                                     const std::string& transportMessage,
                                     const std::string& response,
                                     const std::string& contentType,
                                     long long elapsedMs,
                                     bool requireResultField = true);
GdtfShareResult MakeGdtfShareTransportResult(int transportCode,
                                             const std::string& transportMessage,
                                             long long elapsedMs);
GdtfShareResult MakeGdtfShareTimeoutResult(int transportCode,
                                           const std::string& transportMessage,
                                           long long elapsedMs);

class GdtfShareClient {
public:
    GdtfShareClient();
    explicit GdtfShareClient(const std::string& cookieFile);
    ~GdtfShareClient();

    GdtfShareClient(const GdtfShareClient&) = delete;
    GdtfShareClient& operator=(const GdtfShareClient&) = delete;

    GdtfShareResult Login(const std::string& user, const std::string& password);
    GdtfShareResult GetCatalog();
    GdtfShareResult DownloadRevision(
        const std::string& rid,
        const std::string& destFile,
        std::function<void(const GdtfDownloadProgress&)> progressCallback = {},
        std::function<bool()> shouldCancelCallback = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

bool GdtfLogin(const std::string& user,
               const std::string& password,
               const std::string& cookieFile,
               long& httpCode);
bool GdtfGetList(const std::string& cookieFile,
                 std::string& listData,
                 long* httpCode = nullptr);
bool GdtfDownload(const std::string& rid,
                  const std::string& destFile,
                  const std::string& cookieFile,
                  long& httpCode,
                  std::function<void(const GdtfDownloadProgress&)> progressCallback = {},
                  std::function<bool()> shouldCancelCallback = {});

#endif // GDTFNET_H
