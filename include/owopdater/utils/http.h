#pragma once

#include <string>
#include <map>

struct HttpResponse {
    int status_code = 0;
    std::string reason;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string HttpGet(const std::string& fullUrl);
std::string HttpPostJson(const std::string& fullUrl, const std::string& jsonBody);



HttpResponse HttpGetFull(const std::string& fullUrl);
HttpResponse HttpPostJsonFull(const std::string& fullUrl, const std::string& jsonBody);


HttpResponse HttpRequestFull(const std::string& fullUrl, const std::string& method, const std::string& body = "", const std::map<std::string, std::string>& headers = {});
