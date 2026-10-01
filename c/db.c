#include "db.h"

#include "util.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

sqlite3* g_db = NULL;
static CRITICAL_SECTION g_db_lock;
static int g_lock_ready = 0;

int db_exec(const char* sql) {
    char* error = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &error);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL: %s\n%s\n", error ? error : sqlite3_errmsg(g_db), sql);
        sqlite3_free(error);
        return 0;
    }
    return 1;
}

sqlite3_stmt* db_prep(const char* sql) {
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL: %s\n%s\n", sqlite3_errmsg(g_db), sql);
        return NULL;
    }
    return stmt;
}

void db_bind_text(sqlite3_stmt* stmt, int index, const char* text) {
    sqlite3_bind_text(stmt, index, text ? text : "", -1, SQLITE_TRANSIENT);
}

int db_int(sqlite3_stmt* stmt, int column) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) return 0;
    return sqlite3_column_int(stmt, column);
}

long long db_i64(sqlite3_stmt* stmt, int column) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) return 0;
    return sqlite3_column_int64(stmt, column);
}

double db_real(sqlite3_stmt* stmt, int column) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) return 0;
    return sqlite3_column_double(stmt, column);
}

const char* db_text(sqlite3_stmt* stmt, int column) {
    const char* text = (const char*)sqlite3_column_text(stmt, column);
    return text ? text : "";
}

void db_begin(void) { db_exec("BEGIN IMMEDIATE"); }
void db_commit(void) { db_exec("COMMIT"); }
void db_rollback(void) { db_exec("ROLLBACK"); }

void db_lock(void) {
    if (g_lock_ready) EnterCriticalSection(&g_db_lock);
}

void db_unlock(void) {
    if (g_lock_ready) LeaveCriticalSection(&g_db_lock);
}

void audit_add(int user_id, const char* action, const char* detail, const char* ip) {
    sqlite3_stmt* stmt = db_prep("INSERT INTO audit_logs(user_id, action, detail, ip, created_at) VALUES(?,?,?,?,?)");
    if (!stmt) return;
    sqlite3_bind_int(stmt, 1, user_id);
    db_bind_text(stmt, 2, action);
    db_bind_text(stmt, 3, detail);
    db_bind_text(stmt, 4, ip ? ip : "");
    sqlite3_bind_int64(stmt, 5, now_sec());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void notify_user(int user_id, const char* message) {
    sqlite3_stmt* stmt = db_prep("INSERT INTO notifications(user_id, message, created_at) VALUES(?,?,?)");
    if (!stmt) return;
    sqlite3_bind_int(stmt, 1, user_id);
    db_bind_text(stmt, 2, message);
    sqlite3_bind_int64(stmt, 3, now_sec());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int db_count(const char* sql) {
    sqlite3_stmt* stmt = db_prep(sql);
    int value = 0;
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) value = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

int scalar_int(const char* sql, int a, int b) {
    sqlite3_stmt* stmt = db_prep(sql);
    int value = 0;
    if (!stmt) return 0;
    if (a >= 0) sqlite3_bind_int(stmt, 1, a);
    if (b >= 0) sqlite3_bind_int(stmt, 2, b);
    if (sqlite3_step(stmt) == SQLITE_ROW) value = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

static void ensure_column(const char* table, const char* name, const char* decl);

int db_open(void) {
    char dir_data[MAX_PATH];
    char dir_backup[MAX_PATH];
    char db_path[MAX_PATH];
    char schema_path[MAX_PATH];
    char* schema;
    path_under(dir_data, sizeof(dir_data), "data");
    path_under(dir_backup, sizeof(dir_backup), "data\\backups");
    path_under(db_path, sizeof(db_path), "data\\exam.db");
    path_under(schema_path, sizeof(schema_path), "sql\\schema.sql");
    ensure_dir(dir_data);
    ensure_dir(dir_backup);
    InitializeCriticalSection(&g_db_lock);
    g_lock_ready = 1;
    if (sqlite3_open_v2(db_path, &g_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
        fprintf(stderr, "Khong mo duoc %s\n", db_path);
        return 0;
    }
    schema = read_file(schema_path, NULL);
    if (!schema) {
        fprintf(stderr, "Thieu %s\n", schema_path);
        return 0;
    }
    if (!db_exec(schema)) {
        free(schema);
        return 0;
    }
    free(schema);
    ensure_column("questions", "topic", "TEXT NOT NULL DEFAULT ''");
    ensure_column("questions", "explanation", "TEXT NOT NULL DEFAULT ''");
    ensure_column("questions", "updated_at", "INTEGER NOT NULL DEFAULT 0");
    db_exec(
        "INSERT OR IGNORE INTO system_settings(key, value) VALUES"
        "('defaultDuration','45'),('defaultEasy','8'),('defaultMedium','8'),('defaultHard','4'),"
        "('autosaveSec','8'),('shortDisconnect','30'),('longDisconnect','300'),('syncGrace','300'),"
        "('schoolName','Phòng thi trực tuyến'),('schoolShort','THI'),('schoolLevel','university'),"
        "('schoolMotto','Trộn câu hỏi, trộn đáp án, tính giờ theo máy chủ và vẫn giữ bài khi mất kết nối.'),"
        "('schoolAddress',''),('schoolPhone',''),('schoolEmail',''),('schoolWebsite',''),"
        "('schoolTheme','navy'),('schoolLogo','')");
    return 1;
}

static void ensure_column(const char* table, const char* name, const char* decl) {
    char sql[220];
    sqlite3_stmt* stmt;
    int found = 0;
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table);
    stmt = db_prep(sql);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        if (strcmp(db_text(stmt, 1), name) == 0) found = 1;
    }
    sqlite3_finalize(stmt);
    if (found) return;
    snprintf(sql, sizeof(sql), "ALTER TABLE %s ADD COLUMN %s %s", table, name, decl);
    db_exec(sql);
}

void db_close(void) {
    if (g_db) sqlite3_close(g_db);
    g_db = NULL;
    if (g_lock_ready) DeleteCriticalSection(&g_db_lock);
    g_lock_ready = 0;
}
