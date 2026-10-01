#ifndef OES_DB_H
#define OES_DB_H

#include "sqlite3.h"

extern sqlite3* g_db;

int db_open(void);
void db_close(void);
void db_lock(void);
void db_unlock(void);
int db_exec(const char* sql);
sqlite3_stmt* db_prep(const char* sql);
void db_bind_text(sqlite3_stmt* stmt, int index, const char* text);
int db_int(sqlite3_stmt* stmt, int column);
long long db_i64(sqlite3_stmt* stmt, int column);
double db_real(sqlite3_stmt* stmt, int column);
const char* db_text(sqlite3_stmt* stmt, int column);
void db_begin(void);
void db_commit(void);
void db_rollback(void);
void audit_add(int user_id, const char* action, const char* detail, const char* ip);
void notify_user(int user_id, const char* message);
int db_count(const char* sql);
int scalar_int(const char* sql, int a, int b);

#endif
