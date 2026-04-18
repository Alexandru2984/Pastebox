#include "PasteController.h"
#include <drogon/orm/DbClient.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <unordered_map>
#include <chrono>

using namespace drogon;
using namespace drogon::orm;

static constexpr size_t MAX_PASTE_SIZE = 512 * 1024;
static constexpr size_t MAX_TITLE_LENGTH = 200;
static constexpr size_t MAX_TAG_LENGTH = 50;
static constexpr size_t MAX_TAGS_COUNT = 10;
static constexpr size_t MAX_LANGUAGE_LENGTH = 30;
static constexpr size_t MIN_PASSWORD_LENGTH = 4;
static constexpr size_t MAX_PASSWORD_LENGTH = 256;

// Brute-force protection: track failed password attempts per IP
struct AuthAttempt {
    int failures;
    std::chrono::steady_clock::time_point lastFailure;
};
static std::unordered_map<std::string, AuthAttempt> authAttempts;
static std::mutex authMutex;
static constexpr int MAX_AUTH_FAILURES = 5;
static constexpr double AUTH_LOCKOUT_SECONDS = 300.0; // 5 min lockout

static bool isAuthLocked(const std::string &ip) {
    std::lock_guard<std::mutex> lock(authMutex);
    auto it = authAttempts.find(ip);
    if (it == authAttempts.end()) return false;
    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - it->second.lastFailure).count();
    if (elapsed > AUTH_LOCKOUT_SECONDS) {
        authAttempts.erase(it);
        return false;
    }
    return it->second.failures >= MAX_AUTH_FAILURES;
}

static void recordAuthFailure(const std::string &ip) {
    std::lock_guard<std::mutex> lock(authMutex);
    auto &entry = authAttempts[ip];
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - entry.lastFailure).count();
    if (elapsed > AUTH_LOCKOUT_SECONDS) {
        entry.failures = 0;
    }
    entry.failures++;
    entry.lastFailure = now;
}

static void clearAuthFailures(const std::string &ip) {
    std::lock_guard<std::mutex> lock(authMutex);
    authAttempts.erase(ip);
}

// Cryptographically secure ID generation using OpenSSL RAND_bytes
// Uses rejection sampling to eliminate modular bias
std::string PasteController::generateId(int length) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    static constexpr int charCount = sizeof(chars) - 1; // 62
    // Largest multiple of 62 that fits in a byte: 62 * 4 = 248
    static constexpr unsigned char maxUnbiased = (256 / charCount) * charCount - 1; // 247
    std::string id;
    id.reserve(length);
    while (static_cast<int>(id.size()) < length) {
        unsigned char buf[32];
        RAND_bytes(buf, sizeof(buf));
        for (size_t i = 0; i < sizeof(buf) && static_cast<int>(id.size()) < length; ++i) {
            if (buf[i] <= maxUnbiased)
                id += chars[buf[i] % charCount];
        }
    }
    return id;
}

void PasteController::addCorsHeaders(const HttpResponsePtr &resp) {
    resp->addHeader("Access-Control-Allow-Origin", "https://pastebox.micutu.com");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Password, X-Requested-With");
    resp->addHeader("Vary", "Origin");
}

void PasteController::addSecurityHeaders(const HttpResponsePtr &resp) {
    addCorsHeaders(resp);
    resp->addHeader("X-Content-Type-Options", "nosniff");
    resp->addHeader("X-Frame-Options", "DENY");
    resp->addHeader("Referrer-Policy", "strict-origin-when-cross-origin");
    resp->addHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    resp->addHeader("X-XSS-Protection", "1; mode=block");
    resp->addHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
    resp->addHeader("Content-Security-Policy",
        "default-src 'self'; "
        "script-src 'self'; "
        "style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; "
        "font-src 'self'; "
        "connect-src 'self'; "
        "frame-ancestors 'none'");
}

