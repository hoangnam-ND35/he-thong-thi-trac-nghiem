#include "app.h"

#include <stdio.h>
#include <string.h>

typedef struct Throttle {
    char key[160];
    int fails;
    long long until;
    int used;
} Throttle;

static Throttle g_throttle[64];

static Throttle* throttle_slot(const char* key, int create) {
    int i;
    int free_slot = -1;
    for (i = 0; i < 64; i++) {
        if (g_throttle[i].used && strcmp(g_throttle[i].key, key) == 0) return &g_throttle[i];
        if (!g_throttle[i].used && free_slot < 0) free_slot = i;
    }
    if (!create || free_slot < 0) return NULL;
    memset(&g_throttle[free_slot], 0, sizeof(g_throttle[free_slot]));
    g_throttle[free_slot].used = 1;
    copy_str(g_throttle[free_slot].key, sizeof(g_throttle[free_slot].key), key);
    return &g_throttle[free_slot];
}

static int throttle_blocked(const char* key) {
    Throttle* slot = throttle_slot(key, 0);
    if (!slot) return 0;
    if (slot->until > now_sec()) return 1;
    return 0;
}

static void throttle_fail(const char* key) {
    Throttle* slot = throttle_slot(key, 1);
    if (!slot) return;
    if (slot->until && slot->until <= now_sec()) slot->fails = 0;
    slot->fails++;
    if (slot->fails >= 8) slot->until = now_sec() + 60;
}

static void throttle_clear(const char* key) {
    Throttle* slot = throttle_slot(key, 0);
    if (slot) memset(slot, 0, sizeof(*slot));
}

int role_is(const char* role, const char* allowed) {
    char buf[64];
    char* token;
    if (!role || !allowed) return 0;
    copy_str(buf, sizeof(buf), allowed);
    token = strtok(buf, ",");
    while (token) {
        if (strcmp(token, role) == 0) return 1;
        token = strtok(NULL, ",");
    }
    return 0;
}

static void fill_actor(sqlite3_stmt* stmt, Actor* actor) {
    memset(actor, 0, sizeof(*actor));
    actor->user_id = db_int(stmt, 0);
    copy_str(actor->username, sizeof(actor->username), db_text(stmt, 1));
    copy_str(actor->role, sizeof(actor->role), db_text(stmt, 2));
    copy_str(actor->account_status, sizeof(actor->account_status), db_text(stmt, 3));
    actor->must_change = db_int(stmt, 4);
    actor->profile_id = db_int(stmt, 5);
    copy_str(actor->full_name, sizeof(actor->full_name), db_text(stmt, 6));
    copy_str(actor->email, sizeof(actor->email), db_text(stmt, 7));
    copy_str(actor->phone, sizeof(actor->phone), db_text(stmt, 8));
    copy_str(actor->status, sizeof(actor->status), db_text(stmt, 9));
    actor->student_id = db_int(stmt, 10);
    copy_str(actor->student_code, sizeof(actor->student_code), db_text(stmt, 11));
    copy_str(actor->dob, sizeof(actor->dob), db_text(stmt, 12));
    copy_str(actor->gender, sizeof(actor->gender), db_text(stmt, 13));
    copy_str(actor->class_name, sizeof(actor->class_name), db_text(stmt, 14));
    copy_str(actor->faculty, sizeof(actor->faculty), db_text(stmt, 15));
    actor->lecturer_id = db_int(stmt, 16);
    copy_str(actor->lecturer_code, sizeof(actor->lecturer_code), db_text(stmt, 17));
    copy_str(actor->department, sizeof(actor->department), db_text(stmt, 18));
    if (actor->lecturer_id) copy_str(actor->faculty, sizeof(actor->faculty), db_text(stmt, 19));
}

