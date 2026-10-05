#include "app.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int this_year(void) {
    time_t now = time(NULL);
    struct tm local;
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return local.tm_year + 1900;
}

static int cccd_province(const char* cccd) {
    static const char* list =
        ",001,002,004,006,008,010,011,012,014,015,017,019,020,022,024,025,026,027,"
        "030,031,033,034,035,036,037,038,040,042,044,045,046,048,049,051,052,054,"
        "056,058,060,062,064,066,067,068,070,072,074,075,077,079,080,082,083,084,"
        "086,087,089,091,092,093,094,095,096,";
    char key[8];
    snprintf(key, sizeof(key), ",%.3s,", cccd);
    return strstr(list, key) != NULL;
}

static int cccd_year(const char* cccd) {
    int century = cccd[3] - '0';
    int yy = (cccd[4] - '0') * 10 + (cccd[5] - '0');
    if (century == 0 || century == 1) return 1900 + yy;
    if (century == 2 || century == 3) return 2000 + yy;
    return 0;
}

static int code_ok(const char* code) {
    size_t n = code ? strlen(code) : 0;
    size_t i;
    if (n < 3 || n > 32) return 0;
    for (i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)code[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_')) return 0;
    }
    return 1;
}

static const char* cccd_problem(const char* cccd, const char* gender, const char* dob) {
    size_t i;
    int year;
    int dob_year = 0;
    if (!cccd || strlen(cccd) != 12) return "Số CCCD cần đúng 12 chữ số";
    for (i = 0; i < 12; i++) {
        if (cccd[i] < '0' || cccd[i] > '9') return "Số CCCD cần đúng 12 chữ số";
    }
    if (!cccd_province(cccd)) return "Ba số đầu của CCCD không phải mã tỉnh, thành";
    year = cccd_year(cccd);
    if (!year) return "Số CCCD không đúng cấu trúc giới tính và năm sinh";
    if (this_year() - year < 18) return "Năm sinh trên CCCD chưa đủ 18 tuổi để làm giáo viên";
    if (gender && strcmp(gender, "Nam") == 0 && ((cccd[3] - '0') % 2) != 0) return "Giới tính trên CCCD không khớp hồ sơ";
    if (gender && strcmp(gender, "Nữ") == 0 && ((cccd[3] - '0') % 2) == 0) return "Giới tính trên CCCD không khớp hồ sơ";
    if (dob && strlen(dob) >= 4) {
        dob_year = (dob[0] - '0') * 1000 + (dob[1] - '0') * 100 + (dob[2] - '0') * 10 + (dob[3] - '0');
        if (dob_year > 1900 && dob_year != year) return "Năm sinh trên CCCD không trùng hồ sơ học sinh";
    }
    return NULL;
}

static void mask_cccd(const char* cccd, char out[16]) {
    snprintf(out, 16, "*********%.3s", (cccd && strlen(cccd) == 12) ? cccd + 9 : "");
}

static void write_upgrade(W* w, sqlite3_stmt* stmt, int reveal) {
    char masked[16];
    mask_cccd(db_text(stmt, 3), masked);
    w_obj(w);
    w_key(w, "id");
    w_num(w, db_int(stmt, 0));
    w_key(w, "userId");
    w_num(w, db_int(stmt, 1));
    w_key(w, "username");
    w_str(w, db_text(stmt, 2));
    w_key(w, "cccd");
    w_str(w, reveal ? db_text(stmt, 3) : masked);
    w_key(w, "fullName");
    w_str(w, db_text(stmt, 4));
    w_key(w, "lecturerCode");
    w_str(w, db_text(stmt, 5));
    w_key(w, "department");
    w_str(w, db_text(stmt, 6));
    w_key(w, "faculty");
    w_str(w, db_text(stmt, 7));
    w_key(w, "status");
    w_str(w, db_text(stmt, 8));
    w_key(w, "note");
    w_str(w, db_text(stmt, 9));
    w_key(w, "createdAt");
    w_num(w, (double)db_i64(stmt, 10));
    w_end(w);
}

void route_teacher_upgrade_get(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "student")) return;
    stmt = db_prep(
        "SELECT t.id, t.user_id, u.username, t.cccd, t.full_name, t.lecturer_code, t.department, t.faculty, t.status, t.note, t.created_at "
        "FROM teacher_upgrades t JOIN users u ON u.id=t.user_id WHERE t.user_id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, actor.user_id);
    reply_begin(&w);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) write_upgrade(&w, stmt, 0);
    else {
        w_obj(&w);
        w_key(&w, "status");
        w_str(&w, "");
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    reply_json(response, &w);
}

