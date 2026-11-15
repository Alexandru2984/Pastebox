#include "PasteController.h"
#include "../Security.h"
#include <drogon/orm/DbClient.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <charconv>
#include <algorithm>
#include <set>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace drogon::orm;
namespace {
constexpr size_t MAX_CONTENT = 512 * 1024;
constexpr int HISTORY_LIMIT = 10;
struct Failure { HttpStatusCode status; std::string message; bool password = false; bool owner = false; bool burn = false; std::string retryAfter = "60"; };
void fail(HttpStatusCode code, const std::string &message) { throw Failure{code, message}; }
HttpResponsePtr response(const Json::Value &json, HttpStatusCode code = k200OK) {
    auto result = HttpResponse::newHttpJsonResponse(json); result->setStatusCode(code); return result;
}
template<class Work> void handle(PasteController::Callback callback, Work work) {
    try { callback(work()); }
    catch (const Failure &e) {
        Json::Value json; json["error"] = e.message;
        if (e.password) json["password_required"] = true;
        if (e.owner) json["owner_required"] = true;
        if (e.burn) json["burn_confirmation_required"] = true;
        auto res = response(json, e.status);
        if (e.status == k429TooManyRequests) res->addHeader("Retry-After", e.retryAfter);
        callback(res);
    } catch (const std::exception &) {
        LOG_ERROR << "Paste operation failed";
        callback(security::error(k503ServiceUnavailable, "Storage temporarily unavailable. Please retry."));
    }
}
std::string hex(const unsigned char *data, size_t size) {
    const char *digits = "0123456789abcdef"; std::string out; out.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) { out += digits[data[i] >> 4]; out += digits[data[i] & 15]; }
    return out;
}
std::string randomToken(size_t bytes = 32) {
    std::vector<unsigned char> data(bytes);
    if (RAND_bytes(data.data(), static_cast<int>(bytes)) != 1) throw std::runtime_error("RNG failed");
    return hex(data.data(), data.size());
}
std::string digest(const std::string &value) {
    unsigned char bytes[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(value.data()), value.size(), bytes);
    return hex(bytes, sizeof(bytes));
}
bool equal(const std::string &a, const std::string &b) {
    return a.size() == b.size() && CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}