int actor_load(const char* token, Actor* actor) {
    sqlite3_stmt* stmt;
    int ok = 0;
    if (!token || !token[0]) return 0;
    stmt = db_prep(
        "SELECT u.id, u.username, u.role, u.status, u.must_change, u.profile_id, "
        "p.full_name, p.email, p.phone, p.status, "
        "IFNULL(s.id,0), IFNULL(s.student_code,''), IFNULL(s.dob,''), IFNULL(s.gender,''), IFNULL(s.class_name,''), IFNULL(s.faculty,''), "
        "IFNULL(l.id,0), IFNULL(l.lecturer_code,''), IFNULL(l.department,''), IFNULL(l.faculty,'') "
        "FROM sessions se JOIN users u ON u.id=se.user_id JOIN profiles p ON p.id=u.profile_id "
        "LEFT JOIN students s ON s.user_id=u.id LEFT JOIN lecturers l ON l.user_id=u.id "
        "WHERE se.token=? AND se.expires_at>?");
    if (!stmt) return 0;
    db_bind_text(stmt, 1, token);
    sqlite3_bind_int64(stmt, 2, now_sec());
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        fill_actor(stmt, actor);
        ok = 1;
    }
    sqlite3_finalize(stmt);
    return ok;
}

int require_user(Request* request, Response* response, Actor* actor) {
    if (!actor_load(request->session, actor)) {
        reply_fail(response, 401, "Vui lòng đăng nhập");
        return 0;
    }
    if (strcmp(actor->account_status, "active") != 0 || strcmp(actor->status, "active") != 0) {
        reply_fail(response, 403, "Tài khoản đang bị khóa hoặc hồ sơ đã vô hiệu");
        return 0;
    }
    return 1;
}

int require_role(Request* request, Response* response, Actor* actor, const char* roles) {
    if (!require_user(request, response, actor)) return 0;
    if (!role_is(actor->role, roles)) {
        reply_fail(response, 403, "Bạn không có quyền thực hiện");
        return 0;
    }
    return 1;
}

int owns_subject(const Actor* actor, int subject_id) {
    if (!actor) return 0;
    if (strcmp(actor->role, "admin") == 0) return scalar_int("SELECT COUNT(*) FROM subjects WHERE id=?", subject_id, -1) > 0;
    if (strcmp(actor->role, "lecturer") != 0) return 0;
    return scalar_int("SELECT COUNT(*) FROM subjects WHERE id=? AND lecturer_id=?", subject_id, actor->lecturer_id) > 0;
}

void write_user(W* w, int user_id) {
    sqlite3_stmt* stmt = db_prep(
        "SELECT u.id, u.username, u.role, u.status, u.must_change, u.profile_id, "
        "p.full_name, p.email, p.phone, p.status, p.avatar, "
        "IFNULL(s.id,0), IFNULL(s.student_code,''), IFNULL(s.dob,''), IFNULL(s.gender,''), IFNULL(s.class_name,''), IFNULL(s.faculty,''), "
        "IFNULL(l.id,0), IFNULL(l.lecturer_code,''), IFNULL(l.department,''), IFNULL(l.faculty,'') "
        "FROM users u JOIN profiles p ON p.id=u.profile_id "
        "LEFT JOIN students s ON s.user_id=u.id LEFT JOIN lecturers l ON l.user_id=u.id WHERE u.id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, user_id);
    w_obj(w);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        int student_id = db_int(stmt, 11);
        int lecturer_id = db_int(stmt, 17);
        w_key(w, "id");
        w_num(w, db_int(stmt, 0));
        w_key(w, "username");
        w_str(w, db_text(stmt, 1));
        w_key(w, "role");
        w_str(w, db_text(stmt, 2));
        w_key(w, "fullName");
        w_str(w, db_text(stmt, 6));
        w_key(w, "email");
        w_str(w, db_text(stmt, 7));
        w_key(w, "phone");
        w_str(w, db_text(stmt, 8));
        w_key(w, "avatar");
        w_str(w, db_text(stmt, 10));
        w_key(w, "status");
        w_str(w, db_text(stmt, 9));
        w_key(w, "accountStatus");
        w_str(w, db_text(stmt, 3));
        w_key(w, "mustChangePassword");
        w_bool(w, db_int(stmt, 4));
        if (student_id) {
            w_key(w, "studentId");
            w_num(w, student_id);
            w_key(w, "studentCode");
            w_str(w, db_text(stmt, 12));
            w_key(w, "dob");
            w_str(w, db_text(stmt, 13));
            w_key(w, "gender");
            w_str(w, db_text(stmt, 14));
            w_key(w, "className");
            w_str(w, db_text(stmt, 15));
            w_key(w, "faculty");
            w_str(w, db_text(stmt, 16));
        }
        if (lecturer_id) {
            w_key(w, "lecturerId");
            w_num(w, lecturer_id);
            w_key(w, "lecturerCode");
            w_str(w, db_text(stmt, 18));
            w_key(w, "department");
            w_str(w, db_text(stmt, 19));
            w_key(w, "faculty");
            w_str(w, db_text(stmt, 20));
        }
    }
    sqlite3_finalize(stmt);
    w_end(w);
}

