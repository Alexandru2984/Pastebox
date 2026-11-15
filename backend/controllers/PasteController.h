#pragma once
#include <drogon/HttpController.h>
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
    ADD_METHOD_TO(PasteController::embed, "/api/pastes/{id}/embed", Get);
    ADD_METHOD_TO(PasteController::ownership, "/api/pastes/{id}/ownership", Get);
    ADD_METHOD_TO(PasteController::listRevisions, "/api/pastes/{id}/revisions", Get);
    ADD_METHOD_TO(PasteController::getRevision, "/api/pastes/{id}/revisions/{revision}", Get);
    ADD_METHOD_TO(PasteController::restoreRevision, "/api/pastes/{id}/revisions/{revision}/restore", Post);
    ADD_METHOD_TO(PasteController::clearRevisions, "/api/pastes/{id}/revisions", Delete);
    ADD_METHOD_TO(PasteController::healthCheck, "/api/health", Get);
    METHOD_LIST_END
    using Callback = std::function<void(const HttpResponsePtr &)>;
    void createPaste(const HttpRequestPtr &, Callback &&);
    void listPastes(const HttpRequestPtr &, Callback &&);
    void getPaste(const HttpRequestPtr &, Callback &&, const std::string &);
    void updatePaste(const HttpRequestPtr &, Callback &&, const std::string &);
    void deletePaste(const HttpRequestPtr &, Callback &&, const std::string &);
    void getRawPaste(const HttpRequestPtr &, Callback &&, const std::string &);
    void forkPaste(const HttpRequestPtr &, Callback &&, const std::string &);
    void embed(const HttpRequestPtr &, Callback &&, const std::string &);
    void ownership(const HttpRequestPtr &, Callback &&, const std::string &);
    void listRevisions(const HttpRequestPtr &, Callback &&, const std::string &);
    void getRevision(const HttpRequestPtr &, Callback &&, const std::string &, const std::string &);
    void restoreRevision(const HttpRequestPtr &, Callback &&, const std::string &, const std::string &);
    void clearRevisions(const HttpRequestPtr &, Callback &&, const std::string &);
    void healthCheck(const HttpRequestPtr &, Callback &&);
};