static std::string generateSalt(int length = 16) {
    std::vector<unsigned char> buf(length);
    RAND_bytes(buf.data(), length);
    std::stringstream ss;
    for (int i = 0; i < length; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
    return ss.str();
}

std::string PasteController::sha256Hash(const std::string &input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

std::string PasteController::hashPassword(const std::string &password) {
    std::string salt = generateSalt();
    std::string hash = sha256Hash(salt + password);
    return salt + ":" + hash;
}

bool PasteController::isExpired(const Row &row) {
    if (row["expires_at"].isNull()) return false;
    return false; // Actual check done in SQL WHERE clause
}

bool PasteController::checkPassword(const Row &row, const std::string &password) {
    if (row["password_hash"].isNull()) return true;
    auto stored = row["password_hash"].as<std::string>();
    if (stored.empty()) return true;

    // Support salted format "salt:hash"
    auto colonPos = stored.find(':');
    if (colonPos != std::string::npos) {
        std::string salt = stored.substr(0, colonPos);
        std::string expectedHash = stored.substr(colonPos + 1);
        std::string computed = sha256Hash(salt + password);
        // Constant-time comparison to prevent timing attacks
        if (computed.size() != expectedHash.size()) return false;
        return CRYPTO_memcmp(computed.data(), expectedHash.data(), computed.size()) == 0;
    }
    // Fallback: legacy unsalted hash
    std::string computed = sha256Hash(password);
    if (computed.size() != stored.size()) return false;
    return CRYPTO_memcmp(computed.data(), stored.data(), computed.size()) == 0;
}

Json::Value PasteController::pasteRowToJson(const Row &row, bool includeContent) {
    Json::Value ret;
    ret["id"] = row["id"].as<std::string>();
    ret["title"] = row["title"].as<std::string>();
    ret["language"] = row["language"].as<std::string>();
    ret["created_at"] = row["created_at"].as<std::string>();
    ret["views"] = row["views"].as<int64_t>();
    ret["visibility"] = row["visibility"].isNull() ? "public" : row["visibility"].as<std::string>();
    ret["burn_after_read"] = (!row["burn_after_read"].isNull() && row["burn_after_read"].as<int64_t>() == 1);
    ret["has_password"] = (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty());
    if (!row["expires_at"].isNull())
        ret["expires_at"] = row["expires_at"].as<std::string>();
    else
        ret["expires_at"] = Json::nullValue;
    if (!row["parent_id"].isNull())
        ret["parent_id"] = row["parent_id"].as<std::string>();
    else
        ret["parent_id"] = Json::nullValue;
    if (includeContent)
        ret["content"] = row["content"].as<std::string>();
    return ret;
}

// ─── CREATE ─────────────────────────────────────────────────────────────────
void PasteController::createPaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

    // CSRF protection: require custom header on state-changing requests
    if (req->getHeader("X-Requested-With").empty()) {
        Json::Value ret; ret["error"] = "Missing required header";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k403Forbidden); addSecurityHeaders(resp);
        callback(resp); return;
    }

    auto json = req->getJsonObject();
    if (!json) {
        Json::Value ret; ret["error"] = "Invalid JSON body";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k400BadRequest); addSecurityHeaders(resp);
        callback(resp); return;
    }

    std::string content = (*json).get("content", "").asString();
    if (content.empty()) {
        Json::Value ret; ret["error"] = "Content cannot be empty";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k400BadRequest); addSecurityHeaders(resp);
        callback(resp); return;
    }
    if (content.size() > MAX_PASTE_SIZE) {
        Json::Value ret; ret["error"] = "Paste too large. Max size is 512KB.";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k413RequestEntityTooLarge); addSecurityHeaders(resp);
        callback(resp); return;
    }

    std::string password = (*json).get("password", "").asString();
    // Validate password length
    if (!password.empty() && password.size() < MIN_PASSWORD_LENGTH) {
        Json::Value ret; ret["error"] = "Password must be at least 4 characters";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k400BadRequest); addSecurityHeaders(resp);
        callback(resp); return;
    }
    if (password.size() > MAX_PASSWORD_LENGTH) {
        Json::Value ret; ret["error"] = "Password too long (max 256 characters)";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k400BadRequest); addSecurityHeaders(resp);
        callback(resp); return;
    }

    std::string id = generateId();
    std::string title = (*json).get("title", "Untitled").asString();
    if (title.size() > MAX_TITLE_LENGTH) title = title.substr(0, MAX_TITLE_LENGTH);

    std::string language = (*json).get("language", "plaintext").asString();
    if (language.size() > MAX_LANGUAGE_LENGTH) language = "plaintext";

    std::string visibility = (*json).get("visibility", "public").asString();
    bool burnAfterRead = (*json).get("burn_after_read", false).asBool();
    std::string expiresIn = (*json).get("expires_in", "").asString();
    std::string parentId = (*json).get("parent_id", "").asString();

    // Validate visibility
    if (visibility != "public" && visibility != "private" && visibility != "unlisted")
        visibility = "public";

    // Hash password with salt if provided
    std::string passwordHash = password.empty() ? "" : hashPassword(password);

    // Calculate expires_at from expires_in
    std::string expiresAtExpr = "";
    if (expiresIn == "1h") expiresAtExpr = "datetime('now', '+1 hour')";
    else if (expiresIn == "24h") expiresAtExpr = "datetime('now', '+1 day')";
    else if (expiresIn == "7d") expiresAtExpr = "datetime('now', '+7 days')";
    else if (expiresIn == "30d") expiresAtExpr = "datetime('now', '+30 days')";

    // Extract and validate tags
    Json::Value tagsJson = (*json).get("tags", Json::arrayValue);

    auto db = app().getDbClient();
    std::string sql;
    if (expiresAtExpr.empty()) {
        sql = "INSERT INTO pastes (id, title, content, language, visibility, burn_after_read, password_hash, parent_id) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    } else {
        sql = "INSERT INTO pastes (id, title, content, language, visibility, burn_after_read, password_hash, parent_id, expires_at) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, " + expiresAtExpr + ")";
    }

    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto sharedTags = std::make_shared<Json::Value>(tagsJson);
    auto sharedId = std::make_shared<std::string>(id);

    db->execSqlAsync(
        sql,
        [sharedCallback, sharedId, title, language, visibility, burnAfterRead, sharedTags, db](const Result &) {
            // Insert tags (limit count and length)
            int tagCount = 0;
            for (const auto &tag : *sharedTags) {
                if (tagCount >= 10) break;
                if (tag.isString() && !tag.asString().empty() && tag.asString().size() <= 50) {
                    db->execSqlAsync(
                        "INSERT OR IGNORE INTO tags (paste_id, tag) VALUES (?, ?)",
                        [](const Result &) {},
                        [](const DrogonDbException &) {},
                        *sharedId, tag.asString());
                    tagCount++;
                }
            }

            Json::Value ret;
            ret["id"] = *sharedId;
            ret["title"] = title;
            ret["language"] = language;
            ret["visibility"] = visibility;
            ret["burn_after_read"] = burnAfterRead;
            ret["message"] = "Paste created successfully";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k201Created);
            addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        },
        [sharedCallback](const DrogonDbException &e) {
            Json::Value ret;
            ret["error"] = "Internal server error";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError);
            addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        },
        id, title, content, language, visibility, (burnAfterRead ? 1 : 0),
        passwordHash, (parentId.empty() ? "" : parentId));
}

