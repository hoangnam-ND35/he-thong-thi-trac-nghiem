cmake_minimum_required(VERSION 3.16)
project(OnlineExamSystem LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(online_exam
    third_party/sqlite-amalgamation-3460100/sqlite3.c
    c/json.c
    c/sha256.c
    c/util.c
    c/db.c
    c/http.c
    c/auth.c
    c/profile.c
    c/catalog.c
    c/question.c
    c/exam.c
    c/attempt.c
    c/dash.c
    c/ops.c
    c/seed.c
    c/routes.c
    c/main.c
)

target_include_directories(online_exam PRIVATE c third_party/sqlite-amalgamation-3460100)
target_compile_definitions(online_exam PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION _WIN32_WINNT=0x0601)

if(WIN32)
    target_link_libraries(online_exam PRIVATE ws2_32)
endif()
