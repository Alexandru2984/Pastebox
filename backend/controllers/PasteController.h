#pragma once

#include <drogon/HttpController.h>
#include <string>

using namespace drogon;

class PasteController : public HttpController<PasteController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PasteController::createPaste, "/api/pastes", Post);
    ADD_METHOD_TO(PasteController::listPastes, "/api/pastes", Get);
    ADD_METHOD_TO(PasteController::getPaste, "/api/pastes/{id}", Get);
    ADD_METHOD_TO(PasteController::updatePaste, "/api/pastes/{id}", Put);
    ADD_METHOD_TO(PasteController::deletePaste, "/api/pastes/{id}", Delete);
    ADD_METHOD_TO(PasteController::getRawPaste, "/api/pastes/{id}/raw", Get);
    ADD_METHOD_TO(PasteController::forkPaste, "/api/pastes/{id}/fork", Post);
    ADD_METHOD_TO(PasteController::healthCheck, "/api/health", Get);
    METHOD_LIST_END

    void createPaste(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    void listPastes(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    void getPaste(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback,
                  const std::string &id);
    void updatePaste(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     const std::string &id);
    void deletePaste(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     const std::string &id);
    void getRawPaste(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     const std::string &id);
    void forkPaste(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   const std::string &id);
    void healthCheck(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

private:
    static std::string generateId(int length = 8);
    static void addCorsHeaders(const HttpResponsePtr &resp);
    static void addSecurityHeaders(const HttpResponsePtr &resp);
    static std::string sha256Hash(const std::string &input);
    static std::string hashPassword(const std::string &password);
    static Json::Value pasteRowToJson(const drogon::orm::Row &row, bool includeContent = true);
    static bool isExpired(const drogon::orm::Row &row);
    static bool checkPassword(const drogon::orm::Row &row, const std::string &password);
};