// ─── LIST ───────────────────────────────────────────────────────────────────
void PasteController::listPastes(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

    auto db = app().getDbClient();
    std::string tag = req->getParameter("tag");
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    // Pagination
    int page = 1, limit = 50;
    try {
        auto p = req->getParameter("page");
        auto l = req->getParameter("limit");
        if (!p.empty()) page = std::max(1, std::stoi(p));
        if (!l.empty()) limit = std::max(1, std::min(100, std::stoi(l)));
    } catch (...) {}
    int offset = (page - 1) * limit;

    // Single query with GROUP_CONCAT to avoid N+1 tag fetching
    std::string sql;
    if (tag.empty()) {
        sql = "SELECT p.id, p.title, p.language, p.created_at, length(p.content) as size, "
              "p.visibility, p.burn_after_read, p.password_hash, p.expires_at, p.views, "
              "GROUP_CONCAT(t.tag) as tag_list "
              "FROM pastes p LEFT JOIN tags t ON p.id = t.paste_id "
              "WHERE p.visibility = 'public' "
              "AND (p.expires_at IS NULL OR p.expires_at > datetime('now')) "
              "GROUP BY p.id ORDER BY p.created_at DESC LIMIT ? OFFSET ?";
    } else {
        sql = "SELECT p.id, p.title, p.language, p.created_at, length(p.content) as size, "
              "p.visibility, p.burn_after_read, p.password_hash, p.expires_at, p.views, "
              "GROUP_CONCAT(t2.tag) as tag_list "
              "FROM pastes p "
              "JOIN tags t ON p.id = t.paste_id AND t.tag = ? "
              "LEFT JOIN tags t2 ON p.id = t2.paste_id "
              "WHERE p.visibility = 'public' "
              "AND (p.expires_at IS NULL OR p.expires_at > datetime('now')) "
              "GROUP BY p.id ORDER BY p.created_at DESC LIMIT ? OFFSET ?";
    }

    auto handleResult = [sharedCallback, page, limit](const Result &result) {
        Json::Value response;
        Json::Value pastes(Json::arrayValue);

        for (const auto &row : result) {
            Json::Value paste;
            paste["id"] = row["id"].as<std::string>();
            paste["title"] = row["title"].as<std::string>();
            paste["language"] = row["language"].as<std::string>();
            paste["created_at"] = row["created_at"].as<std::string>();
            paste["size"] = row["size"].as<int64_t>();
            paste["views"] = row["views"].as<int64_t>();
            paste["visibility"] = row["visibility"].isNull() ? "public" : row["visibility"].as<std::string>();
            paste["burn_after_read"] = (!row["burn_after_read"].isNull() && row["burn_after_read"].as<int64_t>() == 1);
            paste["has_password"] = (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty());
            if (!row["expires_at"].isNull())
                paste["expires_at"] = row["expires_at"].as<std::string>();
            else
                paste["expires_at"] = Json::nullValue;

            // Parse GROUP_CONCAT tags
            Json::Value tags(Json::arrayValue);
            if (!row["tag_list"].isNull()) {
                std::string tagStr = row["tag_list"].as<std::string>();
                std::istringstream ss(tagStr);
                std::string t;
                while (std::getline(ss, t, ',')) {
                    if (!t.empty()) tags.append(t);
                }
            }
            paste["tags"] = tags;
            pastes.append(paste);
        }

        response["data"] = pastes;
        response["page"] = page;
        response["limit"] = limit;
        response["count"] = static_cast<int>(result.size());
        response["has_more"] = static_cast<int>(result.size()) == limit;
        auto resp = HttpResponse::newHttpJsonResponse(response);
        addSecurityHeaders(resp);
        (*sharedCallback)(resp);
    };

    auto errorHandler = [sharedCallback](const DrogonDbException &e) {
        Json::Value ret; ret["error"] = "Internal server error";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
        (*sharedCallback)(resp);
    };

    if (tag.empty()) {
        db->execSqlAsync(sql, handleResult, errorHandler, limit, offset);
    } else {
        db->execSqlAsync(sql, handleResult, errorHandler, tag, limit, offset);
    }
}