std::string derive(const std::string &password, const std::string &salt) {
    // An IP budget alone cannot bound CPU under distributed password traffic.
    static security::Limiter passwordWork;
    if (!passwordWork.allow("scrypt",4,8))
        throw Failure{k429TooManyRequests,"Password checks are busy. Please retry shortly.",false,false,false,"1"};
    unsigned char bytes[32];
    // OWASP's 16 MiB option: N=2^14, r=8, p=5. Fits the service's 128 MiB cap.
    if (EVP_PBE_scrypt(password.data(), password.size(), reinterpret_cast<const unsigned char *>(salt.data()),
        salt.size(), 16384, 8, 5, 32 * 1024 * 1024, bytes, sizeof(bytes)) != 1)
        throw std::runtime_error("Password derivation failed");
    return hex(bytes, sizeof(bytes));
}
std::string hashPassword(const std::string &password) {
    auto salt = randomToken(16); return "scrypt$16384$8$5$" + salt + "$" + derive(password, salt);
}
bool verifyPassword(const std::string &stored, const std::string &password) {
    if (stored.rfind("scrypt$16384$8$5$", 0) == 0) {
        const auto start = std::string("scrypt$16384$8$5$").size();
        const auto end = stored.find('$', start);
        if (end == std::string::npos || end - start != 32) return false;
        return equal(derive(password, stored.substr(start, end-start)), stored.substr(end+1));
    }
    const auto colon = stored.find(':');
    return colon == std::string::npos ? equal(digest(password), stored) :
        equal(digest(stored.substr(0, colon) + password), stored.substr(colon+1));
}
std::string value(const Row &row, const char *name) { return row[name].isNull() ? "" : row[name].as<std::string>(); }
bool owner(const Row &row, const HttpRequestPtr &req) {
    const auto &token = req->getHeader("X-Owner-Token");
    const auto stored = value(row, "owner_hash");
    return token.size() == 64 && !stored.empty() && equal(digest(token), stored);
}
void authorize(const Row &row, const HttpRequestPtr &req, bool write = false, const DbClientPtr &connection = nullptr) {
    if (owner(row, req)) return;
    if (write) throw Failure{k403Forbidden, "Owner key required. Read passwords do not grant edit access.", false, true};
    if (value(row, "visibility") == "private") throw Failure{k404NotFound, "Private paste: owner key required.", false, true};
    auto stored = value(row, "password_hash");
    if (stored.empty()) return;
    const auto &password = req->getHeader("X-Password");
    if (password.empty()) throw Failure{k404NotFound, "This paste requires a password.", true};
    if (password.size() > 256) fail(k400BadRequest, "Password is too long");
    static security::Limiter guesses;
    if (!guesses.allow(security::clientIp(req), 1.0 / 12, 5))
        fail(k429TooManyRequests, "Too many password attempts. Try again in a minute.");
    if (!verifyPassword(stored, password)) throw Failure{k404NotFound, "Incorrect password or unavailable paste.", true};
    if (stored.rfind("scrypt$", 0) != 0) {
        (connection ? connection : app().getDbClient())->execSqlSync("UPDATE pastes SET password_hash=? WHERE id=? AND password_hash=?",
                                       hashPassword(password), value(row, "id"), stored);
    }
}
Row find(const DbClientPtr &db, const std::string &id) {
    if (id.empty() || id.size() > 64 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos)
        fail(k404NotFound, "Paste not found");
    auto rows = db->execSqlSync("SELECT * FROM pastes WHERE id=? AND (expires_at IS NULL OR expires_at>datetime('now'))", id);
    if (rows.empty()) fail(k404NotFound, "Paste not found or expired");
    return rows[0];
}
Json::Value body(const HttpRequestPtr &req) {
    auto json = req->getJsonObject();
    if (!json || !json->isObject()) fail(k400BadRequest, "Expected a JSON object");
    const std::set<std::string> strings = {"title","content","language","visibility","password","expires_in"};
    const std::set<std::string> fields = {"title","content","language","visibility","password","expires_in","tags","burn_after_read"};
    for (const auto &key : json->getMemberNames()) {
        if (!fields.count(key)) fail(k400BadRequest, "Unknown field: " + key.substr(0,80));
        if (strings.count(key) && !(*json)[key].isString()) fail(k400BadRequest, key + " must be a string");
    }
    if (json->isMember("burn_after_read") && !(*json)["burn_after_read"].isBool()) fail(k400BadRequest, "burn_after_read must be a boolean");
    const std::vector<std::pair<std::string,size_t>> limits = {{"title",200},{"content",MAX_CONTENT},{"language",30},{"password",256},{"expires_in",10},{"visibility",10}};
    for (const auto &[key, max] : limits) if (json->isMember(key) && (*json)[key].asString().size() > max)
        fail(key == "content" ? k413RequestEntityTooLarge : k400BadRequest, key + " exceeds its size limit");
    if (json->isMember("content") && (*json)["content"].asString().empty()) fail(k400BadRequest, "Content cannot be empty");
    if (json->isMember("language")) {
        const auto lang = (*json)["language"].asString();
        if (lang.empty() || lang.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+-") != std::string::npos)
            fail(k400BadRequest, "Invalid language");
    }
    if (json->isMember("password") && !(*json)["password"].asString().empty() && (*json)["password"].asString().size() < 4)
        fail(k400BadRequest, "Password must have at least 4 bytes");
    if (json->isMember("visibility")) {
        auto v = (*json)["visibility"].asString();
        if (v != "public" && v != "unlisted" && v != "private") fail(k400BadRequest, "Invalid visibility");
    }
    if (json->isMember("expires_in")) {
        auto v = (*json)["expires_in"].asString();
        if (v != "" && v != "1h" && v != "24h" && v != "7d" && v != "30d") fail(k400BadRequest, "Invalid expiration");
    }
    if (json->isMember("tags")) {
        const auto &tags = (*json)["tags"];
        if (!tags.isArray() || tags.size() > 10) fail(k400BadRequest, "Use an array of at most 10 tags");
        for (const auto &tag : tags) {
            if (!tag.isString()) fail(k400BadRequest,"Tags must be strings");
            const auto text = tag.asString();
            if (text.empty() || text.size() > 50 || text.find(',') != std::string::npos ||
                std::any_of(text.begin(),text.end(),[](unsigned char c) { return c < 32 || c == 127; }))
                fail(k400BadRequest, "Tags must be nonempty strings of at most 50 bytes without commas or control characters");
        }
    }
    return *json;
}
std::string expiry(const Json::Value &json) {
    const auto in = json.get("expires_in", "").asString();
    if (in == "1h") return "+1 hour";
    if (in == "24h") return "+1 day";
    if (in == "7d") return "+7 days";
    if (in == "30d") return "+30 days";
    return "";
}
void tags(const DbClientPtr &db, const std::string &id, const Json::Value &json) {
    if (!json.isMember("tags")) return;
    db->execSqlSync("DELETE FROM tags WHERE paste_id=?", id);
    for (const auto &tag : json["tags"]) db->execSqlSync("INSERT OR IGNORE INTO tags(paste_id,tag) VALUES(?,?)", id, tag.asString());
}
Json::Value serialize(const Row &row, const DbClientPtr &db, bool includeContent = true) {
    Json::Value json;
    for (const auto *key : {"id","title","language","created_at","visibility"}) json[key] = value(row,key);
    for (const auto *key : {"expires_at","parent_id","updated_at"})
        json[key] = value(row,key).empty() ? Json::Value(Json::nullValue) : Json::Value(value(row,key));
    json["revision"] = row["revision"].as<int64_t>();
    json["views"] = row["views"].as<int64_t>();
    json["size"] = static_cast<Json::UInt64>(value(row,"content").size());
    json["has_password"] = !value(row,"password_hash").empty();
    json["burn_after_read"] = row["burn_after_read"].as<int>() != 0;
    if (includeContent) json["content"] = value(row,"content");
    Json::Value list(Json::arrayValue);
    auto rows = db->execSqlSync("SELECT tag FROM tags WHERE paste_id=? ORDER BY tag", value(row,"id"));
    for (const auto &tag : rows) list.append(value(tag,"tag"));
    json["tags"] = list;
    return json;
}
std::string revisionTag(const std::string &id, int64_t revision) {
    return "\"paste-" + id + "-v" + std::to_string(revision) + "\"";
}
void requireRevision(const HttpRequestPtr &req, const Row &row) {
    const auto &tag = req->getHeader("If-Match");
    if (tag.empty()) fail(k428PreconditionRequired,"Reload this paste before saving: If-Match with its current revision is required.");
    if (tag != revisionTag(value(row,"id"),row["revision"].as<int64_t>()))
        fail(k412PreconditionFailed,"A newer version was saved. Your changes have not been applied.");
}
HttpResponsePtr versioned(const Json::Value &json) {
    auto res = response(json);
    res->addHeader("ETag",revisionTag(json["id"].asString(),json["revision"].asInt64()));
    return res;
}
void historyAllowed(const Row &row) {
    if (row["burn_after_read"].as<int>() != 0) fail(k409Conflict,"One-time pastes do not keep revision history and cannot be edited.");
}
int revisionNumber(const std::string &text) {
    int number = 0;
    auto parsed = std::from_chars(text.data(),text.data()+text.size(),number);
    if (parsed.ec != std::errc() || parsed.ptr != text.data()+text.size() || number < 1)
        fail(k404NotFound,"Revision not found");
    return number;
}
void snapshot(const Row &row, const DbClientPtr &db) {
    auto json = serialize(row,db,false);
    Json::StreamWriterBuilder writer; writer["indentation"] = "";
    const std::string saved = value(row,"updated_at").empty() ? value(row,"created_at") : value(row,"updated_at");
    db->execSqlSync("INSERT INTO paste_revisions(paste_id,revision,title,content,language,tags_json,saved_at) VALUES(?,?,?,?,?,?,?)",
        value(row,"id"),row["revision"].as<int64_t>(),value(row,"title"),value(row,"content"),value(row,"language"),
        Json::writeString(writer,json["tags"]),saved);
    db->execSqlSync("DELETE FROM paste_revisions WHERE paste_id=? AND revision NOT IN "
        "(SELECT revision FROM paste_revisions WHERE paste_id=? ORDER BY revision DESC LIMIT ?)",value(row,"id"),value(row,"id"),HISTORY_LIMIT);
}
Json::Value parseTags(const std::string &text) {
    Json::CharReaderBuilder reader; std::istringstream input(text); Json::Value json; std::string errors;
    if (!Json::parseFromStream(reader,input,&json,&errors) || !json.isArray()) throw std::runtime_error("Invalid revision tags");
    return json;
}
// Commit is asynchronous in Drogon: send success only after its commit callback.
template<class Work> void transaction(PasteController::Callback callback, Work work) {
    std::shared_ptr<Transaction> tx;
    try {
        tx = app().getDbClient()->newTransaction();
        auto result = work(tx);
        tx->setCommitCallback([callback, result](bool ok) {
            callback(ok ? result : security::error(k503ServiceUnavailable, "Commit failed. Please retry."));
        });
        tx.reset();
    } catch (...) {
        if (tx) tx->rollback();
        tx.reset();
        auto error = std::current_exception();
        handle(std::move(callback), [error]() -> HttpResponsePtr { std::rethrow_exception(error); });
    }
}
Json::Value read(const HttpRequestPtr &req, const std::string &id, const DbClientPtr &db) {
    auto row = find(db,id); authorize(row,req,false,db);
    if (row["burn_after_read"].as<int>() != 0 && req->getHeader("X-Confirm-Burn") != "yes")
        throw Failure{k409Conflict,"This paste can only be opened once. Confirm to reveal it.",false,false,true};
    auto json = serialize(row,db);
    json["can_manage"] = owner(row,req);
    if (row["burn_after_read"].as<int>() != 0) {
        // Only the winner receives content. Raw and JSON share this path.
        auto deleted = db->execSqlSync("DELETE FROM pastes WHERE id=? AND burn_after_read=1 RETURNING id",id);
        if (deleted.empty()) fail(k404NotFound,"Paste has already been read");
        json["burned"] = true;
    } else {
        db->execSqlSync("UPDATE pastes SET views=views+1 WHERE id=?",id);
    }
    return json;
}
int integer(const HttpRequestPtr &req, const char *key, int fallback, int max) {
    auto text = req->getParameter(key); if (text.empty()) return fallback;
    int result = 0; auto parsed = std::from_chars(text.data(),text.data()+text.size(),result);
    if (parsed.ec != std::errc() || parsed.ptr != text.data()+text.size() || result < 1 || result > max)
        fail(k400BadRequest,std::string("Invalid ")+key);
    return result;
}
}

