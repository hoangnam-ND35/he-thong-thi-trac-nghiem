#include "app.h"

#include <string.h>

static int staff(Request* request, Response* response, Actor* actor) {
    return require_role(request, response, actor, "admin,lecturer");
}

void route_departments_list(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!staff(request, response, &actor)) return;
    stmt = db_prep("SELECT id, name, faculty FROM departments ORDER BY name");
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "id");
        w_num(&w, db_int(stmt, 0));
        w_key(&w, "name");
        w_str(&w, db_text(stmt, 1));
        w_key(&w, "faculty");
        w_str(&w, db_text(stmt, 2));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

void route_departments_create(Request* request, Response* response) {
    Actor actor;
    char name[128];
    char faculty[128];
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    trim_copy(name, sizeof(name), js_str(request->json, "name", ""));
    trim_copy(faculty, sizeof(faculty), js_str(request->json, "faculty", ""));
    if (!name[0] || !faculty[0]) {
        reply_fail(response, 400, "Thiếu tên bộ môn hoặc khoa");
        return;
    }
    stmt = db_prep("INSERT INTO departments(name, faculty) VALUES(?,?)");
    db_bind_text(stmt, 1, name);
    db_bind_text(stmt, 2, faculty);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    audit_add(actor.user_id, "CREATE_DEPARTMENT", name, request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "id");
    w_num(&w, (double)sqlite3_last_insert_rowid(g_db));
    w_end(&w);
    reply_json(response, &w);
}

