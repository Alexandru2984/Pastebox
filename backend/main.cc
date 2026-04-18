#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <chrono>

// Rate limiting state
struct RateBucket {
    double tokens;
    std::chrono::steady_clock::time_point lastRefill;
};
static std::unordered_map<std::string, RateBucket> rateLimits;
static std::mutex rateMutex;

static constexpr double RATE_LIMIT = 10.0;  // requests per second
static constexpr double BURST_SIZE = 30.0;
static constexpr size_t MAX_PASTE_SIZE = 512 * 1024; // 512KB

bool checkRateLimit(const std::string &ip) {
    std::lock_guard<std::mutex> lock(rateMutex);
    auto now = std::chrono::steady_clock::now();
    auto &bucket = rateLimits[ip];
    if (bucket.tokens == 0 && bucket.lastRefill.time_since_epoch().count() == 0) {
        bucket.tokens = BURST_SIZE;
        bucket.lastRefill = now;
    }
    double elapsed = std::chrono::duration<double>(now - bucket.lastRefill).count();
    bucket.tokens = std::min(BURST_SIZE, bucket.tokens + elapsed * RATE_LIMIT);
    bucket.lastRefill = now;
    if (bucket.tokens >= 1.0) {
        bucket.tokens -= 1.0;
        return true;
    }
    return false;
}

void initDatabase() {
    auto db = drogon::app().getDbClient();

    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pastes ("
        "  id TEXT PRIMARY KEY,"
        "  title TEXT NOT NULL DEFAULT 'Untitled',"
        "  content TEXT NOT NULL,"
        "  language TEXT NOT NULL DEFAULT 'plaintext',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  expires_at DATETIME,"
        "  views INTEGER DEFAULT 0,"
        "  password_hash TEXT,"
        "  burn_after_read INTEGER DEFAULT 0,"
        "  visibility TEXT DEFAULT 'public',"
        "  parent_id TEXT"
        ")");

    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS tags ("
        "  paste_id TEXT NOT NULL,"
        "  tag TEXT NOT NULL,"
        "  PRIMARY KEY (paste_id, tag),"
        "  FOREIGN KEY (paste_id) REFERENCES pastes(id) ON DELETE CASCADE"
        ")");

    // Migrate existing table if needed
    try { db->execSqlSync("ALTER TABLE pastes ADD COLUMN expires_at DATETIME"); } catch (...) {}
    try { db->execSqlSync("ALTER TABLE pastes ADD COLUMN password_hash TEXT"); } catch (...) {}
    try { db->execSqlSync("ALTER TABLE pastes ADD COLUMN burn_after_read INTEGER DEFAULT 0"); } catch (...) {}
    try { db->execSqlSync("ALTER TABLE pastes ADD COLUMN visibility TEXT DEFAULT 'public'"); } catch (...) {}
    try { db->execSqlSync("ALTER TABLE pastes ADD COLUMN parent_id TEXT"); } catch (...) {}

    std::cout << "[PasteBox] Database initialized." << std::endl;
}

void cleanupExpired() {
    try {
        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "DELETE FROM pastes WHERE expires_at IS NOT NULL AND expires_at < datetime('now')",
            [](const drogon::orm::Result &r) {
                if (r.affectedRows() > 0)
                    std::cout << "[PasteBox] Cleaned " << r.affectedRows() << " expired pastes." << std::endl;
            },
            [](const drogon::orm::DrogonDbException &) {});
    } catch (...) {}
}

int main() {
    drogon::app().loadConfigFile("../config.json");

    drogon::app().registerBeginningAdvice([]() {
        initDatabase();
    });

    // Rate limiting middleware
    drogon::app().registerPreHandlingAdvice(
        [](const drogon::HttpRequestPtr &req,
           drogon::AdviceCallback &&callback,
           drogon::AdviceChainCallback &&chainCallback) {
            auto ip = req->peerAddr().toIp();
            if (!checkRateLimit(ip)) {
                Json::Value ret;
                ret["error"] = "Rate limit exceeded. Please slow down.";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(drogon::k429TooManyRequests);
                resp->addHeader("Retry-After", "1");
                callback(resp);
                return;
            }
            chainCallback();
        });

    // Global CORS for OPTIONS preflight
    drogon::app().registerPreRoutingAdvice(
        [](const drogon::HttpRequestPtr &req,
           drogon::AdviceCallback &&callback,
           drogon::AdviceChainCallback &&chainCallback) {
            if (req->method() == drogon::Options) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Password");
                resp->addHeader("Access-Control-Max-Age", "86400");
                callback(resp);
                return;
            }
            chainCallback();
        });

    // Fallback: serve index.html for SPA routes
    drogon::app().setCustom404Page(
        drogon::HttpResponse::newFileResponse("./public/index.html"),
        false);

    // Expiration cleanup every 60 seconds
    drogon::app().getLoop()->runEvery(60.0, []() { cleanupExpired(); });

    std::cout << "[PasteBox] Starting on http://localhost:7777" << std::endl;
    drogon::app().run();
    return 0;
}
