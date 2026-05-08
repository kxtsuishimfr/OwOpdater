// pasted from Lunox, my other app

#include "owopdater/utils/http.h"
#include "owopdater/utils/generic.h"
// ================== 
#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <regex>
#include <cstring>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <map>
#include <algorithm>


#pragma comment(lib, "winhttp.lib")

static bool ParseFullUrl(const std::string& fullUrl, std::string& out_scheme, std::string& out_host, std::string& out_port, std::string& out_path)
{
    std::regex re(R"(^([a-zA-Z][a-zA-Z0-9+\-.]*):\/\/([^:\/\s]+)(?::(\d+))?(\/.*)?$)");
    std::smatch m;
    if (!std::regex_match(fullUrl, m, re)) return false;

    out_scheme = m[1].str();
    out_host = m[2].str();
    out_port = m[3].matched ? m[3].str() : (out_scheme == "https" ? "443" : "80");
    out_path = m[4].matched ? m[4].str() : "/";
    return true;
}

static std::string WstringToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return std::string();
    int len = ::WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), NULL, 0, NULL, NULL);
    if (len == 0) return std::string();
    std::string s;
    s.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &s[0], len, NULL, NULL);
    return s;
}

static std::wstring Utf8ToWstring(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int len = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len == 0) return std::wstring();
    std::wstring w;
    w.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

static std::string WinErrorToString(DWORD err)
{
    if (err == 0) return std::string();
    LPSTR buf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);
    std::string s;
    if (buf) {
        s = buf;
        LocalFree(buf);
    }
    return s;
}