// ─── GET PASTE ──────────────────────────────────────────────────────────────
void PasteController::getPaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    auto db = app().getDbClient();
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto sharedReq = req;

    auto clientIp = std::make_shared<std::string>(req->peerAddr().toIp());

    // Check brute-force lockout before hitting DB
    if (isAuthLocked(*clientIp)) {
        Json::Value ret;
        ret["error"] = "Too many failed attempts. Try again later.";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k429TooManyRequests);
        resp->addHeader("Retry-After", "300");
        addSecurityHeaders(resp);
        (*sharedCallback)(resp); return;
    }

    db->execSqlAsync(
        "SELECT * FROM pastes WHERE id = ? AND (expires_at IS NULL OR expires_at > datetime('now'))",
        [sharedCallback, sharedReq, db, id, clientIp](const Result &result) {
            if (result.empty()) {
                Json::Value ret; ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Check password with brute-force protection
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    recordAuthFailure(*clientIp);
                    Json::Value ret;
                    // Return 404 (not 403) to prevent paste ID enumeration
                    ret["error"] = "Paste not found";
                    ret["password_required"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
                clearAuthFailures(*clientIp);
            }

            bool isBurn = (!row["burn_after_read"].isNull() && row["burn_after_read"].as<int64_t>() == 1);
            auto paste = pasteRowToJson(row, true);

            // Fetch tags
            db->execSqlAsync(
                "SELECT tag FROM tags WHERE paste_id = ?",
                [sharedCallback, paste, isBurn, db, id](const Result &tagResult) mutable {
                    Json::Value tags(Json::arrayValue);
                    for (const auto &r : tagResult) tags.append(r["tag"].as<std::string>());
                    paste["tags"] = tags;

                    // Increment views
                    db->execSqlAsync(
                        "UPDATE pastes SET views = views + 1 WHERE id = ?",
                        [](const Result &) {}, [](const DrogonDbException &) {}, id);

                    // Burn after read: delete after sending
                    if (isBurn) {
                        paste["burned"] = true;
                        db->execSqlAsync(
                            "DELETE FROM pastes WHERE id = ?",
                            [](const Result &) {}, [](const DrogonDbException &) {}, id);
                    }

                    auto resp = HttpResponse::newHttpJsonResponse(paste);
                    // ETag for caching (content hash — stable across views)
                    if (!isBurn) {
                        std::string etagInput = paste["id"].asString() + "|" +
                                               paste["title"].asString() + "|" +
                                               paste["content"].asString() + "|" +
                                               paste["language"].asString();
                        std::string etag = "\"" + sha256Hash(etagInput).substr(0, 16) + "\"";
                        resp->addHeader("ETag", etag);
                        resp->addHeader("Cache-Control", "private, no-cache");
                    } else {
                        resp->addHeader("Cache-Control", "no-store");
                    }
                    addSecurityHeaders(resp);
                    (*sharedCallback)(resp);
                },
                [sharedCallback](const DrogonDbException &) {
                    Json::Value ret; ret["error"] = "Database error";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
                    (*sharedCallback)(resp);
                }, id);
        },
        [sharedCallback](const DrogonDbException &e) {
            Json::Value ret; ret["error"] = "Internal server error";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        }, id);
}