const char* make_session(int user_id) {
    static char token[80];
    sqlite3_stmt* stmt;
    random_hex(24, token);
    stmt = db_prep("INSERT INTO sessions(token, user_id, expires_at) VALUES(?,?,?)");
    if (!stmt) return "";
    db_bind_text(stmt, 1, token);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, now_sec() + 8 * 3600);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return token;
}

static int find_user_by_name(const char* username, char* hash, char* salt, int* user_id, char* status, char* profile_status) {
    sqlite3_stmt* stmt = db_prep(
        "SELECT u.id, u.password_hash, u.salt, u.status, p.status FROM users u "
        "JOIN profiles p ON p.id=u.profile_id WHERE u.username=?");
    int found = 0;
    if (!stmt) return 0;
    db_bind_text(stmt, 1, username);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *user_id = db_int(stmt, 0);
        copy_str(hash, 65, db_text(stmt, 1));
        copy_str(salt, 48, db_text(stmt, 2));
        copy_str(status, 24, db_text(stmt, 3));
        copy_str(profile_status, 24, db_text(stmt, 4));
        found = 1;
    }
    sqlite3_finalize(stmt);
    return found;
}

void route_login(Request* request, Response* response) {
    char username[64];
    char key[160];
    char hash[65];
    char salt[48];
    char status[24];
    char profile_status[24];
    char expect[65];
    int user_id = 0;
    const char* password = js_str(request->json, "password", "");
    trim_copy(username, sizeof(username), js_str(request->json, "username", ""));
    snprintf(key, sizeof(key), "%s|%s", request->ip, username);
    if (throttle_blocked(key)) {
        reply_fail(response, 429, "Đăng nhập bị tạm khóa, thử lại sau ít giây");
        return;
    }
    if (!find_user_by_name(username, hash, salt, &user_id, status, profile_status)) {
        throttle_fail(key);
        reply_fail(response, 401, "Sai tên đăng nhập hoặc mật khẩu");
        return;
    }
    hash_password(salt, password, expect);
    if (!constant_equal(hash, expect)) {
        throttle_fail(key);
        reply_fail(response, 401, "Sai tên đăng nhập hoặc mật khẩu");
        return;
    }
    if (strcmp(status, "active") != 0 || strcmp(profile_status, "active") != 0) {
        reply_fail(response, 403, "Tài khoản đang bị khóa hoặc hồ sơ đã vô hiệu");
        return;
    }
    throttle_clear(key);
    {
        W w;
        const char* token = make_session(user_id);
        audit_add(user_id, "LOGIN", "Đăng nhập thành công", request->ip);
        reply_begin(&w);
        write_user(&w, user_id);
        reply_json(response, &w);
        cookie_session(response, token, 8 * 3600);
    }
}