static HttpResponse PerformWinHttp(const std::string& fullUrl, const std::string& method, const std::string& body, const std::map<std::string, std::string>& headers)
{
    const int maxAttempts = 3;
    std::string last_reason;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        HttpResponse resp;

        std::string scheme, host, portStr, path;
        if (!ParseFullUrl(fullUrl, scheme, host, portStr, path)) {
            resp.reason = "invalid url";
            return resp;
        }

        DWORD port = 0;
        try { port = static_cast<DWORD>(std::stoul(portStr)); }
        catch (...) { resp.reason = "invalid port"; return resp; }
        bool secure = (_stricmp(scheme.c_str(), "https") == 0);

        std::wstring whost = Utf8ToWstring(host);
        std::wstring wpath = Utf8ToWstring(path);
        std::wstring wmethod = Utf8ToWstring(method);

        DWORD accessType = (attempt == 0) ? WINHTTP_ACCESS_TYPE_DEFAULT_PROXY : WINHTTP_ACCESS_TYPE_NO_PROXY;
        LPCWSTR proxyName = WINHTTP_NO_PROXY_NAME;
        LPCWSTR proxyBypass = WINHTTP_NO_PROXY_BYPASS;
        HINTERNET hSession = WinHttpOpen(L"Lunox/1.0",
            accessType,
            proxyName,
            proxyBypass,
            0);
        if (!hSession) {
            DWORD err = GetLastError();
            resp.reason = "WinHttpOpen failed: " + WinErrorToString(err);
            DebugLog(resp.reason);
            last_reason = resp.reason;
            if (attempt + 1 < maxAttempts) Sleep(100);
            continue;
        }

        WinHttpSetTimeouts(hSession, 2000, 2000, 2000, 2000);
        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

        HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), static_cast<INTERNET_PORT>(port), 0);
        if (!hConnect) {
            DWORD err = GetLastError();
            resp.reason = "WinHttpConnect failed: " + WinErrorToString(err);
            DebugLog(resp.reason + " url=" + fullUrl);
            last_reason = resp.reason;
            WinHttpCloseHandle(hSession);
            if (attempt + 1 < maxAttempts) Sleep(100);
            continue;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect,
            wmethod.c_str(),
            wpath.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            secure ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) {
            DWORD err = GetLastError();
            resp.reason = "WinHttpOpenRequest failed: " + WinErrorToString(err);
            DebugLog(resp.reason + " url=" + fullUrl);
            last_reason = resp.reason;
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (attempt + 1 < maxAttempts) Sleep(100);
            continue;
        }
        std::wstring defaultUA = L"User-Agent: Lunox/1.0";
        std::wstring defaultAccept = L"Accept: application/json";

        WinHttpAddRequestHeaders(hRequest, defaultUA.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_REPLACE);
        WinHttpAddRequestHeaders(hRequest, defaultAccept.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_REPLACE);

        bool has_content_type = false;
        for (const auto& h : headers) {
            std::string key = h.first;
            std::string val = h.second;
            if (_stricmp(key.c_str(), "content-type") == 0) has_content_type = true;
            std::wstring wkey = Utf8ToWstring(key);
            std::wstring wval = Utf8ToWstring(val);
            std::wstring wline = wkey + L": " + wval;

            WinHttpAddRequestHeaders(hRequest, wline.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_REPLACE);
        }
        if (!body.empty() && !has_content_type) {
            std::wstring ct = L"Content-Type: application/json";
            WinHttpAddRequestHeaders(hRequest, ct.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        BOOL ok = FALSE;
        DWORD sendErr = 0;

        if (body.empty()) {
            ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (!ok) sendErr = GetLastError();
        }
        else {

            ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
            if (!ok) {
                sendErr = GetLastError();


                BOOL initOk = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, (DWORD)body.size(), 0);
                if (initOk) {
                    DWORD written = 0;
                    if (WinHttpWriteData(hRequest, (LPVOID)body.c_str(), (DWORD)body.size(), &written) && written == (DWORD)body.size()) {
                        ok = TRUE;
                    }
                    else {
                        sendErr = GetLastError();
                    }
                }
                else {
                    sendErr = GetLastError();
                }
            }
        }

        if (!ok) {
            resp.reason = std::string("WinHttpSendRequest failed (code ") + std::to_string((unsigned long)sendErr) + ": " + WinErrorToString(sendErr);
            DebugLog(resp.reason + " url=" + fullUrl);
            last_reason = resp.reason;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (attempt + 1 < maxAttempts) Sleep(200);
            continue;
        }

        ok = WinHttpReceiveResponse(hRequest, NULL);
        if (!ok) {
            DWORD err = GetLastError();
            resp.reason = "WinHttpReceiveResponse failed: " + WinErrorToString(err);
            DebugLog(resp.reason + " url=" + fullUrl);
            last_reason = resp.reason;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (attempt + 1 < maxAttempts) Sleep(100);
            continue;
        }

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
            resp.status_code = (int)status;
        }

        DWORD reasonLen = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &reasonLen, WINHTTP_NO_HEADER_INDEX);
        if (reasonLen > 0) {
            std::vector<wchar_t> reasonBuf(reasonLen + 1);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, reasonBuf.data(), &reasonLen, WINHTTP_NO_HEADER_INDEX)) {
                std::wstring wreason(reasonBuf.data());
                resp.reason = WstringToUtf8(wreason);
            }
        }

        DWORD headerLen = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &headerLen, WINHTTP_NO_HEADER_INDEX);
        if (headerLen > 0) {
            std::vector<char> headerBuf(headerLen + 1);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, headerBuf.data(), &headerLen, WINHTTP_NO_HEADER_INDEX)) {
                std::string raw(headerBuf.data(), headerLen);
                size_t pos = 0;
                while (pos < raw.size()) {
                    size_t end = raw.find("\r\n", pos);
                    if (end == std::string::npos) end = raw.size();
                    std::string line = raw.substr(pos, end - pos);
                    size_t colon = line.find(':');
                    if (colon != std::string::npos) {
                        std::string k = line.substr(0, colon);
                        std::string v = line.substr(colon + 1);
                        while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
                        size_t first = v.find_first_not_of(" \t");
                        if (first != std::string::npos) v = v.substr(first);
                        resp.headers[k] = v;
                    }
                    if (end == raw.size()) break;
                    pos = end + 2;
                }
            }
        }

        std::string responseBody;
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            std::vector<char> buffer(dwSize + 1);
            ZeroMemory(buffer.data(), buffer.size());
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded) && dwDownloaded > 0) {
                responseBody.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);

        resp.body = responseBody;

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return resp;
    }

    HttpResponse finalResp;
    finalResp.reason = last_reason;
    return finalResp;
}

HttpResponse HttpGetFull(const std::string& fullUrl)
{
    return PerformWinHttp(fullUrl, "GET", "", {});
}

HttpResponse HttpPostJsonFull(const std::string& fullUrl, const std::string& jsonBody)
{
    return PerformWinHttp(fullUrl, "POST", jsonBody, {});
}

HttpResponse HttpRequestFull(const std::string& fullUrl, const std::string& method, const std::string& body, const std::map<std::string, std::string>& headers)
{
    return PerformWinHttp(fullUrl, method, body, headers);
}

std::string HttpGet(const std::string& fullUrl)
{
    auto resp = HttpGetFull(fullUrl);
    return resp.body;
}

std::string HttpPostJson(const std::string& fullUrl, const std::string& jsonBody)
{
    auto resp = HttpPostJsonFull(fullUrl, jsonBody);
    return resp.body;
}