// ─── RAW PASTE ──────────────────────────────────────────────────────────────
void PasteController::getRawPaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    auto db = app().getDbClient();
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto sharedReq = req;

    auto clientIp = std::make_shared<std::string>(req->peerAddr().toIp());

    if (isAuthLocked(*clientIp)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k429TooManyRequests);
        resp->setContentTypeString("text/plain");
        resp->setBody("Too many failed attempts. Try again later.");
        resp->addHeader("Retry-After", "300");
        addSecurityHeaders(resp);
        (*sharedCallback)(resp); return;
    }

    db->execSqlAsync(
        "SELECT * FROM pastes WHERE id = ? AND (expires_at IS NULL OR expires_at > datetime('now'))",
        [sharedCallback, sharedReq, clientIp](const Result &result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k404NotFound);
                resp->setContentTypeString("text/plain");
                resp->setBody("Paste not found");
                addSecurityHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Check password with brute-force protection
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    recordAuthFailure(*clientIp);
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k404NotFound);
                    resp->setContentTypeString("text/plain");
                    resp->setBody("Paste not found");
                    addSecurityHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
                clearAuthFailures(*clientIp);
            }

            auto resp = HttpResponse::newHttpResponse();
            resp->setContentTypeString("text/plain; charset=utf-8");
            resp->setBody(row["content"].as<std::string>());
            resp->addHeader("Content-Disposition", "inline");
            addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        },
        [sharedCallback](const DrogonDbException &e) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k500InternalServerError);
            resp->setContentTypeString("text/plain");
            resp->setBody("Database error");
            addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        }, id);
}

