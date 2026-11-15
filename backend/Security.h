#pragma once
#include <drogon/drogon.h>
#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace security {
inline std::string env(const char *name, const std::string &fallback = "") {
    const char *value = std::getenv(name);
    return value && *value ? value : fallback;
}
inline std::string clientIp(const drogon::HttpRequestPtr &req) {
    const auto peer = req->peerAddr().toIp();
    // Opt in only when nginx overwrites X-Real-IP. Never use the first XFF value.
    if (env("PASTEBOX_TRUST_PROXY") == "1" &&
        (peer == "127.0.0.1" || peer == "::1")) {
        const auto &ip = req->getHeader("X-Real-IP");
        unsigned char address[16];
        if (inet_pton(AF_INET, ip.c_str(), address) == 1 ||
            inet_pton(AF_INET6, ip.c_str(), address) == 1) return ip;
    }
    return peer;
}
class Limiter {
    struct Bucket { double tokens; std::chrono::steady_clock::time_point last; };
    std::mutex mutex;
    std::unordered_map<std::string, Bucket> buckets;
    std::chrono::steady_clock::time_point lastCleanup{};
public:
    bool allow(const std::string &key, double rate, double burst) {
        std::lock_guard<std::mutex> lock(mutex);
        auto now = std::chrono::steady_clock::now();
        if (now - lastCleanup > std::chrono::minutes(1)) {
            for (auto it = buckets.begin(); it != buckets.end();) {
                if (now - it->second.last > std::chrono::minutes(10)) it = buckets.erase(it);
                else ++it;
            }
            lastCleanup = now;
        }
        auto it = buckets.find(key);
        if (it == buckets.end()) {
            if (buckets.size() >= 10000) return false;
            it = buckets.emplace(key, Bucket{burst, now}).first;
        }
        auto &bucket = it->second;
        bucket.tokens = std::min(burst, bucket.tokens +
            std::chrono::duration<double>(now - bucket.last).count() * rate);
        bucket.last = now;
        if (bucket.tokens < 1) return false;
        bucket.tokens -= 1;
        return true;
    }
};
inline drogon::HttpResponsePtr error(drogon::HttpStatusCode code, const std::string &message) {
    Json::Value json; json["error"] = message;
    auto response = drogon::HttpResponse::newHttpJsonResponse(json);
    response->setStatusCode(code);
    return response;
}
}
