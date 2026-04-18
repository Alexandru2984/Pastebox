#include "PasteController.h"
#include <drogon/orm/DbClient.h>
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>

using namespace drogon;
using namespace drogon::orm;

static constexpr size_t MAX_PASTE_SIZE = 512 * 1024;

std::string PasteController::generateId(int length) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);
    std::string id;
    id.reserve(length);
    for (int i = 0; i < length; ++i)
        id += chars[dist(rng)];
    return id;
}

void PasteController::addCorsHeaders(const HttpResponsePtr &resp) {
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Password");
}

std::string PasteController::sha256Hash(const std::string &input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

bool PasteController::isExpired(const Row &row) {
    if (row["expires_at"].isNull()) return false;
    auto expiresAt = row["expires_at"].as<std::string>();
    // SQLite datetime comparison: if expires_at < now, it's expired
    return false; // Actual check done in SQL query
}

bool PasteController::checkPassword(const Row &row, const std::string &password) {
    if (row["password_hash"].isNull()) return true; // no password set
    auto stored = row["password_hash"].as<std::string>();
    if (stored.empty()) return true;
    return sha256Hash(password) == stored;
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

    auto json = req->getJsonObject();
    if (!json) {
        Json::Value ret; ret["error"] = "Invalid JSON body";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k400BadRequest); addCorsHeaders(resp);
        callback(resp); return;
    }

    std::string content = (*json).get("content", "").asString();
    if (content.empty()) {
        Json::Value ret; ret["error"] = "Content cannot be empty";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k400BadRequest); addCorsHeaders(resp);
        callback(resp); return;
    }
    if (content.size() > MAX_PASTE_SIZE) {
        Json::Value ret; ret["error"] = "Paste too large. Max size is 512KB.";
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        resp->setStatusCode(k413RequestEntityTooLarge); addCorsHeaders(resp);
        callback(resp); return;
    }

    std::string id = generateId();
    std::string title = (*json).get("title", "Untitled").asString();
    std::string language = (*json).get("language", "plaintext").asString();
    std::string visibility = (*json).get("visibility", "public").asString();
    bool burnAfterRead = (*json).get("burn_after_read", false).asBool();
    std::string password = (*json).get("password", "").asString();
    std::string expiresIn = (*json).get("expires_in", "").asString();
    std::string parentId = (*json).get("parent_id", "").asString();

    // Validate visibility
    if (visibility != "public" && visibility != "private" && visibility != "unlisted")
        visibility = "public";

    // Hash password if provided
    std::string passwordHash = password.empty() ? "" : sha256Hash(password);

    // Calculate expires_at from expires_in
    std::string expiresAtExpr = "";
    if (expiresIn == "1h") expiresAtExpr = "datetime('now', '+1 hour')";
    else if (expiresIn == "24h") expiresAtExpr = "datetime('now', '+1 day')";
    else if (expiresIn == "7d") expiresAtExpr = "datetime('now', '+7 days')";
    else if (expiresIn == "30d") expiresAtExpr = "datetime('now', '+30 days')";

    // Extract tags
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
            // Insert tags
            for (const auto &tag : *sharedTags) {
                if (tag.isString() && !tag.asString().empty()) {
                    db->execSqlAsync(
                        "INSERT OR IGNORE INTO tags (paste_id, tag) VALUES (?, ?)",
                        [](const Result &) {},
                        [](const DrogonDbException &) {},
                        *sharedId, tag.asString());
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
            addCorsHeaders(resp);
            (*sharedCallback)(resp);
        },
        [sharedCallback](const DrogonDbException &e) {
            Json::Value ret;
            ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError);
            addCorsHeaders(resp);
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

    std::string sql;
    if (tag.empty()) {
        sql = "SELECT id, title, language, created_at, length(content) as size, "
              "visibility, burn_after_read, password_hash, expires_at, views "
              "FROM pastes WHERE visibility = 'public' "
              "AND (expires_at IS NULL OR expires_at > datetime('now')) "
              "ORDER BY created_at DESC LIMIT 50";
    } else {
        sql = "SELECT p.id, p.title, p.language, p.created_at, length(p.content) as size, "
              "p.visibility, p.burn_after_read, p.password_hash, p.expires_at, p.views "
              "FROM pastes p JOIN tags t ON p.id = t.paste_id "
              "WHERE p.visibility = 'public' "
              "AND (p.expires_at IS NULL OR p.expires_at > datetime('now')) "
              "AND t.tag = ? "
              "ORDER BY p.created_at DESC LIMIT 50";
    }

    auto fetchTags = [](const std::shared_ptr<DbClient> &db, const std::string &pid,
                        std::function<void(Json::Value)> cb) {
        db->execSqlAsync(
            "SELECT tag FROM tags WHERE paste_id = ?",
            [cb](const Result &r) {
                Json::Value tags(Json::arrayValue);
                for (const auto &row : r) tags.append(row["tag"].as<std::string>());
                cb(tags);
            },
            [cb](const DrogonDbException &) { cb(Json::Value(Json::arrayValue)); },
            pid);
    };

    auto handleResult = [sharedCallback, db, fetchTags](const Result &result) {
        if (result.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(Json::Value(Json::arrayValue));
            addCorsHeaders(resp);
            (*sharedCallback)(resp);
            return;
        }

        auto pastes = std::make_shared<Json::Value>(Json::arrayValue);
        auto remaining = std::make_shared<int>(result.size());

        for (size_t i = 0; i < result.size(); i++) {
            const auto &row = result[i];
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

            auto idx = std::make_shared<size_t>(i);
            auto pastePtr = std::make_shared<Json::Value>(paste);

            fetchTags(db, paste["id"].asString(), [pastes, remaining, sharedCallback, pastePtr, idx](Json::Value tags) {
                (*pastePtr)["tags"] = tags;
                (*pastes).append(*pastePtr);
                (*remaining)--;
                if (*remaining == 0) {
                    auto resp = HttpResponse::newHttpJsonResponse(*pastes);
                    addCorsHeaders(resp);
                    (*sharedCallback)(resp);
                }
            });
        }
    };

    if (tag.empty()) {
        db->execSqlAsync(sql, handleResult,
            [sharedCallback](const DrogonDbException &e) {
                Json::Value ret; ret["error"] = std::string("Database error: ") + e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k500InternalServerError); addCorsHeaders(resp);
                (*sharedCallback)(resp);
            });
    } else {
        db->execSqlAsync(sql, handleResult,
            [sharedCallback](const DrogonDbException &e) {
                Json::Value ret; ret["error"] = std::string("Database error: ") + e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k500InternalServerError); addCorsHeaders(resp);
                (*sharedCallback)(resp);
            }, tag);
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

    db->execSqlAsync(
        "SELECT * FROM pastes WHERE id = ? AND (expires_at IS NULL OR expires_at > datetime('now'))",
        [sharedCallback, sharedReq, db, id](const Result &result) {
            if (result.empty()) {
                Json::Value ret; ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound); addCorsHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Check password
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    Json::Value ret;
                    ret["error"] = "Password required";
                    ret["password_required"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k403Forbidden); addCorsHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
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
                    addCorsHeaders(resp);
                    (*sharedCallback)(resp);
                },
                [sharedCallback](const DrogonDbException &) {
                    Json::Value ret; ret["error"] = "Database error";
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k500InternalServerError); addCorsHeaders(resp);
                    (*sharedCallback)(resp);
                }, id);
        },
        [sharedCallback](const DrogonDbException &e) {
            Json::Value ret; ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError); addCorsHeaders(resp);
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

    db->execSqlAsync(
        "SELECT * FROM pastes WHERE id = ? AND (expires_at IS NULL OR expires_at > datetime('now'))",
        [sharedCallback, sharedReq](const Result &result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k404NotFound);
                resp->setContentTypeString("text/plain");
                resp->setBody("Paste not found");
                addCorsHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Check password
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k403Forbidden);
                    resp->setContentTypeString("text/plain");
                    resp->setBody("Password required");
                    addCorsHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
            }

            auto resp = HttpResponse::newHttpResponse();
            resp->setContentTypeString("text/plain; charset=utf-8");
            resp->setBody(row["content"].as<std::string>());
            addCorsHeaders(resp);
            (*sharedCallback)(resp);
        },
        [sharedCallback](const DrogonDbException &e) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k500InternalServerError);
            resp->setContentTypeString("text/plain");
            resp->setBody("Database error");
            addCorsHeaders(resp);
            (*sharedCallback)(resp);
        }, id);
}