void route_teacher_upgrade_submit(Request* request, Response* response) {
    Actor actor;
    char cccd[16];
    char full_name[160];
    char code[40];
    char department[128];
    char faculty[128];
    const char* problem;
    sqlite3_stmt* stmt;
    int existing = 0;
    int step = SQLITE_ERROR;
    if (!require_role(request, response, &actor, "student")) return;
    trim_copy(cccd, sizeof(cccd), js_str(request->json, "cccd", ""));
    trim_copy(full_name, sizeof(full_name), js_str(request->json, "fullName", ""));
    trim_copy(code, sizeof(code), js_str(request->json, "lecturerCode", ""));
    trim_copy(department, sizeof(department), js_str(request->json, "department", ""));
    trim_copy(faculty, sizeof(faculty), js_str(request->json, "faculty", ""));
    if (strcmp(full_name, actor.full_name) != 0) {
        reply_fail(response, 400, "Họ tên trên CCCD phải trùng họ tên tài khoản");
        return;
    }
    problem = cccd_problem(cccd, actor.gender, actor.dob);
    if (problem) {
        reply_fail(response, 400, problem);
        return;
    }
    if (!code_ok(code) || !department[0] || !faculty[0] || strlen(department) > 120 || strlen(faculty) > 120) {
        reply_fail(response, 400, "Cần mã giáo viên, bộ môn và khoa hợp lệ");
        return;
    }
    stmt = db_prep("SELECT COUNT(*) FROM lecturers WHERE lecturer_code=?");
    if (stmt) db_bind_text(stmt, 1, code);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW && db_int(stmt, 0) > 0) {
        sqlite3_finalize(stmt);
        reply_fail(response, 400, "Mã giáo viên đã tồn tại");
        return;
    }
    sqlite3_finalize(stmt);
    stmt = db_prep("SELECT COUNT(*) FROM teacher_upgrades WHERE cccd=? AND user_id<>? AND status IN ('pending','approved')");
    if (stmt) {
        db_bind_text(stmt, 1, cccd);
        sqlite3_bind_int(stmt, 2, actor.user_id);
    }
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW && db_int(stmt, 0) > 0) {
        sqlite3_finalize(stmt);
        reply_fail(response, 400, "Số CCCD này đã được dùng cho đơn khác");
        return;
    }
    sqlite3_finalize(stmt);
    stmt = db_prep("SELECT COUNT(*) FROM teacher_upgrades WHERE lecturer_code=? AND user_id<>? AND status='pending'");
    if (stmt) {
        db_bind_text(stmt, 1, code);
        sqlite3_bind_int(stmt, 2, actor.user_id);
    }
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW && db_int(stmt, 0) > 0) {
        sqlite3_finalize(stmt);
        reply_fail(response, 400, "Mã giáo viên đang chờ duyệt ở đơn khác");
        return;
    }
    sqlite3_finalize(stmt);
    stmt = db_prep("SELECT id, status FROM teacher_upgrades WHERE user_id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, actor.user_id);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        existing = db_int(stmt, 0);
        if (strcmp(db_text(stmt, 1), "approved") == 0) {
            sqlite3_finalize(stmt);
            reply_fail(response, 400, "Đơn này đã được duyệt");
            return;
        }
    }
    sqlite3_finalize(stmt);
    if (existing) {
        stmt = db_prep("UPDATE teacher_upgrades SET cccd=?, full_name=?, lecturer_code=?, department=?, faculty=?, status='pending', note='', created_at=?, reviewed_at=0 WHERE id=?");
        if (!stmt) {
            reply_fail(response, 500, "Lỗi dữ liệu");
            return;
        }
        db_bind_text(stmt, 1, cccd);
        db_bind_text(stmt, 2, full_name);
        db_bind_text(stmt, 3, code);
        db_bind_text(stmt, 4, department);
        db_bind_text(stmt, 5, faculty);
        sqlite3_bind_int64(stmt, 6, now_sec());
        sqlite3_bind_int(stmt, 7, existing);
        step = sqlite3_step(stmt);
    } else {
        stmt = db_prep("INSERT INTO teacher_upgrades(user_id, profile_id, cccd, full_name, lecturer_code, department, faculty, status, note, created_at, reviewed_at) VALUES(?,?,?,?,?,?,?,'pending','',?,0)");
        if (!stmt) {
            reply_fail(response, 500, "Lỗi dữ liệu");
            return;
        }
        sqlite3_bind_int(stmt, 1, actor.user_id);
        sqlite3_bind_int(stmt, 2, actor.profile_id);
        db_bind_text(stmt, 3, cccd);
        db_bind_text(stmt, 4, full_name);
        db_bind_text(stmt, 5, code);
        db_bind_text(stmt, 6, department);
        db_bind_text(stmt, 7, faculty);
        sqlite3_bind_int64(stmt, 8, now_sec());
        step = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    if (step != SQLITE_DONE) {
        reply_fail(response, 500, "Không lưu được đơn");
        return;
    }
    audit_add(actor.user_id, "REQUEST_TEACHER", "Gửi đơn nâng cấp giáo viên", request->ip);
    route_teacher_upgrade_get(request, response);
}