void route_department_update(Request* request, Response* response) {
    Actor actor;
    char name[128];
    char faculty[128];
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    trim_copy(name, sizeof(name), js_str(request->json, "name", ""));
    trim_copy(faculty, sizeof(faculty), js_str(request->json, "faculty", ""));
    stmt = db_prep("UPDATE departments SET name=?, faculty=? WHERE id=?");
    db_bind_text(stmt, 1, name);
    db_bind_text(stmt, 2, faculty);
    sqlite3_bind_int(stmt, 3, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_classes_list(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!staff(request, response, &actor)) return;
    stmt = db_prep("SELECT c.id, c.name, c.faculty, c.department_id, d.name FROM classes c JOIN departments d ON d.id=c.department_id ORDER BY c.name");
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "id");
        w_num(&w, db_int(stmt, 0));
        w_key(&w, "name");
        w_str(&w, db_text(stmt, 1));
        w_key(&w, "faculty");
        w_str(&w, db_text(stmt, 2));
        w_key(&w, "departmentId");
        w_num(&w, db_int(stmt, 3));
        w_key(&w, "departmentName");
        w_str(&w, db_text(stmt, 4));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

void route_classes_create(Request* request, Response* response) {
    Actor actor;
    char name[64];
    char faculty[128];
    int department_id;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    trim_copy(name, sizeof(name), js_str(request->json, "name", ""));
    trim_copy(faculty, sizeof(faculty), js_str(request->json, "faculty", ""));
    department_id = (int)js_num(request->json, "departmentId", 0);
    if (!name[0] || !faculty[0] || scalar_int("SELECT COUNT(*) FROM departments WHERE id=?", department_id, -1) == 0) {
        reply_fail(response, 400, "Thông tin lớp chưa hợp lệ");
        return;
    }
    stmt = db_prep("INSERT INTO classes(name, faculty, department_id) VALUES(?,?,?)");
    db_bind_text(stmt, 1, name);
    db_bind_text(stmt, 2, faculty);
    sqlite3_bind_int(stmt, 3, department_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    audit_add(actor.user_id, "CREATE_CLASS", name, request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "id");
    w_num(&w, (double)sqlite3_last_insert_rowid(g_db));
    w_end(&w);
    reply_json(response, &w);
}

void route_class_update(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    stmt = db_prep("UPDATE classes SET name=?, faculty=?, department_id=? WHERE id=?");
    db_bind_text(stmt, 1, js_str(request->json, "name", ""));
    db_bind_text(stmt, 2, js_str(request->json, "faculty", ""));
    sqlite3_bind_int(stmt, 3, (int)js_num(request->json, "departmentId", 0));
    sqlite3_bind_int(stmt, 4, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}

static void write_subject(W* w, sqlite3_stmt* stmt, const Actor* actor) {
    int lecturer_id = db_int(stmt, 5);
    w_obj(w);
    w_key(w, "id");
    w_num(w, db_int(stmt, 0));
    w_key(w, "code");
    w_str(w, db_text(stmt, 1));
    w_key(w, "name");
    w_str(w, db_text(stmt, 2));
    w_key(w, "credits");
    w_num(w, db_int(stmt, 3));
    w_key(w, "departmentId");
    w_num(w, db_int(stmt, 4));
    w_key(w, "lecturerId");
    w_num(w, lecturer_id);
    w_key(w, "description");
    w_str(w, db_text(stmt, 6));
    w_key(w, "departmentName");
    w_str(w, db_text(stmt, 7));
    w_key(w, "lecturerName");
    w_str(w, db_text(stmt, 8));
    w_key(w, "canEdit");
    w_bool(w, strcmp(actor->role, "admin") == 0 || lecturer_id == actor->lecturer_id);
    w_end(w);
}

void route_subjects_list(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!staff(request, response, &actor)) return;
    stmt = db_prep(
        "SELECT s.id, s.code, s.name, s.credits, s.department_id, s.lecturer_id, s.description, d.name, p.full_name "
        "FROM subjects s JOIN departments d ON d.id=s.department_id "
        "JOIN lecturers l ON l.id=s.lecturer_id JOIN profiles p ON p.id=l.profile_id "
        "WHERE (?=0 OR s.lecturer_id=?) ORDER BY s.code");
    sqlite3_bind_int(stmt, 1, strcmp(actor.role, "lecturer") == 0 ? actor.lecturer_id : 0);
    sqlite3_bind_int(stmt, 2, actor.lecturer_id);
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) write_subject(&w, stmt, &actor);
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

static int save_subject(Request* request, Response* response, Actor* actor, int existing) {
    char code[32];
    char name[160];
    char description[500];
    int credits;
    int department_id;
    int lecturer_id;
    sqlite3_stmt* stmt;
    W w;
    trim_copy(code, sizeof(code), js_str(request->json, "code", ""));
    trim_copy(name, sizeof(name), js_str(request->json, "name", ""));
    trim_copy(description, sizeof(description), js_str(request->json, "description", ""));
    credits = (int)js_num(request->json, "credits", 3);
    department_id = (int)js_num(request->json, "departmentId", 0);
    lecturer_id = strcmp(actor->role, "admin") == 0 ? (int)js_num(request->json, "lecturerId", 0) : actor->lecturer_id;
    if (!code[0] || !name[0] || credits < 1 || credits > 10) {
        reply_fail(response, 400, "Thông tin môn học chưa hợp lệ");
        return 0;
    }
    if (scalar_int("SELECT COUNT(*) FROM departments WHERE id=?", department_id, -1) == 0 ||
        scalar_int("SELECT COUNT(*) FROM lecturers WHERE id=?", lecturer_id, -1) == 0) {
        reply_fail(response, 400, "Bộ môn hoặc giáo viên không tồn tại");
        return 0;
    }
    if (existing) {
        if (!owns_subject(actor, existing)) {
            reply_fail(response, 403, "Bạn không sửa được môn này");
            return 0;
        }
        stmt = db_prep("UPDATE subjects SET code=?, name=?, credits=?, department_id=?, lecturer_id=?, description=? WHERE id=?");
        sqlite3_bind_int(stmt, 7, existing);
    } else {
        stmt = db_prep("INSERT INTO subjects(code, name, credits, department_id, lecturer_id, description) VALUES(?,?,?,?,?,?)");
    }
    db_bind_text(stmt, 1, code);
    db_bind_text(stmt, 2, name);
    sqlite3_bind_int(stmt, 3, credits);
    sqlite3_bind_int(stmt, 4, department_id);
    sqlite3_bind_int(stmt, 5, lecturer_id);
    db_bind_text(stmt, 6, description);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        reply_fail(response, 400, "Mã môn đã tồn tại");
        return 0;
    }
    sqlite3_finalize(stmt);
    audit_add(actor->user_id, existing ? "UPDATE_SUBJECT" : "CREATE_SUBJECT", code, request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "id");
    w_num(&w, existing ? existing : (double)sqlite3_last_insert_rowid(g_db));
    w_end(&w);
    reply_json(response, &w);
    return 1;
}

void route_subjects_create(Request* request, Response* response) {
    Actor actor;
    if (!staff(request, response, &actor)) return;
    save_subject(request, response, &actor, 0);
}

void route_subject_update(Request* request, Response* response) {
    Actor actor;
    if (!staff(request, response, &actor)) return;
    save_subject(request, response, &actor, request->id);
}

void route_subject_delete(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!staff(request, response, &actor)) return;
    if (!owns_subject(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không xóa được môn này");
        return;
    }
    if (scalar_int("SELECT COUNT(*) FROM exams WHERE subject_id=?", request->id, -1) > 0 ||
        scalar_int("SELECT COUNT(*) FROM questions WHERE subject_id=?", request->id, -1) > 0) {
        reply_fail(response, 400, "Môn đã có câu hỏi hoặc kỳ thi");
        return;
    }
    stmt = db_prep("DELETE FROM subjects WHERE id=?");
    sqlite3_bind_int(stmt, 1, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}