void PasteController::createPaste(const HttpRequestPtr &req, Callback &&callback) {
    try {
        auto json = body(req);
        if (!json.isMember("content")) fail(k400BadRequest,"Content is required");
        auto password = json.get("password", "").asString();
        auto hash = password.empty() ? "" : hashPassword(password);
        auto token = randomToken(); auto id = randomToken(8);
        transaction(std::move(callback), [json,hash,token,id](const DbClientPtr &db) {
            db->execSqlSync("INSERT INTO pastes(id,title,content,language,visibility,burn_after_read,password_hash,owner_hash,expires_at) "
                "VALUES(?,?,?,?,?,?,?,?,datetime('now',nullif(?,'')))", id,
                json.get("title","Untitled").asString(),json["content"].asString(),json.get("language","plaintext").asString(),
                json.get("visibility","public").asString(),json.get("burn_after_read",false).asBool() ? 1 : 0,hash,digest(token),expiry(json));
            tags(db,id,json);
            auto result = serialize(find(db,id),db,false); result["owner_token"] = token;
            return response(result,k201Created);
        });
    } catch (...) {
        auto error = std::current_exception();
        handle(std::move(callback),[error]() -> HttpResponsePtr {std::rethrow_exception(error);});
    }
}
void PasteController::getPaste(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    transaction(std::move(callback),[req,id](const DbClientPtr &db) {return versioned(read(req,id,db));});
}
void PasteController::getRawPaste(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    transaction(std::move(callback),[req,id](const DbClientPtr &db) {
        auto json = read(req,id,db); auto res = HttpResponse::newHttpResponse();
        res->setContentTypeString("text/plain; charset=utf-8"); res->setBody(json["content"].asString());
        res->addHeader("Content-Disposition","inline"); return res;
    });
}
void PasteController::deletePaste(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    transaction(std::move(callback),[req,id](const DbClientPtr &db) {
        auto row = find(db,id); authorize(row,req,true);
        db->execSqlSync("DELETE FROM pastes WHERE id=?",id);
        Json::Value json; json["id"] = id; json["message"] = "Paste deleted"; return response(json);
    });
}
void PasteController::updatePaste(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    try {
        auto json = body(req);
        for (const auto *key : {"password","expires_in","burn_after_read"})
            if (json.isMember(key)) fail(k400BadRequest,"This update field is not supported");
        transaction(std::move(callback),[req,id,json](const DbClientPtr &db) {
            auto row = find(db,id); authorize(row,req,true); historyAllowed(row); requireRevision(req,row);
            if (row["revision"].as<int64_t>() >= 2147483647) fail(k409Conflict,"Revision limit reached");
            snapshot(row,db);
            auto updated = db->execSqlSync("UPDATE pastes SET title=?,content=?,language=?,visibility=?,updated_at=CURRENT_TIMESTAMP,revision=revision+1 WHERE id=? AND revision=?",
                json.get("title",value(row,"title")).asString(),json.get("content",value(row,"content")).asString(),
                json.get("language",value(row,"language")).asString(),json.get("visibility",value(row,"visibility")).asString(),id,row["revision"].as<int64_t>());
            if (updated.affectedRows() != 1) fail(k412PreconditionFailed,"A newer version was saved. Your changes have not been applied.");
            tags(db,id,json); return versioned(serialize(find(db,id),db,false));
        });
    } catch (...) {
        auto error = std::current_exception();
        handle(std::move(callback),[error]() -> HttpResponsePtr {std::rethrow_exception(error);});
    }
}
void PasteController::forkPaste(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    try {
        auto json = body(req);
        if (json.isMember("password") || json.isMember("expires_in") || json.isMember("burn_after_read"))
            fail(k400BadRequest,"Forks inherit protection and expiration");
        const auto newId = randomToken(8), token = randomToken();
        transaction(std::move(callback),[req,id,json,newId,token](const DbClientPtr &db) {
            auto row = find(db,id); authorize(row,req,false,db);
            if (row["burn_after_read"].as<int>() != 0) fail(k409Conflict,"One-time pastes cannot be forked");
            auto visibility = value(row,"visibility");
            if (json.isMember("visibility")) {
                auto next = json["visibility"].asString();
                if (visibility != "public" && next == "public") fail(k400BadRequest,"A protected fork cannot be made public here");
                visibility = next;
            }
            const std::string parent = value(row,"visibility") == "public" && value(row,"password_hash").empty() ? id : "";
            db->execSqlSync("INSERT INTO pastes(id,title,content,language,visibility,password_hash,owner_hash,parent_id,expires_at) "
                "VALUES(?,?,?,?,?,?,?,?,nullif(?,''))",newId,
                json.get("title",(value(row,"title").size() <= 192 ? "Fork of " + value(row,"title") : value(row,"title"))).asString(),
                json.get("content",value(row,"content")).asString(),json.get("language",value(row,"language")).asString(),
                visibility,value(row,"password_hash"),digest(token),
                parent,value(row,"expires_at"));
            db->execSqlSync("INSERT INTO tags(paste_id,tag) SELECT ?,tag FROM tags WHERE paste_id=?",newId,id);
            tags(db,newId,json);
            auto result = serialize(find(db,newId),db,false); result["owner_token"] = token;
            return response(result,k201Created);
        });
    } catch (...) {
        auto error = std::current_exception();
        handle(std::move(callback),[error]() -> HttpResponsePtr {std::rethrow_exception(error);});
    }
}
void PasteController::listPastes(const HttpRequestPtr &req, Callback &&callback) {
    handle(std::move(callback),[req] {
        const int page = integer(req,"page",1,10000), limit = integer(req,"limit",50,100);
        const auto query = req->getParameter("q"), tag = req->getParameter("tag"), language = req->getParameter("language");
        if (query.size() > 100 || tag.size() > 50 || language.size() > 30) fail(k400BadRequest,"Search filter is too long");
        auto sort = req->getParameter("sort");
        const std::map<std::string,std::string> orders = {
            {"","p.created_at DESC,p.id DESC"},{"newest","p.created_at DESC,p.id DESC"},
            {"oldest","p.created_at ASC,p.id ASC"},{"popular","p.views DESC,p.created_at DESC,p.id DESC"}
        };
        auto order = orders.find(sort); if (order == orders.end()) fail(k400BadRequest,"Invalid sort order");
        // Search metadata only; never match password/private content.
        auto db = app().getDbClient();
        auto rows = db->execSqlSync(
            "SELECT p.id,p.title,p.language,p.created_at,length(CAST(p.content AS BLOB)) AS size,p.views,"
            "p.password_hash,p.burn_after_read,p.expires_at,"
            "(SELECT json_group_array(tag) FROM tags WHERE paste_id=p.id) AS tags "
            "FROM pastes p WHERE p.visibility='public' AND (p.expires_at IS NULL OR p.expires_at>datetime('now')) "
            "AND (?='' OR p.language=?) "
            "AND (?='' OR EXISTS(SELECT 1 FROM tags WHERE paste_id=p.id AND tag=?)) "
            "AND (?='' OR instr(lower(p.title),lower(?))>0 OR EXISTS(SELECT 1 FROM tags WHERE paste_id=p.id AND instr(lower(tag),lower(?))>0)) "
            "ORDER BY " + order->second + " LIMIT ? OFFSET ?",language,language,tag,tag,query,query,query,limit+1,(page-1)*limit);
        Json::Value data(Json::arrayValue);
        size_t count = std::min(rows.size(),static_cast<size_t>(limit));
        for (size_t i = 0; i < count; ++i) {
            auto row = rows[i]; Json::Value item;
            for (const auto *key : {"id","title","language","created_at"}) item[key] = value(row,key);
            item["visibility"] = "public"; item["size"] = row["size"].as<int64_t>(); item["views"] = row["views"].as<int64_t>();
            item["expires_at"] = value(row,"expires_at").empty() ? Json::Value(Json::nullValue) : Json::Value(value(row,"expires_at"));
            item["has_password"] = !value(row,"password_hash").empty(); item["burn_after_read"] = row["burn_after_read"].as<int>() != 0;
            Json::CharReaderBuilder builder; std::istringstream input(value(row,"tags")); std::string errors;
            if (!Json::parseFromStream(builder,input,&item["tags"],&errors)) item["tags"] = Json::Value(Json::arrayValue);
            data.append(item);
        }
        Json::Value result; result["data"] = data; result["page"] = page; result["limit"] = limit;
        result["count"] = static_cast<int>(count); result["has_more"] = rows.size() > static_cast<size_t>(limit);
        return response(result);
    });
}
void PasteController::healthCheck(const HttpRequestPtr &, Callback &&callback) {
    handle(std::move(callback),[] {
        app().getDbClient()->execSqlSync("SELECT 1"); Json::Value json; json["status"] = "ok"; return response(json);
    });
}