void route_teacher_upgrades_list(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    stmt = db_prep(
        "SELECT t.id, t.user_id, u.username, t.cccd, t.full_name, t.lecturer_code, t.department, t.faculty, t.status, t.note, t.created_at "
        "FROM teacher_upgrades t JOIN users u ON u.id=t.user_id WHERE t.status='pending' ORDER BY t.created_at");
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) write_upgrade(&w, stmt, 1);
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

void route_teacher_upgrade_approve(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    int user_id = 0;
    int profile_id = 0;
    char code[40];
    char department[128];
    char faculty[128];
    char status[16];
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    stmt = db_prep("SELECT user_id, profile_id, lecturer_code, department, faculty, status FROM teacher_upgrades WHERE id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, request->id);
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không thấy đơn này");
        return;
    }
    user_id = db_int(stmt, 0);
    profile_id = db_int(stmt, 1);
    copy_str(code, sizeof(code), db_text(stmt, 2));
    copy_str(department, sizeof(department), db_text(stmt, 3));
    copy_str(faculty, sizeof(faculty), db_text(stmt, 4));
    copy_str(status, sizeof(status), db_text(stmt, 5));
    sqlite3_finalize(stmt);
    if (strcmp(status, "pending") != 0) {
        reply_fail(response, 400, "Đơn này không còn chờ duyệt");
        return;
    }
    if (scalar_int("SELECT COUNT(*) FROM users WHERE id=? AND role='student'", user_id, -1) != 1) {
        reply_fail(response, 400, "Tài khoản không còn là học sinh");
        return;
    }
    stmt = db_prep("SELECT COUNT(*) FROM lecturers WHERE lecturer_code=?");
    if (stmt) db_bind_text(stmt, 1, code);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW && db_int(stmt, 0) > 0) {
        sqlite3_finalize(stmt);
        reply_fail(response, 400, "Mã giáo viên đã tồn tại");
        return;
    }
    sqlite3_finalize(stmt);
    db_begin();
    stmt = db_prep("UPDATE users SET role='lecturer' WHERE id=? AND role='student'");
    if (stmt) sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (sqlite3_changes(g_db) != 1) {
        db_rollback();
        reply_fail(response, 400, "Không nâng được tài khoản này");
        return;
    }
    stmt = db_prep("UPDATE profiles SET role='lecturer' WHERE id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, profile_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = db_prep("INSERT INTO lecturers(user_id, profile_id, lecturer_code, department, faculty, status) VALUES(?,?,?,?,?,'active')");
    if (stmt) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, profile_id);
        db_bind_text(stmt, 3, code);
        db_bind_text(stmt, 4, department);
        db_bind_text(stmt, 5, faculty);
    }
    if (!stmt || sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        db_rollback();
        reply_fail(response, 400, "Không tạo được hồ sơ giáo viên");
        return;
    }
    sqlite3_finalize(stmt);
    stmt = db_prep("UPDATE teacher_upgrades SET status='approved', note='', reviewed_at=? WHERE id=?");
    if (stmt) {
        sqlite3_bind_int64(stmt, 1, now_sec());
        sqlite3_bind_int(stmt, 2, request->id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    db_commit();
    notify_user(user_id, "Đơn nâng cấp giáo viên đã được duyệt. Hãy tải lại trang để vào quyền giáo viên.");
    audit_add(actor.user_id, "APPROVE_TEACHER", "Duyệt nâng cấp giáo viên", request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_teacher_upgrade_reject(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    char note[200];
    int user_id = 0;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    trim_copy(note, sizeof(note), js_str(request->json, "note", ""));
    if (!note[0]) {
        reply_fail(response, 400, "Cần ghi lý do từ chối");
        return;
    }
    stmt = db_prep("SELECT user_id, status FROM teacher_upgrades WHERE id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, request->id);
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không thấy đơn này");
        return;
    }
    user_id = db_int(stmt, 0);
    if (strcmp(db_text(stmt, 1), "pending") != 0) {
        sqlite3_finalize(stmt);
        reply_fail(response, 400, "Đơn này không còn chờ duyệt");
        return;
    }
    sqlite3_finalize(stmt);
    stmt = db_prep("UPDATE teacher_upgrades SET status='rejected', note=?, reviewed_at=? WHERE id=?");
    if (stmt) {
        db_bind_text(stmt, 1, note);
        sqlite3_bind_int64(stmt, 2, now_sec());
        sqlite3_bind_int(stmt, 3, request->id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    notify_user(user_id, "Đơn nâng cấp giáo viên bị từ chối. Xem lý do trong tài khoản.");
    audit_add(actor.user_id, "REJECT_TEACHER", "Từ chối nâng cấp giáo viên", request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}