// ─── FORK PASTE ─────────────────────────────────────────────────────────────
void PasteController::forkPaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    // CSRF protection
    if (req->getHeader("X-Requested-With").empty()) {
        Json::Value ret; ret["error"] = "Missing required header";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k403Forbidden); addSecurityHeaders(resp);
        callback(resp); return;
    }

    auto db = app().getDbClient();
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto sharedReq = req;

    auto clientIp = std::make_shared<std::string>(req->peerAddr().toIp());

    if (isAuthLocked(*clientIp)) {
        Json::Value ret;
        ret["error"] = "Too many failed attempts. Try again later.";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k429TooManyRequests);
        resp->addHeader("Retry-After", "300");
        addSecurityHeaders(resp);
        (*sharedCallback)(resp); return;
    }

    db->execSqlAsync(
        "SELECT * FROM pastes WHERE id = ? AND (expires_at IS NULL OR expires_at > datetime('now'))",
        [sharedCallback, sharedReq, db, id, clientIp](const Result &result) {
            if (result.empty()) {
                Json::Value ret; ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Check password with brute-force protection
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    recordAuthFailure(*clientIp);
                    // Return 404 (not 403) to prevent paste ID enumeration
                    Json::Value ret; ret["error"] = "Paste not found"; ret["password_required"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
                clearAuthFailures(*clientIp);
            }

            std::string newId = generateId();
            std::string title = "Fork of " + row["title"].as<std::string>();
            std::string content = row["content"].as<std::string>();
            std::string language = row["language"].as<std::string>();

            // Allow overriding title/content from request body with validation
            auto json = sharedReq->getJsonObject();
            if (json) {
                if (json->isMember("title")) {
                    title = (*json)["title"].asString();
                    if (title.size() > MAX_TITLE_LENGTH) title = title.substr(0, MAX_TITLE_LENGTH);
                }
                if (json->isMember("content")) {
                    content = (*json)["content"].asString();
                    if (content.empty()) {
                        Json::Value ret; ret["error"] = "Content cannot be empty";
                        auto resp = HttpResponse::newHttpJsonResponse(ret);
                        resp->setStatusCode(k400BadRequest); addSecurityHeaders(resp);
                        (*sharedCallback)(resp); return;
                    }
                    if (content.size() > MAX_PASTE_SIZE) {
                        Json::Value ret; ret["error"] = "Paste too large. Max size is 512KB.";
                        auto resp = HttpResponse::newHttpJsonResponse(ret);
                        resp->setStatusCode(k413RequestEntityTooLarge); addSecurityHeaders(resp);
                        (*sharedCallback)(resp); return;
                    }
                }
                if (json->isMember("language")) {
                    language = (*json)["language"].asString();
                    if (language.size() > MAX_LANGUAGE_LENGTH) language = "plaintext";
                }
            }

            db->execSqlAsync(
                "INSERT INTO pastes (id, title, content, language, visibility, parent_id) "
                "VALUES (?, ?, ?, ?, 'public', ?)",
                [sharedCallback, newId, title, language, id](const Result &) {
                    Json::Value ret;
                    ret["id"] = newId;
                    ret["title"] = title;
                    ret["language"] = language;
                    ret["parent_id"] = id;
                    ret["message"] = "Paste forked successfully";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k201Created); addSecurityHeaders(resp);
                    (*sharedCallback)(resp);
                },
                [sharedCallback](const DrogonDbException &e) {
                    Json::Value ret; ret["error"] = "Internal server error";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
                    (*sharedCallback)(resp);
                },
                newId, title, content, language, id);
        },
        [sharedCallback](const DrogonDbException &e) {
            Json::Value ret; ret["error"] = "Internal server error";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        }, id);
}