void PasteController::ownership(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    handle(std::move(callback),[req,id] {
        auto db = app().getDbClient(); auto row = find(db,id); authorize(row,req,true);
        auto json = serialize(row,db,false); json["can_manage"] = true; return response(json);
    });
}

void PasteController::embed(const HttpRequestPtr &, Callback &&callback, const std::string &id) {
    handle(std::move(callback),[id] {
        auto db = app().getDbClient(); auto row = find(db,id);
        if (value(row,"visibility") != "public" || !value(row,"password_hash").empty() || row["burn_after_read"].as<int>() != 0)
            fail(k404NotFound,"Only public, unprotected, reusable pastes can be embedded");
        return response(serialize(row,db));
    });
}

void PasteController::listRevisions(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    transaction(std::move(callback),[req,id](const DbClientPtr &db) {
        auto current = find(db,id); authorize(current,req,true); historyAllowed(current);
        auto rows = db->execSqlSync("SELECT revision,title,language,length(CAST(content AS BLOB)) AS size,saved_at "
            "FROM paste_revisions WHERE paste_id=? ORDER BY revision DESC LIMIT ?",id,HISTORY_LIMIT);
        Json::Value json; json["current_revision"] = current["revision"].as<int64_t>();
        json["retention_limit"] = HISTORY_LIMIT; json["data"] = Json::Value(Json::arrayValue);
        for (const auto &row : rows) {
            Json::Value item; item["revision"] = row["revision"].as<int64_t>(); item["size"] = row["size"].as<int64_t>();
            for (const auto *key : {"title","language","saved_at"}) item[key] = value(row,key);
            json["data"].append(item);
        }
        return response(json);
    });
}
void PasteController::getRevision(const HttpRequestPtr &req, Callback &&callback, const std::string &id, const std::string &revision) {
    transaction(std::move(callback),[req,id,revision](const DbClientPtr &db) {
        auto current = find(db,id); authorize(current,req,true); historyAllowed(current);
        auto rows = db->execSqlSync("SELECT * FROM paste_revisions WHERE paste_id=? AND revision=?",id,revisionNumber(revision));
        if (rows.empty()) fail(k404NotFound,"Revision not found");
        const auto row = rows[0]; Json::Value json; json["revision"] = row["revision"].as<int64_t>();
        for (const auto *key : {"title","content","language","saved_at"}) json[key] = value(row,key);
        json["tags"] = parseTags(value(row,"tags_json")); return response(json);
    });
}
void PasteController::restoreRevision(const HttpRequestPtr &req, Callback &&callback, const std::string &id, const std::string &revision) {
    try {
        auto json = body(req); if (!json.empty()) fail(k400BadRequest,"Restore accepts an empty JSON object");
        const int number = revisionNumber(revision);
        transaction(std::move(callback),[req,id,number](const DbClientPtr &db) {
            auto current = find(db,id); authorize(current,req,true); historyAllowed(current); requireRevision(req,current);
            if (current["revision"].as<int64_t>() >= 2147483647) fail(k409Conflict,"Revision limit reached");
            auto rows = db->execSqlSync("SELECT * FROM paste_revisions WHERE paste_id=? AND revision=?",id,number);
            if (rows.empty()) fail(k404NotFound,"Revision not found");
            const auto saved = rows[0]; snapshot(current,db);
            db->execSqlSync("UPDATE pastes SET title=?,content=?,language=?,updated_at=CURRENT_TIMESTAMP,revision=revision+1 WHERE id=?",
                value(saved,"title"),value(saved,"content"),value(saved,"language"),id);
            Json::Value tagUpdate; tagUpdate["tags"] = parseTags(value(saved,"tags_json")); tags(db,id,tagUpdate);
            return versioned(serialize(find(db,id),db,false));
        });
    } catch (...) {
        auto error = std::current_exception();
        handle(std::move(callback),[error]() -> HttpResponsePtr {std::rethrow_exception(error);});
    }
}
void PasteController::clearRevisions(const HttpRequestPtr &req, Callback &&callback, const std::string &id) {
    transaction(std::move(callback),[req,id](const DbClientPtr &db) {
        auto current = find(db,id); authorize(current,req,true); historyAllowed(current); requireRevision(req,current);
        db->execSqlSync("DELETE FROM paste_revisions WHERE paste_id=?",id);
        Json::Value json; json["cleared"] = true; json["current_revision"] = current["revision"].as<int64_t>();
        return response(json);
    });
}