// ─── FORK PASTE ─────────────────────────────────────────────────────────────
void PasteController::forkPaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    auto db = app().getDbClient();
    auto sharedCallback = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
    auto sharedReq = req;

    db->execSqlAsync(
        "SELECT * FROM pastes WHERE id = ? AND (expires_at IS NULL OR expires_at > datetime('now'))",
        [sharedCallback, sharedReq, db, id](const Result &result) {
            if (result.empty()) {
                Json::Value ret; ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound); addCorsHeaders(resp);
                (*sharedCallback)(resp); return;
            }
            const auto &row = result[0];

            // Check password
            if (!row["password_hash"].isNull() && !row["password_hash"].as<std::string>().empty()) {
                std::string providedPw = sharedReq->getHeader("X-Password");
                if (!checkPassword(row, providedPw)) {
                    Json::Value ret; ret["error"] = "Password required"; ret["password_required"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k403Forbidden); addCorsHeaders(resp);
                    (*sharedCallback)(resp); return;
                }
            }

            std::string newId = generateId();
            std::string title = "Fork of " + row["title"].as<std::string>();
            std::string content = row["content"].as<std::string>();
            std::string language = row["language"].as<std::string>();

            // Allow overriding title/content from request body
            auto json = sharedReq->getJsonObject();
            if (json) {
                if (json->isMember("title")) title = (*json)["title"].asString();
                if (json->isMember("content")) content = (*json)["content"].asString();
                if (json->isMember("language")) language = (*json)["language"].asString();
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
                    resp->setStatusCode(k201Created); addCorsHeaders(resp);
                    (*sharedCallback)(resp);
                },
                [sharedCallback](const DrogonDbException &e) {
                    Json::Value ret; ret["error"] = std::string("Database error: ") + e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(ret);
                    resp->setStatusCode(k500InternalServerError); addCorsHeaders(resp);
                    (*sharedCallback)(resp);
                },
                newId, title, content, language, id);
        },
        [sharedCallback](const DrogonDbException &e) {
            Json::Value ret; ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError); addCorsHeaders(resp);
            (*sharedCallback)(resp);
        }, id);
}

// ─── DELETE ─────────────────────────────────────────────────────────────────
void PasteController::deletePaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    auto db = app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM pastes WHERE id = ?",
        [callback, id](const Result &result) {
            if (result.affectedRows() == 0) {
                Json::Value ret; ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound); addCorsHeaders(resp);
                callback(resp); return;
            }
            Json::Value ret; ret["message"] = "Paste deleted"; ret["id"] = id;
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            addCorsHeaders(resp);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value ret; ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError); addCorsHeaders(resp);
            callback(resp);
        }, id);
}
