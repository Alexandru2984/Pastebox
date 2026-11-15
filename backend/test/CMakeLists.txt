# Generated Drogon harness retained for compatibility; production regressions
# are registered with CTest in ../CMakeLists.txt.
cmake_minimum_required(VERSION 3.16)
project(backend_test CXX)
add_executable(${PROJECT_NAME} test_main.cc)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon sqlite3 brotlidec brotlienc)
ParseAndAddDrogonTests(${PROJECT_NAME})
