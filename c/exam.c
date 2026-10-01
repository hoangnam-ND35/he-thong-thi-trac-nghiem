#include "app.h"

#include <stdio.h>
#include <string.h>

static int exam_subject(int exam_id) {
    return scalar_int("SELECT subject_id FROM exams WHERE id=?", exam_id, -1);
}

static int can_manage_exam(const Actor* actor, int exam_id) {
    int subject_id = exam_subject(exam_id);
    return subject_id && owns_subject(actor, subject_id);
}

static void write_exam_row(W* w, sqlite3_stmt* stmt, const Actor* actor) {
    int class_id = db_int(stmt, 16);
    int subject_id = db_int(stmt, 1);
    w_obj(w);
    w_key(w, "id");
    w_num(w, db_int(stmt, 0));
    w_key(w, "subjectId");
    w_num(w, subject_id);
    w_key(w, "subjectCode");
    w_str(w, db_text(stmt, 21));
    w_key(w, "subjectName");
    w_str(w, db_text(stmt, 22));
    w_key(w, "lecturerName");
    w_str(w, db_text(stmt, 23));
    w_key(w, "title");
    w_str(w, db_text(stmt, 2));
    w_key(w, "description");
    w_str(w, db_text(stmt, 3));
    w_key(w, "startTime");
    w_num(w, (double)db_i64(stmt, 4));
    w_key(w, "endTime");
    w_num(w, (double)db_i64(stmt, 5));
    w_key(w, "durationMinutes");
    w_num(w, db_int(stmt, 6));
    w_key(w, "easyCount");
    w_num(w, db_int(stmt, 7));
    w_key(w, "mediumCount");
    w_num(w, db_int(stmt, 8));
    w_key(w, "hardCount");
    w_num(w, db_int(stmt, 9));
    w_key(w, "totalQuestions");
    w_num(w, db_int(stmt, 10));
    w_key(w, "totalScore");
    w_num(w, db_real(stmt, 11));
    w_key(w, "shuffleQuestions");
    w_bool(w, db_int(stmt, 12));
    w_key(w, "shuffleAnswers");
    w_bool(w, db_int(stmt, 13));
    w_key(w, "autoSubmit");
    w_bool(w, db_int(stmt, 14));
    w_key(w, "maxAttempts");
    w_num(w, db_int(stmt, 15));
    w_key(w, "classId");
    w_num(w, class_id);
    w_key(w, "className");
    w_str(w, class_id ? db_text(stmt, 24) : "Tất cả");
    w_key(w, "status");
    w_str(w, db_text(stmt, 17));
    w_key(w, "shortDisconnectSec");
    w_num(w, db_int(stmt, 18));
    w_key(w, "longDisconnectSec");
    w_num(w, db_int(stmt, 19));
    w_key(w, "syncGraceSec");
    w_num(w, db_int(stmt, 20));
    w_key(w, "attemptCount");
    w_num(w, db_int(stmt, 25));
    w_key(w, "canManage");
    w_bool(w, strcmp(actor->role, "admin") == 0 || owns_subject(actor, subject_id));
    w_key(w, "matrix");
    w_arr(w);
    {
        sqlite3_stmt* matrix = db_prep("SELECT chapter, easy_count, medium_count, hard_count FROM exam_matrix WHERE exam_id=? ORDER BY id");
        if (matrix) sqlite3_bind_int(matrix, 1, db_int(stmt, 0));
        while (matrix && sqlite3_step(matrix) == SQLITE_ROW) {
            w_obj(w);
            w_key(w, "chapter");
            w_str(w, db_text(matrix, 0));
            w_key(w, "easyCount");
            w_num(w, db_int(matrix, 1));
            w_key(w, "mediumCount");
            w_num(w, db_int(matrix, 2));
            w_key(w, "hardCount");
            w_num(w, db_int(matrix, 3));
            w_end(w);
        }
        sqlite3_finalize(matrix);
    }
    w_end(w);
    w_end(w);
}

