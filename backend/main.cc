#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <iostream>

void initDatabase() {
    auto db = drogon::app().getDbClient();
    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS pastes ("
        "  id TEXT PRIMARY KEY,"
        "  title TEXT NOT NULL DEFAULT 'Untitled',"
        "  content TEXT NOT NULL,"
        "  language TEXT NOT NULL DEFAULT 'plaintext',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  views INTEGER DEFAULT 0"
        ")");
    std::cout << "[PasteBox] Database initialized." << std::endl;
}

int main() {
    drogon::app().loadConfigFile("../config.json");

    drogon::app().registerBeginningAdvice([]() {
        initDatabase();
    });

    // Fallback: serve index.html for SPA routes
    drogon::app().setCustom404Page(
        drogon::HttpResponse::newFileResponse("./public/index.html"),
        false);

    std::cout << "[PasteBox] Starting on http://localhost:8080" << std::endl;
    drogon::app().run();
    return 0;
}
