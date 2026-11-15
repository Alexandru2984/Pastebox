#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <fstream>
#include "Security.h"

void initDatabase() {
    auto db = drogon::app().getDbClient();
    db->execSqlSync("PRAGMA journal_mode=WAL");
    db->execSqlSync("PRAGMA foreign_keys=ON");
    db->execSqlSync("PRAGMA busy_timeout=5000");
    // Bound database growth (256 MiB with the default 4 KiB page size).
    db->execSqlSync("PRAGMA max_page_count=65536");
    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pastes (id TEXT PRIMARY KEY, title TEXT NOT NULL DEFAULT 'Untitled',"
        "content TEXT NOT NULL, language TEXT NOT NULL DEFAULT 'plaintext',"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, expires_at DATETIME, views INTEGER DEFAULT 0,"
        "password_hash TEXT, burn_after_read INTEGER DEFAULT 0, visibility TEXT DEFAULT 'public',"
        "parent_id TEXT, owner_hash TEXT, updated_at DATETIME)");
    auto columns = db->execSqlSync("PRAGMA table_info(pastes)");
    const std::vector<std::pair<std::string, std::string>> additions = {
        {"expires_at", "DATETIME"}, {"password_hash", "TEXT"},
        {"burn_after_read", "INTEGER DEFAULT 0"}, {"visibility", "TEXT DEFAULT 'public'"},
        {"parent_id", "TEXT"}, {"owner_hash", "TEXT"}, {"updated_at", "DATETIME"}
    };
    for (const auto &[name, type] : additions) {
        bool found = false;
        for (const auto &column : columns) if (column["name"].as<std::string>() == name) found = true;
        if (!found) db->execSqlSync("ALTER TABLE pastes ADD COLUMN " + name + " " + type);
    }
    db->execSqlSync("CREATE TABLE IF NOT EXISTS tags (paste_id TEXT NOT NULL, tag TEXT NOT NULL,"
                    "PRIMARY KEY(paste_id,tag), FOREIGN KEY(paste_id) REFERENCES pastes(id) ON DELETE CASCADE)");
    db->execSqlSync("CREATE INDEX IF NOT EXISTS idx_pastes_expires ON pastes(expires_at)");
    db->execSqlSync("CREATE INDEX IF NOT EXISTS idx_pastes_public_created ON pastes(visibility,created_at DESC,id)");
    db->execSqlSync("CREATE INDEX IF NOT EXISTS idx_tags_tag ON tags(tag,paste_id)");
    LOG_INFO << "PasteBox database ready";
}

int main() {
    drogon::app().loadConfigFile(security::env("PASTEBOX_CONFIG", "../config.json"));
    drogon::app().registerBeginningAdvice(initDatabase);
    static security::Limiter requests, writes;
    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req,
        drogon::AdviceCallback &&callback, drogon::AdviceChainCallback &&next) {
        if (req->path().rfind("/api/", 0) != 0) { next(); return; }
        const auto origin = security::env("PASTEBOX_ORIGIN", "https://pastebox.micutu.com");
        if (req->method() == drogon::Options) {
            if (!req->getHeader("Origin").empty() && req->getHeader("Origin") != origin) {
                callback(security::error(drogon::k403Forbidden, "Origin not allowed")); return;
            }
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setStatusCode(drogon::k204NoContent);
            response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            response->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Password, X-Owner-Token, X-Requested-With, X-Confirm-Burn");
            response->addHeader("Access-Control-Max-Age", "600");
            callback(response); return;
        }
        const bool mutation = req->method() == drogon::Post || req->method() == drogon::Put || req->method() == drogon::Delete;
        if (mutation && (req->getHeader("X-Requested-With") != "PasteBox" ||
            (!req->getHeader("Origin").empty() && req->getHeader("Origin") != origin) ||
            req->getHeader("Sec-Fetch-Site") == "cross-site")) {
            callback(security::error(drogon::k403Forbidden, "Request origin or required header is invalid")); return;
        }
        const auto ip = security::clientIp(req);
        if (!requests.allow(ip, 10, 30) || (mutation && !writes.allow(ip, 0.5, 12))) {
            auto response = security::error(drogon::k429TooManyRequests, "Too many requests. Please try again shortly.");
            response->addHeader("Retry-After", mutation ? "2" : "1");
            callback(response); return;
        }
        if ((req->method() == drogon::Post || req->method() == drogon::Put) &&
            req->getHeader("Content-Type").rfind("application/json", 0) != 0) {
            callback(security::error(drogon::k415UnsupportedMediaType, "Use application/json")); return;
        }
        next();
    });
    drogon::app().registerPreSendingAdvice([](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &response) {
        response->addHeader("X-Content-Type-Options", "nosniff");
        response->addHeader("Referrer-Policy", "no-referrer");
        response->addHeader("Strict-Transport-Security", "max-age=31536000");
        response->addHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
        response->addHeader("X-Frame-Options", "SAMEORIGIN");
        response->addHeader("Content-Security-Policy",
            "default-src 'self'; script-src 'self' https://analytics.micutu.com https://static.cloudflareinsights.com; "
            "style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; "
            "connect-src 'self' https://analytics.micutu.com https://cloudflareinsights.com; "
            "object-src 'none'; base-uri 'none'; form-action 'self'; frame-ancestors 'self'");
        if (req->path().rfind("/api/", 0) == 0) {
            response->addHeader("Cache-Control", "no-store");
            response->addHeader("X-Robots-Tag", "noindex, nofollow, noarchive");
            response->addHeader("Access-Control-Allow-Origin", security::env("PASTEBOX_ORIGIN", "https://pastebox.micutu.com"));
            response->addHeader("Vary", "Origin");
        } else if (req->path() == "/" || req->path() == "/index.html") {
            response->addHeader("Cache-Control", "no-cache");
        }
    });
    drogon::app().setCustom404Page(security::error(drogon::k404NotFound, "Resource not found"), false);
    drogon::app().getLoop()->runEvery(60, [] {
        drogon::app().getDbClient()->execSqlAsync(
            "DELETE FROM pastes WHERE expires_at IS NOT NULL AND expires_at <= datetime('now')",
            [](const drogon::orm::Result &) {},
            [](const drogon::orm::DrogonDbException &) { LOG_ERROR << "Expiration cleanup failed"; });
    });
    drogon::app().run();
}