void route_exams_list(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    stmt = db_prep(strcmp(actor.role, "lecturer") == 0 ? 
        "SELECT e.id, e.subject_id, e.title, e.description, e.start_time, e.end_time, e.duration_minutes, "
        "e.easy_count, e.medium_count, e.hard_count, e.total_questions, e.total_score, e.shuffle_questions, e.shuffle_answers, "
        "e.auto_submit, e.max_attempts, e.class_id, e.status, e.short_disconnect_sec, e.long_disconnect_sec, e.sync_grace_sec, "
        "s.code, s.name, IFNULL(p.full_name,''), IFNULL(c.name,''), (SELECT COUNT(*) FROM attempts a WHERE a.exam_id=e.id) "
        "FROM exams e JOIN subjects s ON s.id=e.subject_id LEFT JOIN lecturers l ON l.id=s.lecturer_id "
        "LEFT JOIN profiles p ON p.id=l.profile_id LEFT JOIN classes c ON c.id=e.class_id WHERE s.lecturer_id=? ORDER BY e.id DESC"
        :
        "SELECT e.id, e.subject_id, e.title, e.description, e.start_time, e.end_time, e.duration_minutes, "
        "e.easy_count, e.medium_count, e.hard_count, e.total_questions, e.total_score, e.shuffle_questions, e.shuffle_answers, "
        "e.auto_submit, e.max_attempts, e.class_id, e.status, e.short_disconnect_sec, e.long_disconnect_sec, e.sync_grace_sec, "
        "s.code, s.name, IFNULL(p.full_name,''), IFNULL(c.name,''), (SELECT COUNT(*) FROM attempts a WHERE a.exam_id=e.id) "
        "FROM exams e JOIN subjects s ON s.id=e.subject_id LEFT JOIN lecturers l ON l.id=s.lecturer_id "
        "LEFT JOIN profiles p ON p.id=l.profile_id LEFT JOIN classes c ON c.id=e.class_id ORDER BY e.id DESC");
    if (strcmp(actor.role, "lecturer") == 0 && stmt) sqlite3_bind_int(stmt, 1, actor.lecturer_id);
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) write_exam_row(&w, stmt, &actor);
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

