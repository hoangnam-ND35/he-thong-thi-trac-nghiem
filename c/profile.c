#include "app.h"

#include <stdio.h>
#include <string.h>

static int username_ok(const char* username) {
    size_t n = username ? strlen(username) : 0;
    size_t i;
    if (n < 3 || n > 32) return 0;
    for (i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)username[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_')) return 0;
    }
    return 1;
}

static int gender_ok(const char* gender) {
    return strcmp(gender, "Nam") == 0 || strcmp(gender, "Nữ") == 0 || strcmp(gender, "Khác") == 0;
}

static int class_exists(const char* name, const char* faculty) {
    sqlite3_stmt* stmt = db_prep("SELECT COUNT(*) FROM classes WHERE name=? AND faculty=?");
    int count = 0;
    if (!stmt) return 0;
    db_bind_text(stmt, 1, name);
    db_bind_text(stmt, 2, faculty);
    if (sqlite3_step(stmt) == SQLITE_ROW) count = db_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
}

static int taken(const char* sql, const char* value) {
    sqlite3_stmt* stmt = db_prep(sql);
    int count = 0;
    if (!stmt) return 1;
    db_bind_text(stmt, 1, value);
    if (sqlite3_step(stmt) == SQLITE_ROW) count = db_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
}

static int insert_user(const char* username, const char* password, const char* role, const char* full_name,
                       const char* email, const char* phone, int* user_id) {
    char salt[40];
    char hash[65];
    sqlite3_stmt* stmt;
    int profile_id;
    random_hex(16, salt);
    hash_password(salt, password, hash);
    stmt = db_prep("INSERT INTO profiles(user_id, role, full_name, email, phone, avatar, status) VALUES(0,?,?,?,?,'','active')");
    if (!stmt) return 0;
    db_bind_text(stmt, 1, role);
    db_bind_text(stmt, 2, full_name);
    db_bind_text(stmt, 3, email);
    db_bind_text(stmt, 4, phone);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    profile_id = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("INSERT INTO users(username, password_hash, salt, role, status, profile_id, created_at, must_change) VALUES(?,?,?,?,'active',?,?,0)");
    if (!stmt) return 0;
    db_bind_text(stmt, 1, username);
    db_bind_text(stmt, 2, hash);
    db_bind_text(stmt, 3, salt);
    db_bind_text(stmt, 4, role);
    sqlite3_bind_int(stmt, 5, profile_id);
    sqlite3_bind_int64(stmt, 6, now_sec());
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    *user_id = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("UPDATE profiles SET user_id=? WHERE id=?");
    if (!stmt) return 0;
    sqlite3_bind_int(stmt, 1, *user_id);
    sqlite3_bind_int(stmt, 2, profile_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return profile_id;
}

int create_account(const Js* body, int self_register, int* user_id, char* err, size_t err_cap) {
    char role[16];
    char username[64];
    char full_name[160];
    char email[160];
    char phone[40];
    char student_code[40];
    char dob[16];
    char gender[16];
    char class_name[64];
    char faculty[128];
    char lecturer_code[40];
    char department[128];
    const char* problem;
    int profile_id;
    sqlite3_stmt* stmt;
    copy_str(role, sizeof(role), js_str(body, "role", "student"));
    trim_copy(username, sizeof(username), js_str(body, "username", ""));
    trim_copy(full_name, sizeof(full_name), js_str(body, "fullName", ""));
    trim_copy(email, sizeof(email), js_str(body, "email", ""));
    trim_copy(phone, sizeof(phone), js_str(body, "phone", ""));
    if (self_register && strcmp(role, "student") != 0) {
        snprintf(err, err_cap, "Chỉ sinh viên được tự đăng ký. Giảng viên do quản trị tạo");
        return 400;
    }
    if (!role_is(role, "student,lecturer,admin")) {
        snprintf(err, err_cap, "Vai trò không hợp lệ");
        return 400;
    }
    if (!username_ok(username)) {
        snprintf(err, err_cap, "Tên đăng nhập cần 3 đến 32 ký tự, gồm chữ, số, dấu chấm hoặc gạch dưới");
        return 400;
    }
    problem = password_problem(js_str(body, "password", ""));
    if (problem) {
        snprintf(err, err_cap, "%s", problem);
        return 400;
    }
    if (!full_name[0] || !strchr(email, '@')) {
        snprintf(err, err_cap, "Họ tên và email chưa hợp lệ");
        return 400;
    }
    if (taken("SELECT COUNT(*) FROM users WHERE username=?", username)) {
        snprintf(err, err_cap, "Tên đăng nhập đã tồn tại");
        return 400;
    }
    if (taken("SELECT COUNT(*) FROM profiles WHERE email=?", email)) {
        snprintf(err, err_cap, "Email đã được dùng");
        return 400;
    }
    if (strcmp(role, "student") == 0) {
        trim_copy(student_code, sizeof(student_code), js_str(body, "studentCode", ""));
        trim_copy(dob, sizeof(dob), js_str(body, "dob", ""));
        trim_copy(gender, sizeof(gender), js_str(body, "gender", ""));
        trim_copy(class_name, sizeof(class_name), js_str(body, "className", ""));
        trim_copy(faculty, sizeof(faculty), js_str(body, "faculty", ""));
        if (!student_code[0] || strlen(dob) != 10 || !gender_ok(gender) || !class_name[0] || !faculty[0]) {
            snprintf(err, err_cap, "Thiếu hoặc sai thông tin sinh viên");
            return 400;
        }
        if (!class_exists(class_name, faculty)) {
            snprintf(err, err_cap, "Lớp không thuộc khoa đã chọn");
            return 400;
        }
        if (taken("SELECT COUNT(*) FROM students WHERE student_code=?", student_code)) {
            snprintf(err, err_cap, "Mã sinh viên đã tồn tại");
            return 400;
        }
    } else if (strcmp(role, "lecturer") == 0) {
        trim_copy(lecturer_code, sizeof(lecturer_code), js_str(body, "lecturerCode", ""));
        trim_copy(department, sizeof(department), js_str(body, "department", ""));
        trim_copy(faculty, sizeof(faculty), js_str(body, "faculty", ""));
        if (!lecturer_code[0] || !department[0] || !faculty[0]) {
            snprintf(err, err_cap, "Thiếu thông tin giảng viên");
            return 400;
        }
        if (taken("SELECT COUNT(*) FROM lecturers WHERE lecturer_code=?", lecturer_code)) {
            snprintf(err, err_cap, "Mã giảng viên đã tồn tại");
            return 400;
        }
    }
    db_begin();
    profile_id = insert_user(username, js_str(body, "password", ""), role, full_name, email, phone, user_id);
    if (!profile_id) {
        db_rollback();
        snprintf(err, err_cap, "Không tạo được tài khoản");
        return 500;
    }
    if (strcmp(role, "student") == 0) {
        stmt = db_prep("INSERT INTO students(user_id, profile_id, student_code, dob, gender, class_name, faculty, status) VALUES(?,?,?,?,?,?,?,'active')");
        if (stmt) {
            sqlite3_bind_int(stmt, 1, *user_id);
            sqlite3_bind_int(stmt, 2, profile_id);
            db_bind_text(stmt, 3, student_code);
            db_bind_text(stmt, 4, dob);
            db_bind_text(stmt, 5, gender);
            db_bind_text(stmt, 6, class_name);
            db_bind_text(stmt, 7, faculty);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else if (strcmp(role, "lecturer") == 0) {
        stmt = db_prep("INSERT INTO lecturers(user_id, profile_id, lecturer_code, department, faculty, status) VALUES(?,?,?,?,?,'active')");
        if (stmt) {
            sqlite3_bind_int(stmt, 1, *user_id);
            sqlite3_bind_int(stmt, 2, profile_id);
            db_bind_text(stmt, 3, lecturer_code);
            db_bind_text(stmt, 4, department);
            db_bind_text(stmt, 5, faculty);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    db_commit();
    return 0;
}

static void write_profile_row(W* w, sqlite3_stmt* stmt) {
    w_obj(w);
    w_key(w, "profileId");
    w_num(w, db_int(stmt, 0));
    w_key(w, "userId");
    w_num(w, db_int(stmt, 1));
    w_key(w, "username");
    w_str(w, db_text(stmt, 2));
    w_key(w, "fullName");
    w_str(w, db_text(stmt, 3));
    w_key(w, "role");
    w_str(w, db_text(stmt, 4));
    w_key(w, "email");
    w_str(w, db_text(stmt, 5));
    w_key(w, "phone");
    w_str(w, db_text(stmt, 6));
    w_key(w, "accountStatus");
    w_str(w, db_text(stmt, 7));
    w_key(w, "status");
    w_str(w, db_text(stmt, 8));
    w_key(w, "studentCode");
    w_str(w, db_text(stmt, 9));
    w_key(w, "lecturerCode");
    w_str(w, db_text(stmt, 10));
    w_key(w, "className");
    w_str(w, db_text(stmt, 11));
    w_key(w, "department");
    w_str(w, db_text(stmt, 12));
    w_key(w, "faculty");
    w_str(w, db_text(stmt, 13));
    w_key(w, "studentId");
    w_num(w, db_int(stmt, 14));
    w_key(w, "dob");
    w_str(w, db_text(stmt, 15));
    w_key(w, "gender");
    w_str(w, db_text(stmt, 16));
    w_key(w, "lecturerId");
    w_num(w, db_int(stmt, 17));
    w_end(w);
}

void route_profiles_list(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    char role[16];
    char q[80];
    char faculty[128];
    char class_name[64];
    char like_q[96];
    char like_faculty[140];
    char like_class[80];
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    copy_str(role, sizeof(role), "");
    query_get(request->query, "role", role, sizeof(role));
    query_get(request->query, "q", q, sizeof(q));
    query_get(request->query, "faculty", faculty, sizeof(faculty));
    query_get(request->query, "className", class_name, sizeof(class_name));
    if (strcmp(actor.role, "lecturer") == 0) {
        copy_str(role, sizeof(role), "student");
        copy_str(faculty, sizeof(faculty), actor.faculty);
    }
    snprintf(like_q, sizeof(like_q), "%%%s%%", q);
    snprintf(like_faculty, sizeof(like_faculty), "%%%s%%", faculty);
    snprintf(like_class, sizeof(like_class), "%%%s%%", class_name);
    stmt = db_prep(
        "SELECT p.id, u.id, u.username, p.full_name, u.role, p.email, p.phone, u.status, p.status, "
        "IFNULL(s.student_code,''), IFNULL(l.lecturer_code,''), IFNULL(s.class_name,''), IFNULL(l.department,''), "
        "CASE WHEN s.id IS NOT NULL THEN s.faculty ELSE IFNULL(l.faculty,'') END, "
        "IFNULL(s.id,0), IFNULL(s.dob,''), IFNULL(s.gender,''), IFNULL(l.id,0) "
        "FROM profiles p JOIN users u ON u.profile_id=p.id "
        "LEFT JOIN students s ON s.user_id=u.id LEFT JOIN lecturers l ON l.user_id=u.id "
        "WHERE (?='' OR u.role=?) "
        "AND (?='' OR p.full_name LIKE ? OR u.username LIKE ? OR p.email LIKE ? OR IFNULL(s.student_code,'') LIKE ? OR IFNULL(l.lecturer_code,'') LIKE ?) "
        "AND (?='' OR (CASE WHEN s.id IS NOT NULL THEN s.faculty ELSE IFNULL(l.faculty,'') END) LIKE ?) "
        "AND (?='' OR IFNULL(s.class_name,'') LIKE ?) "
        "ORDER BY p.full_name");
    if (!stmt) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    db_bind_text(stmt, 1, role);
    db_bind_text(stmt, 2, role);
    db_bind_text(stmt, 3, q);
    db_bind_text(stmt, 4, like_q);
    db_bind_text(stmt, 5, like_q);
    db_bind_text(stmt, 6, like_q);
    db_bind_text(stmt, 7, like_q);
    db_bind_text(stmt, 8, like_q);
    db_bind_text(stmt, 9, faculty);
    db_bind_text(stmt, 10, like_faculty);
    db_bind_text(stmt, 11, class_name);
    db_bind_text(stmt, 12, like_class);
    reply_begin(&w);
    w_arr(&w);
    while (sqlite3_step(stmt) == SQLITE_ROW) write_profile_row(&w, stmt);
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

void route_profiles_create(Request* request, Response* response) {
    Actor actor;
    char err[240];
    int user_id = 0;
    int status;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    status = create_account(request->json, 0, &user_id, err, sizeof(err));
    if (status) {
        reply_fail(response, status, err);
        return;
    }
    audit_add(actor.user_id, "CREATE_PROFILE", "Tạo hồ sơ mới", request->ip);
    reply_begin(&w);
    write_user(&w, user_id);
    reply_json(response, &w);
}

void route_profile_me(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    char phone[40];
    W w;
    if (!require_user(request, response, &actor)) return;
    trim_copy(phone, sizeof(phone), js_str(request->json, "phone", ""));
    stmt = db_prep("UPDATE profiles SET phone=? WHERE id=?");
    if (!stmt) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    db_bind_text(stmt, 1, phone);
    sqlite3_bind_int(stmt, 2, actor.profile_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    reply_begin(&w);
    write_user(&w, actor.user_id);
    reply_json(response, &w);
}

void route_avatar(Request* request, Response* response) {
    Actor actor;
    const char* avatar = js_str(request->json, "avatar", "");
    sqlite3_stmt* stmt;
    W w;
    if (!require_user(request, response, &actor)) return;
    if (avatar[0] && (strncmp(avatar, "data:image", 10) != 0 || strlen(avatar) > 120000)) {
        reply_fail(response, 400, "Ảnh đại diện không hợp lệ");
        return;
    }
    stmt = db_prep("UPDATE profiles SET avatar=? WHERE id=?");
    if (!stmt) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    db_bind_text(stmt, 1, avatar);
    sqlite3_bind_int(stmt, 2, actor.profile_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    reply_begin(&w);
    write_user(&w, actor.user_id);
    reply_json(response, &w);
}

void route_profile_update(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    char role[16];
    char full_name[160];
    char email[160];
    char phone[40];
    int profile_id = request->id;
    int user_id = 0;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    stmt = db_prep("SELECT user_id, role FROM profiles WHERE id=?");
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không tìm thấy hồ sơ");
        return;
    }
    user_id = db_int(stmt, 0);
    copy_str(role, sizeof(role), db_text(stmt, 1));
    sqlite3_finalize(stmt);
    trim_copy(full_name, sizeof(full_name), js_str(request->json, "fullName", ""));
    trim_copy(email, sizeof(email), js_str(request->json, "email", ""));
    trim_copy(phone, sizeof(phone), js_str(request->json, "phone", ""));
    if (!full_name[0] || !strchr(email, '@')) {
        reply_fail(response, 400, "Họ tên và email chưa hợp lệ");
        return;
    }
    stmt = db_prep("UPDATE profiles SET full_name=?, email=?, phone=? WHERE id=?");
    if (stmt) {
        db_bind_text(stmt, 1, full_name);
        db_bind_text(stmt, 2, email);
        db_bind_text(stmt, 3, phone);
        sqlite3_bind_int(stmt, 4, profile_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (strcmp(role, "student") == 0) {
        char student_code[40], dob[16], gender[16], class_name[64], faculty[128];
        trim_copy(student_code, sizeof(student_code), js_str(request->json, "studentCode", ""));
        trim_copy(dob, sizeof(dob), js_str(request->json, "dob", ""));
        trim_copy(gender, sizeof(gender), js_str(request->json, "gender", ""));
        trim_copy(class_name, sizeof(class_name), js_str(request->json, "className", ""));
        trim_copy(faculty, sizeof(faculty), js_str(request->json, "faculty", ""));
        if (!student_code[0] || !gender_ok(gender) || !class_exists(class_name, faculty)) {
            reply_fail(response, 400, "Thông tin sinh viên chưa hợp lệ");
            return;
        }
        stmt = db_prep("UPDATE students SET student_code=?, dob=?, gender=?, class_name=?, faculty=? WHERE profile_id=?");
        if (stmt) {
            db_bind_text(stmt, 1, student_code);
            db_bind_text(stmt, 2, dob);
            db_bind_text(stmt, 3, gender);
            db_bind_text(stmt, 4, class_name);
            db_bind_text(stmt, 5, faculty);
            sqlite3_bind_int(stmt, 6, profile_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else if (strcmp(role, "lecturer") == 0) {
        char lecturer_code[40], department[128], faculty[128];
        trim_copy(lecturer_code, sizeof(lecturer_code), js_str(request->json, "lecturerCode", ""));
        trim_copy(department, sizeof(department), js_str(request->json, "department", ""));
        trim_copy(faculty, sizeof(faculty), js_str(request->json, "faculty", ""));
        if (!lecturer_code[0] || !department[0] || !faculty[0]) {
            reply_fail(response, 400, "Thiếu thông tin giảng viên");
            return;
        }
        stmt = db_prep("UPDATE lecturers SET lecturer_code=?, department=?, faculty=? WHERE profile_id=?");
        if (stmt) {
            db_bind_text(stmt, 1, lecturer_code);
            db_bind_text(stmt, 2, department);
            db_bind_text(stmt, 3, faculty);
            sqlite3_bind_int(stmt, 4, profile_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    audit_add(actor.user_id, "UPDATE_PROFILE", "Cập nhật hồ sơ", request->ip);
    reply_begin(&w);
    write_user(&w, user_id);
    reply_json(response, &w);
}

static int profile_user(int profile_id, int* user_id) {
    sqlite3_stmt* stmt = db_prep("SELECT user_id FROM profiles WHERE id=?");
    int ok = 0;
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        *user_id = db_int(stmt, 0);
        ok = 1;
    }
    sqlite3_finalize(stmt);
    return ok;
}

static void set_status(const char* sql, int id, const char* status) {
    sqlite3_stmt* stmt = db_prep(sql);
    if (!stmt) return;
    db_bind_text(stmt, 1, status);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void drop_sessions(int user_id) {
    sqlite3_stmt* stmt = db_prep("DELETE FROM sessions WHERE user_id=?");
    if (!stmt) return;
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void account_done(Response* response) {
    W w;
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_profile_lock(Request* request, Response* response) {
    Actor actor;
    int user_id = 0;
    if (!require_role(request, response, &actor, "admin")) return;
    if (!profile_user(request->id, &user_id)) {
        reply_fail(response, 404, "Không tìm thấy hồ sơ");
        return;
    }
    if (user_id == actor.user_id) {
        reply_fail(response, 400, "Không khóa chính tài khoản đang dùng");
        return;
    }
    set_status("UPDATE users SET status=? WHERE id=?", user_id, "locked");
    drop_sessions(user_id);
    audit_add(actor.user_id, "LOCK", "Khóa tài khoản", request->ip);
    account_done(response);
}

void route_profile_unlock(Request* request, Response* response) {
    Actor actor;
    int user_id = 0;
    if (!require_role(request, response, &actor, "admin")) return;
    if (!profile_user(request->id, &user_id)) {
        reply_fail(response, 404, "Không tìm thấy hồ sơ");
        return;
    }
    set_status("UPDATE users SET status=? WHERE id=?", user_id, "active");
    audit_add(actor.user_id, "UNLOCK", "Mở khóa tài khoản", request->ip);
    account_done(response);
}

void route_profile_disable(Request* request, Response* response) {
    Actor actor;
    int user_id = 0;
    if (!require_role(request, response, &actor, "admin")) return;
    if (!profile_user(request->id, &user_id) || user_id == actor.user_id) {
        reply_fail(response, 400, "Không vô hiệu hồ sơ này");
        return;
    }
    set_status("UPDATE profiles SET status=? WHERE id=?", request->id, "disabled");
    drop_sessions(user_id);
    audit_add(actor.user_id, "DISABLE", "Vô hiệu hồ sơ", request->ip);
    account_done(response);
}

void route_profile_enable(Request* request, Response* response) {
    Actor actor;
    if (!require_role(request, response, &actor, "admin")) return;
    set_status("UPDATE profiles SET status=? WHERE id=?", request->id, "active");
    audit_add(actor.user_id, "ENABLE", "Mở lại hồ sơ", request->ip);
    account_done(response);
}

void route_profile_reset(Request* request, Response* response) {
    Actor actor;
    int user_id = 0;
    char salt[40];
    char hash[65];
    char temp[24];
    char hex[8];
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    if (!profile_user(request->id, &user_id)) {
        reply_fail(response, 404, "Không tìm thấy hồ sơ");
        return;
    }
    random_hex(2, hex);
    snprintf(temp, sizeof(temp), "Reset@%s", hex);
    random_hex(16, salt);
    hash_password(salt, temp, hash);
    stmt = db_prep("UPDATE users SET password_hash=?, salt=?, must_change=1 WHERE id=?");
    if (!stmt) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    db_bind_text(stmt, 1, hash);
    db_bind_text(stmt, 2, salt);
    sqlite3_bind_int(stmt, 3, user_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    drop_sessions(user_id);
    audit_add(actor.user_id, "RESET_PASSWORD", "Đặt lại mật khẩu", request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "temporaryPassword");
    w_str(&w, temp);
    w_end(&w);
    reply_json(response, &w);
}

void route_profile_delete(Request* request, Response* response) {
    Actor actor;
    int user_id = 0;
    if (!require_role(request, response, &actor, "admin")) return;
    if (!profile_user(request->id, &user_id) || user_id == actor.user_id) {
        reply_fail(response, 400, "Không xóa hồ sơ này");
        return;
    }
    if (scalar_int("SELECT COUNT(*) FROM attempts a JOIN students s ON s.id=a.student_id WHERE s.profile_id=?", request->id, -1) > 0 ||
        scalar_int("SELECT COUNT(*) FROM subjects sub JOIN lecturers l ON l.id=sub.lecturer_id WHERE l.profile_id=?", request->id, -1) > 0) {
        reply_fail(response, 400, "Không xóa được hồ sơ đã có bài thi hoặc môn học");
        return;
    }
    drop_sessions(user_id);
    {
        sqlite3_stmt* stmt = db_prep("DELETE FROM notifications WHERE user_id=?");
        if (stmt) {
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        stmt = db_prep("DELETE FROM students WHERE profile_id=?");
        if (stmt) {
            sqlite3_bind_int(stmt, 1, request->id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        stmt = db_prep("DELETE FROM lecturers WHERE profile_id=?");
        if (stmt) {
            sqlite3_bind_int(stmt, 1, request->id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        stmt = db_prep("DELETE FROM users WHERE id=?");
        if (stmt) {
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        stmt = db_prep("DELETE FROM profiles WHERE id=?");
        if (stmt) {
            sqlite3_bind_int(stmt, 1, request->id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    audit_add(actor.user_id, "DELETE_PROFILE", "Xóa hồ sơ", request->ip);
    account_done(response);
}

static int can_view_student(const Actor* actor, int student_id) {
    sqlite3_stmt* stmt;
    char faculty[128];
    int user_id = 0;
    int ok = 0;
    if (strcmp(actor->role, "admin") == 0) return 1;
    stmt = db_prep("SELECT user_id, faculty FROM students WHERE id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, student_id);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = db_int(stmt, 0);
        copy_str(faculty, sizeof(faculty), db_text(stmt, 1));
        ok = 1;
    }
    sqlite3_finalize(stmt);
    if (!ok) return 0;
    if (strcmp(actor->role, "student") == 0) return actor->user_id == user_id;
    if (strcmp(actor->role, "lecturer") == 0) return strcmp(actor->faculty, faculty) == 0;
    return 0;
}

void route_history(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_user(request, response, &actor)) return;
    if (!can_view_student(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không xem được lịch sử này");
        return;
    }
    stmt = db_prep(
        "SELECT a.id, e.title, a.status, a.score, a.paper_total, a.paper_code, a.submit_time "
        "FROM attempts a JOIN exams e ON e.id=a.exam_id WHERE a.student_id=? ORDER BY a.id DESC");
    if (!stmt) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    sqlite3_bind_int(stmt, 1, request->id);
    reply_begin(&w);
    w_arr(&w);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "attemptId");
        w_num(&w, db_int(stmt, 0));
        w_key(&w, "examTitle");
        w_str(&w, db_text(stmt, 1));
        w_key(&w, "status");
        w_str(&w, db_text(stmt, 2));
        w_key(&w, "score");
        w_num(&w, db_real(stmt, 3));
        w_key(&w, "total");
        w_num(&w, db_real(stmt, 4));
        w_key(&w, "paperCode");
        w_str(&w, db_text(stmt, 5));
        w_key(&w, "submitTime");
        w_num(&w, (double)db_i64(stmt, 6));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}
