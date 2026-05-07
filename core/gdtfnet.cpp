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
#include <curl/curl.h>
#include <fstream>
#include <utility>
#include "json.hpp"

namespace {
size_t WriteToString(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* s = static_cast<std::string*>(userp);
    size_t total = size * nmemb;
    s->append(static_cast<char*>(contents), total);
    return total;
}


bool ParseResultFlag(const std::string& response, bool& resultValue) {
    nlohmann::json root = nlohmann::json::parse(response, nullptr, false);
    if (root.is_discarded() || !root.is_object())
        return false;
    auto it = root.find("result");
    if (it == root.end() || !it->is_boolean())
        return false;
    resultValue = it->get<bool>();
    return true;
}
}

// Authenticates a GDTF Share user and persists the session cookie for follow-up requests.
bool GdtfLogin(const std::string& user,
               const std::string& password,
               const std::string& cookieFile,
               long& httpCode)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    std::string jsonData = "{\"user\":\"" + user + "\",\"password\":\"" + password + "\"}";
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://gdtf-share.com/apis/public/login.php");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return false;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    bool apiResult = false;
    if (!ParseResultFlag(response, apiResult))
        return httpCode == 200;
    return httpCode == 200 && apiResult;
}

// Fetches the authenticated GDTF revision catalog using the provided session cookie file.
bool GdtfGetList(const std::string& cookieFile,
                 std::string& listData,
                 long* httpCode)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    curl_easy_setopt(curl, CURLOPT_URL, "https://gdtf-share.com/apis/public/getList.php");
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &listData);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return false;
    }

    if (httpCode)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, httpCode);

    curl_easy_cleanup(curl);

    bool apiResult = false;
    if (!ParseResultFlag(listData, apiResult))
        return httpCode ? (*httpCode == 200) : true;
    return (httpCode ? (*httpCode == 200) : true) && apiResult;
}

namespace {
size_t WriteToFile(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* ofs = static_cast<std::ofstream*>(userp);
    ofs->write(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

struct DownloadProgressContext {
    std::function<void(const GdtfDownloadProgress&)> progressCallback;
    std::function<bool()> shouldCancelCallback;
};

int ReportDownloadProgress(void* clientp,
                           curl_off_t dltotal,
                           curl_off_t dlnow,
                           curl_off_t /*ultotal*/,
                           curl_off_t /*ulnow*/) {
    DownloadProgressContext* ctx = static_cast<DownloadProgressContext*>(clientp);
    if (!ctx)
        return 0;

    if (ctx->shouldCancelCallback && ctx->shouldCancelCallback()) {
        return 1;
    }

    if (ctx->progressCallback) {
        GdtfDownloadProgress progress;
        progress.downloadedBytes = static_cast<long long>(dlnow);
        progress.totalBytes = static_cast<long long>(dltotal);
        if (dltotal > 0) {
            progress.percentage = (static_cast<double>(dlnow) * 100.0) /
                                  static_cast<double>(dltotal);
        }
        ctx->progressCallback(progress);
    }
    return 0;
}
}

bool GdtfDownload(const std::string& rid,
                  const std::string& destFile,
                  const std::string& cookieFile,
                  long& httpCode,
                  std::function<void(const GdtfDownloadProgress&)> progressCallback,
                  std::function<bool()> shouldCancelCallback)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    httpCode = 0;
    std::string url = "https://gdtf-share.com/apis/public/downloadFile.php?rid=" + rid;
    std::ofstream ofs(destFile, std::ios::binary);
    if (!ofs.is_open()) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookieFile.c_str());
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);
    DownloadProgressContext progressContext{
        std::move(progressCallback),
        std::move(shouldCancelCallback)};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ReportDownloadProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressContext);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (res != CURLE_OK) {
        ofs.close();
        curl_easy_cleanup(curl);
        return false;
    }

    ofs.close();
    curl_easy_cleanup(curl);
    return true;
}