// ─── DELETE ─────────────────────────────────────────────────────────────────
void PasteController::deletePaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    // CSRF protection
    if (req->getHeader("X-Requested-With").empty()) {
        Json::Value ret; ret["error"] = "Missing required header";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k403Forbidden); addSecurityHeaders(resp);
        callback(resp); return;
    }

    auto db = app().getDbClient();
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto sharedReq = req;

    auto clientIp = std::make_shared<std::string>(req->peerAddr().toIp());

    if (isAuthLocked(*clientIp)) {
        Json::Value ret;
        ret["error"] = "Too many failed attempts. Try again later.";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k429TooManyRequests);
        resp->addHeader("Retry-After", "300");
        addSecurityHeaders(resp);
        (*sharedCallback)(resp); return;
    }

    // First check if paste exists and is password-protected
    db->execSqlAsync(
        "SELECT password_hash FROM pastes WHERE id = ?",
        [sharedCallback, sharedReq, db, id, clientIp](const Result &result) {
            if (result.empty()) {
                Json::Value ret; ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Require password for protected pastes with brute-force protection
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    recordAuthFailure(*clientIp);
                    // Return 404 (not 403) to prevent paste ID enumeration
                    Json::Value ret;
                    ret["error"] = "Paste not found";
                    ret["password_required"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
                clearAuthFailures(*clientIp);
            }

            db->execSqlAsync(
                "DELETE FROM pastes WHERE id = ?",
                [sharedCallback, id](const Result &) {
                    Json::Value ret; ret["message"] = "Paste deleted"; ret["id"] = id;
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    addSecurityHeaders(resp);
                    (*sharedCallback)(resp);
                },
                [sharedCallback](const DrogonDbException &) {
                    Json::Value ret; ret["error"] = "Internal server error";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
                    (*sharedCallback)(resp);
                }, id);
        },
        [sharedCallback](const DrogonDbException &) {
            Json::Value ret; ret["error"] = "Internal server error";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        }, id);
}

