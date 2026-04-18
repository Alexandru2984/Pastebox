#include "PasteController.h"
#include <drogon/orm/DbClient.h>
#include <random>

using namespace drogon;
using namespace drogon::orm;

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
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void PasteController::createPaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

    if (req->method() == Options) {
        auto resp = HttpResponse::newHttpResponse();
        addCorsHeaders(resp);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpJsonResponse(
            Json::Value("Invalid JSON body"));
        resp->setStatusCode(k400BadRequest);
        addCorsHeaders(resp);
        callback(resp);
        return;
    }

    std::string title = (*json).get("title", "Untitled").asString();
    std::string content = (*json).get("content", "").asString();
    std::string language = (*json).get("language", "plaintext").asString();

    if (content.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(
            Json::Value("Content cannot be empty"));
        resp->setStatusCode(k400BadRequest);
        addCorsHeaders(resp);
        callback(resp);
        return;
    }

    std::string id = generateId();
    auto db = app().getDbClient();

    db->execSqlAsync(
        "INSERT INTO pastes (id, title, content, language) VALUES (?, ?, ?, ?)",
        [callback, id, title, language](const Result &result) {
            Json::Value ret;
            ret["id"] = id;
            ret["title"] = title;
            ret["language"] = language;
            ret["message"] = "Paste created successfully";
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k201Created);
            addCorsHeaders(resp);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value ret;
            ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError);
            addCorsHeaders(resp);
            callback(resp);
        },
        id, title, content, language);
}

void PasteController::listPastes(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

    if (req->method() == Options) {
        auto resp = HttpResponse::newHttpResponse();
        addCorsHeaders(resp);
        callback(resp);
        return;
    }

    auto db = app().getDbClient();
    db->execSqlAsync(
        "SELECT id, title, language, created_at, length(content) as size "
        "FROM pastes ORDER BY created_at DESC LIMIT 50",
        [callback](const Result &result) {
            Json::Value ret(Json::arrayValue);
            for (const auto &row : result) {
                Json::Value paste;
                paste["id"] = row["id"].as<std::string>();
                paste["title"] = row["title"].as<std::string>();
                paste["language"] = row["language"].as<std::string>();
                paste["created_at"] = row["created_at"].as<std::string>();
                paste["size"] = row["size"].as<int64_t>();
                ret.append(paste);
            }
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            addCorsHeaders(resp);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value ret;
            ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError);
            addCorsHeaders(resp);
            callback(resp);
        });
}

void PasteController::getPaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    if (req->method() == Options) {
        auto resp = HttpResponse::newHttpResponse();
        addCorsHeaders(resp);
        callback(resp);
        return;
    }

    auto db = app().getDbClient();

    // Increment views
    db->execSqlAsync(
        "UPDATE pastes SET views = views + 1 WHERE id = ?",
        [](const Result &) {},
        [](const DrogonDbException &) {},
        id);

    db->execSqlAsync(
        "SELECT id, title, content, language, created_at, views "
        "FROM pastes WHERE id = ?",
        [callback](const Result &result) {
            if (result.empty()) {
                Json::Value ret;
                ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound);
                addCorsHeaders(resp);
                callback(resp);
                return;
            }
            const auto &row = result[0];
            Json::Value ret;
            ret["id"] = row["id"].as<std::string>();
            ret["title"] = row["title"].as<std::string>();
            ret["content"] = row["content"].as<std::string>();
            ret["language"] = row["language"].as<std::string>();
            ret["created_at"] = row["created_at"].as<std::string>();
            ret["views"] = row["views"].as<int64_t>();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            addCorsHeaders(resp);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value ret;
            ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError);
            addCorsHeaders(resp);
            callback(resp);
        },
        id);
}

void PasteController::deletePaste(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &id) {

    if (req->method() == Options) {
        auto resp = HttpResponse::newHttpResponse();
        addCorsHeaders(resp);
        callback(resp);
        return;
    }

    auto db = app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM pastes WHERE id = ?",
        [callback, id](const Result &result) {
            if (result.affectedRows() == 0) {
                Json::Value ret;
                ret["error"] = "Paste not found";
                auto resp = HttpResponse::newHttpJsonResponse(ret);
                resp->setStatusCode(k404NotFound);
                addCorsHeaders(resp);
                callback(resp);
                return;
            }
            Json::Value ret;
            ret["message"] = "Paste deleted";
            ret["id"] = id;
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            addCorsHeaders(resp);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value ret;
            ret["error"] = std::string("Database error: ") + e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(ret);
            resp->setStatusCode(k500InternalServerError);
            addCorsHeaders(resp);
            callback(resp);
        },
        id);
}
