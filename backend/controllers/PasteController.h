#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class PasteController : public HttpController<PasteController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PasteController::createPaste, "/api/pastes", Post, Options);
    ADD_METHOD_TO(PasteController::listPastes, "/api/pastes", Get, Options);
    ADD_METHOD_TO(PasteController::getPaste, "/api/pastes/{id}", Get, Options);
    ADD_METHOD_TO(PasteController::deletePaste, "/api/pastes/{id}", Delete, Options);
    METHOD_LIST_END

    void createPaste(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    void listPastes(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    void getPaste(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback,
                  const std::string &id);
    void deletePaste(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     const std::string &id);

private:
    static std::string generateId(int length = 8);
    static void addCorsHeaders(const HttpResponsePtr &resp);
};