static int bank_count(int subject_id, const char* chapter, const char* difficulty) {
    sqlite3_stmt* stmt = db_prep(
        "SELECT COUNT(*) FROM questions WHERE subject_id=? AND chapter=? AND difficulty=? AND is_active=1 "
        "AND EXISTS(SELECT 1 FROM answers a WHERE a.question_id=questions.id AND a.is_correct=1)");
    int value = 0;
    if (!stmt) return 0;
    sqlite3_bind_int(stmt, 1, subject_id);
    db_bind_text(stmt, 2, chapter);
    db_bind_text(stmt, 3, difficulty);
    if (sqlite3_step(stmt) == SQLITE_ROW) value = db_int(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

static int matrix_sums(const Js* body, int* easy, int* medium, int* hard) {
    const Js* matrix = js_get(body, "matrix");
    int i;
    int n;
    int e = 0;
    int m = 0;
    int h = 0;
    int any = 0;
    if (!matrix || matrix->type != JS_ARR) return 0;
    n = js_len(matrix);
    for (i = 0; i < n && i < 24; i++) {
        const Js* row = js_at(matrix, i);
        int re = (int)js_num(row, "easyCount", 0);
        int rm = (int)js_num(row, "mediumCount", 0);
        int rh = (int)js_num(row, "hardCount", 0);
        if (re + rm + rh <= 0) continue;
        any = 1;
        e += re;
        m += rm;
        h += rh;
    }
    if (!any) return 0;
    *easy = e;
    *medium = m;
    *hard = h;
    return 1;
}

static int matrix_json_ok(int subject_id, const Js* body) {
    const Js* matrix = js_get(body, "matrix");
    int i;
    int n;
    if (!matrix || matrix->type != JS_ARR) return 1;
    n = js_len(matrix);
    for (i = 0; i < n && i < 24; i++) {
        const Js* row = js_at(matrix, i);
        char chapter[120];
        int easy = (int)js_num(row, "easyCount", 0);
        int medium = (int)js_num(row, "mediumCount", 0);
        int hard = (int)js_num(row, "hardCount", 0);
        trim_copy(chapter, sizeof(chapter), js_str(row, "chapter", ""));
        if (!chapter[0] || easy + medium + hard <= 0) continue;
        if (easy < 0 || medium < 0 || hard < 0) return 0;
        if (bank_count(subject_id, chapter, "easy") < easy) return 0;
        if (bank_count(subject_id, chapter, "medium") < medium) return 0;
        if (bank_count(subject_id, chapter, "hard") < hard) return 0;
    }
    return 1;
}

static void save_matrix(int exam_id, const Js* body) {
    const Js* matrix = js_get(body, "matrix");
    sqlite3_stmt* stmt;
    int i;
    int n;
    if (scalar_int("SELECT COUNT(*) FROM attempts WHERE exam_id=?", exam_id, -1) > 0) return;
    stmt = db_prep("DELETE FROM exam_matrix WHERE exam_id=?");
    if (stmt) {
        sqlite3_bind_int(stmt, 1, exam_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (!matrix || matrix->type != JS_ARR) return;
    n = js_len(matrix);
    for (i = 0; i < n && i < 24; i++) {
        const Js* row = js_at(matrix, i);
        char chapter[120];
        int easy = (int)js_num(row, "easyCount", 0);
        int medium = (int)js_num(row, "mediumCount", 0);
        int hard = (int)js_num(row, "hardCount", 0);
        trim_copy(chapter, sizeof(chapter), js_str(row, "chapter", ""));
        if (!chapter[0] || easy + medium + hard <= 0) continue;
        stmt = db_prep("INSERT INTO exam_matrix(exam_id, chapter, easy_count, medium_count, hard_count) VALUES(?,?,?,?,?)");
        sqlite3_bind_int(stmt, 1, exam_id);
        db_bind_text(stmt, 2, chapter);
        sqlite3_bind_int(stmt, 3, easy);
        sqlite3_bind_int(stmt, 4, medium);
        sqlite3_bind_int(stmt, 5, hard);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

static int saved_matrix_ok(int exam_id, int subject_id) {
    sqlite3_stmt* stmt = db_prep("SELECT chapter, easy_count, medium_count, hard_count FROM exam_matrix WHERE exam_id=?");
    int rows = 0;
    int ok = 1;
    if (stmt) sqlite3_bind_int(stmt, 1, exam_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        char chapter[120];
        copy_str(chapter, sizeof(chapter), db_text(stmt, 0));
        rows++;
        if (bank_count(subject_id, chapter, "easy") < db_int(stmt, 1) ||
            bank_count(subject_id, chapter, "medium") < db_int(stmt, 2) ||
            bank_count(subject_id, chapter, "hard") < db_int(stmt, 3)) ok = 0;
    }
    sqlite3_finalize(stmt);
    if (!rows) return -1;
    return ok;
}

static int planned_score(int subject_id, int easy, int medium, int hard, double* total) {
    sqlite3_stmt* stmt = db_prep("SELECT difficulty, score FROM questions WHERE subject_id=? AND is_active=1 ORDER BY id");
    double easy_score = 1, medium_score = 1, hard_score = 1;
    int seen_e = 0, seen_m = 0, seen_h = 0;
    int have_e = 0, have_m = 0, have_h = 0;
    if (stmt) sqlite3_bind_int(stmt, 1, subject_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        const char* difficulty = db_text(stmt, 0);
        if (strcmp(difficulty, "easy") == 0) {
            have_e++;
            if (!seen_e) { easy_score = db_real(stmt, 1); seen_e = 1; }
        } else if (strcmp(difficulty, "medium") == 0) {
            have_m++;
            if (!seen_m) { medium_score = db_real(stmt, 1); seen_m = 1; }
        } else if (strcmp(difficulty, "hard") == 0) {
            have_h++;
            if (!seen_h) { hard_score = db_real(stmt, 1); seen_h = 1; }
        }
    }
    sqlite3_finalize(stmt);
    *total = easy * easy_score + medium * medium_score + hard * hard_score;
    return have_e >= easy && have_m >= medium && have_h >= hard;
}

static int read_exam_body(Request* request, Response* response, int existing, int* subject_id, char* title, char* description,
                          long long* start, long long* end, int* minutes, int* easy, int* medium, int* hard, int* max_attempts,
                          int* class_id, int* shuffle_q, int* shuffle_a, int* auto_submit, int* short_s, int* long_s, int* grace,
                          double* total) {
    int attempts = existing ? scalar_int("SELECT COUNT(*) FROM attempts WHERE exam_id=?", existing, -1) : 0;
    *subject_id = (int)js_num(request->json, "subjectId", 0);
    trim_copy(title, 200, js_str(request->json, "title", ""));
    trim_copy(description, 800, js_str(request->json, "description", ""));
    *start = (long long)js_num(request->json, "startTime", 0);
    *end = (long long)js_num(request->json, "endTime", 0);
    *minutes = (int)js_num(request->json, "durationMinutes", 0);
    *easy = (int)js_num(request->json, "easyCount", 0);
    *medium = (int)js_num(request->json, "mediumCount", 0);
    *hard = (int)js_num(request->json, "hardCount", 0);
    *max_attempts = (int)js_num(request->json, "maxAttempts", 1);
    *class_id = (int)js_num(request->json, "classId", 0);
    *shuffle_q = js_bool(request->json, "shuffleQuestions", 1);
    *shuffle_a = js_bool(request->json, "shuffleAnswers", 1);
    *auto_submit = js_bool(request->json, "autoSubmit", 1);
    *short_s = (int)js_num(request->json, "shortDisconnectSec", 30);
    *long_s = (int)js_num(request->json, "longDisconnectSec", 300);
    *grace = (int)js_num(request->json, "syncGraceSec", 300);
    if (attempts > 0) {
        sqlite3_stmt* stmt = db_prep("SELECT subject_id, duration_minutes, easy_count, medium_count, hard_count, shuffle_questions, shuffle_answers FROM exams WHERE id=?");
        sqlite3_bind_int(stmt, 1, existing);
        if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
            *subject_id = db_int(stmt, 0);
            *minutes = db_int(stmt, 1);
            *easy = db_int(stmt, 2);
            *medium = db_int(stmt, 3);
            *hard = db_int(stmt, 4);
            *shuffle_q = db_int(stmt, 5);
            *shuffle_a = db_int(stmt, 6);
        }
        sqlite3_finalize(stmt);
    } else {
        matrix_sums(request->json, easy, medium, hard);
    }
    if (!title[0] || strlen(title) > 180) {
        reply_fail(response, 400, "Thiếu tên kỳ thi");
        return 0;
    }
    if (*start <= 0 || *end <= *start) {
        reply_fail(response, 400, "Thời gian mở và đóng kỳ thi không hợp lệ");
        return 0;
    }
    if (*minutes < 1 || *minutes > 300) {
        reply_fail(response, 400, "Thời lượng từ 1 đến 300 phút");
        return 0;
    }
    if (*easy < 0 || *medium < 0 || *hard < 0 || *easy + *medium + *hard < 1 || *easy + *medium + *hard > 60) {
        reply_fail(response, 400, "Ma trận đề phải có từ 1 đến 60 câu");
        return 0;
    }
    if (*max_attempts < 1 || *max_attempts > 10) {
        reply_fail(response, 400, "Số lần thi từ 1 đến 10");
        return 0;
    }
    if (*class_id && scalar_int("SELECT COUNT(*) FROM classes WHERE id=?", *class_id, -1) == 0) {
        reply_fail(response, 400, "Lớp không tồn tại");
        return 0;
    }
    if (*short_s < 5 || *short_s > 180) {
        reply_fail(response, 400, "Ngưỡng mất kết nối ngắn không hợp lệ");
        return 0;
    }
    if (*long_s <= *short_s || *long_s > 1800) {
        reply_fail(response, 400, "Ngưỡng mất kết nối dài phải lớn hơn ngưỡng ngắn");
        return 0;
    }
    if (*grace < 30 || *grace > 900) {
        reply_fail(response, 400, "Thời gian ân hạn đồng bộ từ 30 đến 900 giây");
        return 0;
    }
    if (scalar_int("SELECT COUNT(*) FROM subjects WHERE id=?", *subject_id, -1) == 0) {
        reply_fail(response, 400, "Môn học không tồn tại");
        return 0;
    }
    planned_score(*subject_id, *easy, *medium, *hard, total);
    return 1;
}

static void finish_exam_id(Response* response, int exam_id) {
    W w;
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "id");
    w_num(&w, exam_id);
    w_end(&w);
    reply_json(response, &w);
}

void route_exam_create(Request* request, Response* response) {
    Actor actor;
    int subject_id, minutes, easy, medium, hard, max_attempts, class_id, shuffle_q, shuffle_a, auto_submit, short_s, long_s, grace;
    long long start, end;
    double total = 0;
    char title[200];
    char description[800];
    sqlite3_stmt* stmt;
    int exam_id;
    int publish;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!read_exam_body(request, response, 0, &subject_id, title, description, &start, &end, &minutes, &easy, &medium, &hard,
                        &max_attempts, &class_id, &shuffle_q, &shuffle_a, &auto_submit, &short_s, &long_s, &grace, &total)) return;
    if (!owns_subject(&actor, subject_id)) {
        reply_fail(response, 403, "Bạn không tạo kỳ thi cho môn này");
        return;
    }
    publish = js_bool(request->json, "publish", 0);
    if (publish && matrix_sums(request->json, &easy, &medium, &hard) && !matrix_json_ok(subject_id, request->json)) {
        reply_fail(response, 400, "Ngân hàng không đủ câu cho ma trận theo chương");
        return;
    }
    if (publish && !planned_score(subject_id, easy, medium, hard, &total)) {
        reply_fail(response, 400, "Ngân hàng không đủ câu cho ma trận đề");
        return;
    }
    planned_score(subject_id, easy, medium, hard, &total);
    stmt = db_prep(
        "INSERT INTO exams(subject_id, title, description, start_time, end_time, duration_minutes, easy_count, medium_count, hard_count, "
        "total_questions, total_score, shuffle_questions, shuffle_answers, auto_submit, max_attempts, status, class_id, "
        "short_disconnect_sec, long_disconnect_sec, sync_grace_sec, created_by, created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    sqlite3_bind_int(stmt, 1, subject_id);
    db_bind_text(stmt, 2, title);
    db_bind_text(stmt, 3, description);
    sqlite3_bind_int64(stmt, 4, start);
    sqlite3_bind_int64(stmt, 5, end);
    sqlite3_bind_int(stmt, 6, minutes);
    sqlite3_bind_int(stmt, 7, easy);
    sqlite3_bind_int(stmt, 8, medium);
    sqlite3_bind_int(stmt, 9, hard);
    sqlite3_bind_int(stmt, 10, easy + medium + hard);
    sqlite3_bind_double(stmt, 11, total);
    sqlite3_bind_int(stmt, 12, shuffle_q);
    sqlite3_bind_int(stmt, 13, shuffle_a);
    sqlite3_bind_int(stmt, 14, auto_submit);
    sqlite3_bind_int(stmt, 15, max_attempts);
    db_bind_text(stmt, 16, publish ? "published" : "draft");
    sqlite3_bind_int(stmt, 17, class_id);
    sqlite3_bind_int(stmt, 18, short_s);
    sqlite3_bind_int(stmt, 19, long_s);
    sqlite3_bind_int(stmt, 20, grace);
    sqlite3_bind_int(stmt, 21, actor.user_id);
    sqlite3_bind_int64(stmt, 22, now_sec());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    exam_id = (int)sqlite3_last_insert_rowid(g_db);
    save_matrix(exam_id, request->json);
    audit_add(actor.user_id, "CREATE_EXAM", title, request->ip);
    finish_exam_id(response, exam_id);
}

void route_exam_update(Request* request, Response* response) {
    Actor actor;
    int subject_id, minutes, easy, medium, hard, max_attempts, class_id, shuffle_q, shuffle_a, auto_submit, short_s, long_s, grace;
    long long start, end;
    double total = 0;
    char title[200];
    char description[800];
    sqlite3_stmt* stmt;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!can_manage_exam(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không sửa được kỳ thi này");
        return;
    }
    if (!read_exam_body(request, response, request->id, &subject_id, title, description, &start, &end, &minutes, &easy, &medium, &hard,
                        &max_attempts, &class_id, &shuffle_q, &shuffle_a, &auto_submit, &short_s, &long_s, &grace, &total)) return;
    stmt = db_prep(
        "UPDATE exams SET subject_id=?, title=?, description=?, start_time=?, end_time=?, duration_minutes=?, easy_count=?, medium_count=?, "
        "hard_count=?, total_questions=?, total_score=?, shuffle_questions=?, shuffle_answers=?, auto_submit=?, max_attempts=?, class_id=?, "
        "short_disconnect_sec=?, long_disconnect_sec=?, sync_grace_sec=? WHERE id=?");
    sqlite3_bind_int(stmt, 1, subject_id);
    db_bind_text(stmt, 2, title);
    db_bind_text(stmt, 3, description);
    sqlite3_bind_int64(stmt, 4, start);
    sqlite3_bind_int64(stmt, 5, end);
    sqlite3_bind_int(stmt, 6, minutes);
    sqlite3_bind_int(stmt, 7, easy);
    sqlite3_bind_int(stmt, 8, medium);
    sqlite3_bind_int(stmt, 9, hard);
    sqlite3_bind_int(stmt, 10, easy + medium + hard);
    sqlite3_bind_double(stmt, 11, total);
    sqlite3_bind_int(stmt, 12, shuffle_q);
    sqlite3_bind_int(stmt, 13, shuffle_a);
    sqlite3_bind_int(stmt, 14, auto_submit);
    sqlite3_bind_int(stmt, 15, max_attempts);
    sqlite3_bind_int(stmt, 16, class_id);
    sqlite3_bind_int(stmt, 17, short_s);
    sqlite3_bind_int(stmt, 18, long_s);
    sqlite3_bind_int(stmt, 19, grace);
    sqlite3_bind_int(stmt, 20, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    save_matrix(request->id, request->json);
    audit_add(actor.user_id, "UPDATE_EXAM", title, request->ip);
    finish_exam_id(response, request->id);
}

void route_exam_publish(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    int subject_id, easy, medium, hard;
    double total = 0;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!can_manage_exam(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không mở được kỳ thi này");
        return;
    }
    stmt = db_prep("SELECT subject_id, easy_count, medium_count, hard_count FROM exams WHERE id=?");
    sqlite3_bind_int(stmt, 1, request->id);
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không tìm thấy kỳ thi");
        return;
    }
    subject_id = db_int(stmt, 0);
    easy = db_int(stmt, 1);
    medium = db_int(stmt, 2);
    hard = db_int(stmt, 3);
    sqlite3_finalize(stmt);
    {
        int matrix_ok = saved_matrix_ok(request->id, subject_id);
        if (matrix_ok == 0 || (matrix_ok < 0 && !planned_score(subject_id, easy, medium, hard, &total))) {
            reply_fail(response, 400, "Ngân hàng không đủ câu cho ma trận đề");
            return;
        }
    }
    if (!planned_score(subject_id, easy, medium, hard, &total)) {
        reply_fail(response, 400, "Ngân hàng không đủ câu cho ma trận đề");
        return;
    }
    stmt = db_prep("UPDATE exams SET status='published', total_score=? WHERE id=?");
    sqlite3_bind_double(stmt, 1, total);
    sqlite3_bind_int(stmt, 2, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    audit_add(actor.user_id, "PUBLISH_EXAM", "Mở kỳ thi", request->ip);
    finish_exam_id(response, request->id);
}

void route_exam_close(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!can_manage_exam(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không đóng được kỳ thi này");
        return;
    }
    stmt = db_prep("UPDATE exams SET status='closed' WHERE id=?");
    sqlite3_bind_int(stmt, 1, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    audit_add(actor.user_id, "CLOSE_EXAM", "Đóng kỳ thi", request->ip);
    finish_exam_id(response, request->id);
}

void route_exam_delete(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!can_manage_exam(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không xóa được kỳ thi này");
        return;
    }
    stmt = db_prep("SELECT status FROM exams WHERE id=?");
    sqlite3_bind_int(stmt, 1, request->id);
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không tìm thấy kỳ thi");
        return;
    }
    if (strcmp(db_text(stmt, 0), "draft") != 0 || scalar_int("SELECT COUNT(*) FROM attempts WHERE exam_id=?", request->id, -1) > 0) {
        sqlite3_finalize(stmt);
        reply_fail(response, 400, "Chỉ xóa được kỳ thi nháp chưa có bài làm");
        return;
    }
    sqlite3_finalize(stmt);
    stmt = db_prep("DELETE FROM exams WHERE id=?");
    sqlite3_bind_int(stmt, 1, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    audit_add(actor.user_id, "DELETE_EXAM", "Xóa kỳ thi nháp", request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_exam_analysis(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    int count = 0;
    double sum = 0, highest = 0, lowest = 0;
    int any = 0;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!can_manage_exam(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không xem được thống kê này");
        return;
    }
    stmt = db_prep("SELECT score FROM attempts WHERE exam_id=? AND status IN ('GRADED','AUTO_SUBMITTED','EXPIRED')");
    sqlite3_bind_int(stmt, 1, request->id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        double score = db_real(stmt, 0);
        if (!any || score > highest) highest = score;
        if (!any || score < lowest) lowest = score;
        sum += score;
        count++;
        any = 1;
    }
    sqlite3_finalize(stmt);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "examId");
    w_num(&w, request->id);
    w_key(&w, "title");
    {
        sqlite3_stmt* title = db_prep("SELECT title FROM exams WHERE id=?");
        sqlite3_bind_int(title, 1, request->id);
        w_str(&w, title && sqlite3_step(title) == SQLITE_ROW ? db_text(title, 0) : "");
        sqlite3_finalize(title);
    }
    w_key(&w, "gradedCount");
    w_num(&w, count);
    w_key(&w, "average");
    w_num(&w, count ? sum / count : 0);
    w_key(&w, "highest");
    w_num(&w, any ? highest : 0);
    w_key(&w, "lowest");
    w_num(&w, any ? lowest : 0);
    w_key(&w, "questions");
    w_arr(&w);
    stmt = db_prep(
        "SELECT q.text, COUNT(*), SUM(CASE WHEN a.is_correct=1 THEN 1 ELSE 0 END) "
        "FROM attempts t JOIN attempt_questions aq ON aq.attempt_id=t.id "
        "JOIN questions q ON q.id=aq.question_id "
        "LEFT JOIN student_answers sa ON sa.attempt_id=t.id AND sa.question_id=aq.question_id "
        "LEFT JOIN answers a ON a.id=sa.answer_id "
        "WHERE t.exam_id=? AND t.status IN ('GRADED','AUTO_SUBMITTED','EXPIRED') "
        "GROUP BY aq.question_id ORDER BY MIN(aq.order_index)");
    sqlite3_bind_int(stmt, 1, request->id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        int seen = db_int(stmt, 1);
        int correct = db_int(stmt, 2);
        w_obj(&w);
        w_key(&w, "text");
        w_str(&w, db_text(stmt, 0));
        w_key(&w, "seen");
        w_num(&w, seen);
        w_key(&w, "correct");
        w_num(&w, correct);
        w_key(&w, "correctRate");
        w_num(&w, seen ? (100.0 * correct / seen) : 0);
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_exam_results(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!can_manage_exam(&actor, request->id)) {
        reply_fail(response, 403, "Bạn không xem được kết quả này");
        return;
    }
    stmt = db_prep(
        "SELECT t.id, p.full_name, s.student_code, s.class_name, t.paper_code, t.status, t.score, t.paper_total, t.submit_time, "
        "t.correct_count, t.wrong_count, t.skipped, t.start_time "
        "FROM attempts t JOIN students s ON s.id=t.student_id JOIN profiles p ON p.id=s.profile_id "
        "WHERE t.exam_id=? ORDER BY p.full_name");
    sqlite3_bind_int(stmt, 1, request->id);
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "attemptId");
        w_num(&w, db_int(stmt, 0));
        w_key(&w, "fullName");
        w_str(&w, db_text(stmt, 1));
        w_key(&w, "studentCode");
        w_str(&w, db_text(stmt, 2));
        w_key(&w, "className");
        w_str(&w, db_text(stmt, 3));
        w_key(&w, "paperCode");
        w_str(&w, db_text(stmt, 4));
        w_key(&w, "status");
        w_str(&w, db_text(stmt, 5));
        w_key(&w, "score");
        w_num(&w, db_real(stmt, 6));
        w_key(&w, "total");
        w_num(&w, db_real(stmt, 7));
        w_key(&w, "submitTime");
        w_num(&w, (double)db_i64(stmt, 8));
        w_key(&w, "correctAnswers");
        w_num(&w, db_int(stmt, 9));
        w_key(&w, "wrongAnswers");
        w_num(&w, db_int(stmt, 10));
        w_key(&w, "skipped");
        w_num(&w, db_int(stmt, 11));
        w_key(&w, "startTime");
        w_num(&w, (double)db_i64(stmt, 12));
        w_key(&w, "tab");
        w_num(&w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='TAB'", db_int(stmt, 0), -1));
        w_key(&w, "reconnect");
        w_num(&w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='RECONNECT'", db_int(stmt, 0), -1));
        w_key(&w, "offline");
        w_num(&w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='OFFLINE'", db_int(stmt, 0), -1));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}