// ─── UPDATE ─────────────────────────────────────────────────────────────────
void PasteController::updatePaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    // CSRF protection
    if (req->getHeader("X-Requested-With").empty()) {
        Json::Value ret; ret["error"] = "Missing required header";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k403Forbidden); addSecurityHeaders(resp);
        callback(resp); return;
    }

    auto json = req->getJsonObject();
    if (!json) {
        Json::Value ret; ret["error"] = "Invalid JSON body";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k400BadRequest); addSecurityHeaders(resp);
        callback(resp); return;
    }

    auto db = app().getDbClient();
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto sharedReq = req;
    auto sharedJson = json;

    auto clientIp = std::make_shared<std::string>(req->peerAddr().toIp());

    if (isAuthLocked(*clientIp)) {
        Json::Value ret; ret["error"] = "Too many failed attempts. Try again later.";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k429TooManyRequests);
        resp->addHeader("Retry-After", "300"); addSecurityHeaders(resp);
        (*sharedCallback)(resp); return;
    }

    db->execSqlAsync(
        "SELECT * FROM pastes WHERE id = ? AND (expires_at IS NULL OR expires_at > datetime('now'))",
        [sharedCallback, sharedReq, sharedJson, db, id, clientIp](const Result &result) {
            if (result.empty()) {
                Json::Value ret; ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Require password for protected pastes
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    recordAuthFailure(*clientIp);
                    Json::Value ret; ret["error"] = "Paste not found"; ret["password_required"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k404NotFound); addSecurityHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
                clearAuthFailures(*clientIp);
            }

            // Build update SQL dynamically
            std::string title = row["title"].as<std::string>();
            std::string content = row["content"].as<std::string>();
            std::string language = row["language"].as<std::string>();
            std::string visibility = row["visibility"].isNull() ? "public" : row["visibility"].as<std::string>();

            if (sharedJson->isMember("title")) {
                title = (*sharedJson)["title"].asString();
                if (title.size() > MAX_TITLE_LENGTH) title = title.substr(0, MAX_TITLE_LENGTH);
            }
            if (sharedJson->isMember("content")) {
                content = (*sharedJson)["content"].asString();
                if (content.empty()) {
                    Json::Value ret; ret["error"] = "Content cannot be empty";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k400BadRequest); addSecurityHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
                if (content.size() > MAX_PASTE_SIZE) {
                    Json::Value ret; ret["error"] = "Paste too large. Max size is 512KB.";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k413RequestEntityTooLarge); addSecurityHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
            }
            if (sharedJson->isMember("language")) {
                language = (*sharedJson)["language"].asString();
                if (language.size() > MAX_LANGUAGE_LENGTH) language = "plaintext";
            }
            if (sharedJson->isMember("visibility")) {
                std::string v = (*sharedJson)["visibility"].asString();
                if (v == "public" || v == "unlisted" || v == "private") visibility = v;
            }

            db->execSqlAsync(
                "UPDATE pastes SET title = ?, content = ?, language = ?, visibility = ? WHERE id = ?",
                [sharedCallback, sharedJson, db, id, title, content, language, visibility](const Result &) {
                    // Handle tags update if provided
                    if (sharedJson->isMember("tags") && (*sharedJson)["tags"].isArray()) {
                        db->execSqlAsync("DELETE FROM tags WHERE paste_id = ?",
                            [sharedCallback, sharedJson, db, id, title, content, language, visibility](const Result &) {
                                auto newTags = (*sharedJson)["tags"];
                                auto remaining = std::make_shared<int>(newTags.size());
                                if (newTags.empty()) {
                                    Json::Value ret;
                                    ret["id"] = id; ret["title"] = title; ret["language"] = language;
                                    ret["visibility"] = visibility; ret["message"] = "Paste updated";
                                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                                    addSecurityHeaders(resp);
                                    (*sharedCallback)(resp);
                                    return;
                                }
                                int tagCount = 0;
                                for (const auto &tag : newTags) {
                                    if (tagCount >= static_cast<int>(MAX_TAGS_COUNT)) break;
                                    std::string t = tag.asString();
                                    if (t.empty() || t.size() > MAX_TAG_LENGTH) { (*remaining)--; continue; }
                                    tagCount++;
                                    db->execSqlAsync("INSERT INTO tags (paste_id, tag) VALUES (?, ?)",
                                        [remaining, sharedCallback, id, title, language, visibility](const Result &) {
                                            (*remaining)--;
                                            if (*remaining <= 0) {
                                                Json::Value ret;
                                                ret["id"] = id; ret["title"] = title; ret["language"] = language;
                                                ret["visibility"] = visibility; ret["message"] = "Paste updated";
                                                auto resp = HttpResponse::newHttpJsonResponse(ret);
                                                addSecurityHeaders(resp);
                                                (*sharedCallback)(resp);
                                            }
                                        },
                                        [remaining, sharedCallback](const DrogonDbException &) {
                                            (*remaining)--;
                                        }, id, t);
                                }
                            },
                            [sharedCallback](const DrogonDbException &) {
                                Json::Value ret; ret["error"] = "Database error";
                                auto resp = HttpResponse::newHttpJsonResponse(ret);
                                resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
                                (*sharedCallback)(resp);
                            }, id);
                    } else {
                        Json::Value ret;
                        ret["id"] = id; ret["title"] = title; ret["language"] = language;
                        ret["visibility"] = visibility; ret["message"] = "Paste updated";
                        auto resp = HttpResponse::newHttpJsonResponse(ret);
                        addSecurityHeaders(resp);
                        (*sharedCallback)(resp);
                    }
                },
                [sharedCallback](const DrogonDbException &e) {
                    Json::Value ret; ret["error"] = "Internal server error";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
                    (*sharedCallback)(resp);
                },
                title, content, language, visibility, id);
        },
        [sharedCallback](const DrogonDbException &e) {
            Json::Value ret; ret["error"] = "Internal server error";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError); addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        }, id);
}

// ─── HEALTH CHECK ───────────────────────────────────────────────────────────
void PasteController::healthCheck(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

    auto db = app().getDbClient();
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));

    db->execSqlAsync(
        "SELECT COUNT(*) as count FROM pastes",
        [sharedCallback](const Result &result) {
            Json::Value ret;
            ret["status"] = "ok";
            ret["db"] = "connected";
            ret["paste_count"] = result[0]["count"].as<int64_t>();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        },
        [sharedCallback](const DrogonDbException &) {
            Json::Value ret;
            ret["status"] = "error";
            ret["db"] = "disconnected";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k503ServiceUnavailable);
            addSecurityHeaders(resp);
            (*sharedCallback)(resp);
        });
}