void route_logout(Request* request, Response* response) {
    sqlite3_stmt* stmt = db_prep("SELECT user_id FROM sessions WHERE token=?");
    int user_id = 0;
    W w;
    if (stmt) {
        db_bind_text(stmt, 1, request->session);
        if (sqlite3_step(stmt) == SQLITE_ROW) user_id = db_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    stmt = db_prep("DELETE FROM sessions WHERE token=?");
    if (stmt) {
        db_bind_text(stmt, 1, request->session);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (user_id) audit_add(user_id, "LOGOUT", "Đăng xuất", request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
    cookie_session(response, "", 0);
}

void route_me(Request* request, Response* response) {
    Actor actor;
    W w;
    if (!require_user(request, response, &actor)) return;
    reply_begin(&w);
    write_user(&w, actor.user_id);
    reply_json(response, &w);
}

void route_change_password(Request* request, Response* response) {
    Actor actor;
    char hash[65];
    char salt[48];
    char expect[65];
    char new_salt[40];
    char new_hash[65];
    const char* problem;
    sqlite3_stmt* stmt;
    W w;
    if (!require_user(request, response, &actor)) return;
    stmt = db_prep("SELECT password_hash, salt FROM users WHERE id=?");
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không tìm thấy tài khoản");
        return;
    }
    copy_str(hash, sizeof(hash), db_text(stmt, 0));
    copy_str(salt, sizeof(salt), db_text(stmt, 1));
    sqlite3_finalize(stmt);
    hash_password(salt, js_str(request->json, "oldPassword", ""), expect);
    if (!constant_equal(hash, expect)) {
        reply_fail(response, 400, "Mật khẩu hiện tại không đúng");
        return;
    }
    problem = password_problem(js_str(request->json, "newPassword", ""));
    if (problem) {
        reply_fail(response, 400, problem);
        return;
    }
    random_hex(16, new_salt);
    hash_password(new_salt, js_str(request->json, "newPassword", ""), new_hash);
    stmt = db_prep("UPDATE users SET password_hash=?, salt=?, must_change=0 WHERE id=?");
    if (!stmt) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    db_bind_text(stmt, 1, new_hash);
    db_bind_text(stmt, 2, new_salt);
    sqlite3_bind_int(stmt, 3, actor.user_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    audit_add(actor.user_id, "CHANGE_PASSWORD", "Đổi mật khẩu", request->ip);
    reply_begin(&w);
    write_user(&w, actor.user_id);
    reply_json(response, &w);
}

void route_register(Request* request, Response* response) {
    char key[80];
    char err[240];
    int user_id = 0;
    int status;
    W w;
    snprintf(key, sizeof(key), "%s|register", request->ip);
    if (throttle_blocked(key)) {
        reply_fail(response, 429, "Đăng ký tạm khóa, thử lại sau ít giây");
        return;
    }
    status = create_account(request->json, 1, &user_id, err, sizeof(err));
    if (status) {
        throttle_fail(key);
        reply_fail(response, status, err);
        return;
    }
    throttle_clear(key);
    {
        const char* token = make_session(user_id);
        audit_add(user_id, "REGISTER", "Học sinh tự đăng ký", request->ip);
        reply_begin(&w);
        write_user(&w, user_id);
        reply_json(response, &w);
        cookie_session(response, token, 8 * 3600);
    }
}

void route_register_options(Request* request, Response* response) {
    sqlite3_stmt* stmt;
    W w;
    (void)request;
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "faculties");
    w_arr(&w);
    stmt = db_prep("SELECT DISTINCT faculty FROM classes ORDER BY faculty");
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) w_str(&w, db_text(stmt, 0));
    sqlite3_finalize(stmt);
    w_end(&w);
    w_key(&w, "classes");
    w_arr(&w);
    stmt = db_prep("SELECT name, faculty FROM classes ORDER BY name");
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "name");
        w_str(&w, db_text(stmt, 0));
        w_key(&w, "faculty");
        w_str(&w, db_text(stmt, 1));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    w_end(&w);
    reply_json(response, &w);
}
