#include "app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_final(const char* status) {
    return strcmp(status, "GRADED") == 0 || strcmp(status, "AUTO_SUBMITTED") == 0 ||
           strcmp(status, "EXPIRED") == 0 || strcmp(status, "SUBMITTED") == 0;
}

static void score_text(char* buf, size_t cap, double value) {
    if (value > -0.0001 && value < 0.0001) snprintf(buf, cap, "0");
    else if (fabs(value - (int)value) < 0.0001) snprintf(buf, cap, "%d", (int)value);
    else snprintf(buf, cap, "%.1f", value);
}

static void finalize_attempt(int attempt_id, int automatic) {
    sqlite3_stmt* stmt;
    char status[32];
    int student_id = 0;
    int exam_id = 0;
    double total = 0;
    double score = 0;
    int correct = 0;
    int wrong = 0;
    int skipped = 0;
    int any = 0;
    char next[32];
    long long now = now_sec();
    stmt = db_prep("SELECT status, student_id, exam_id FROM attempts WHERE id=?");
    if (!stmt) return;
    sqlite3_bind_int(stmt, 1, attempt_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return;
    }
    copy_str(status, sizeof(status), db_text(stmt, 0));
    student_id = db_int(stmt, 1);
    exam_id = db_int(stmt, 2);
    sqlite3_finalize(stmt);
    if (is_final(status)) return;

    stmt = db_prep(
        "SELECT aq.question_id, q.score, IFNULL(sa.answer_id,0), IFNULL(a.is_correct,0) "
        "FROM attempt_questions aq JOIN questions q ON q.id=aq.question_id "
        "LEFT JOIN student_answers sa ON sa.attempt_id=aq.attempt_id AND sa.question_id=aq.question_id "
        "LEFT JOIN answers a ON a.id=sa.answer_id WHERE aq.attempt_id=?");
    sqlite3_bind_int(stmt, 1, attempt_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        int chosen = db_int(stmt, 2);
        total += db_real(stmt, 1);
        if (!chosen) skipped++;
        else if (db_int(stmt, 3)) {
            score += db_real(stmt, 1);
            correct++;
            any = 1;
        } else {
            wrong++;
            any = 1;
        }
    }
    sqlite3_finalize(stmt);
    if (!any && automatic) copy_str(next, sizeof(next), "EXPIRED");
    else if (automatic) copy_str(next, sizeof(next), "AUTO_SUBMITTED");
    else copy_str(next, sizeof(next), "GRADED");

    stmt = db_prep(
        "UPDATE attempts SET status=?, score=?, paper_total=?, correct_count=?, wrong_count=?, skipped=?, "
        "is_auto=?, submit_time=?, end_time=? WHERE id=?");
    db_bind_text(stmt, 1, next);
    sqlite3_bind_double(stmt, 2, score);
    sqlite3_bind_double(stmt, 3, total);
    sqlite3_bind_int(stmt, 4, correct);
    sqlite3_bind_int(stmt, 5, wrong);
    sqlite3_bind_int(stmt, 6, skipped);
    sqlite3_bind_int(stmt, 7, automatic);
    sqlite3_bind_int64(stmt, 8, now);
    sqlite3_bind_int64(stmt, 9, now);
    sqlite3_bind_int(stmt, 10, attempt_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (scalar_int("SELECT COUNT(*) FROM results WHERE attempt_id=?", attempt_id, -1) == 0) {
        int user_id = scalar_int("SELECT user_id FROM students WHERE id=?", student_id, -1);
        char title[200];
        char score_buf[32];
        char total_buf[32];
        char message[320];
        sqlite3_stmt* title_stmt = db_prep("SELECT title FROM exams WHERE id=?");
        sqlite3_bind_int(title_stmt, 1, exam_id);
        copy_str(title, sizeof(title), title_stmt && sqlite3_step(title_stmt) == SQLITE_ROW ? db_text(title_stmt, 0) : "bài thi");
        sqlite3_finalize(title_stmt);
        stmt = db_prep("INSERT INTO results(attempt_id, student_id, exam_id, score, total, percent, graded_at) VALUES(?,?,?,?,?,?,?)");
        sqlite3_bind_int(stmt, 1, attempt_id);
        sqlite3_bind_int(stmt, 2, student_id);
        sqlite3_bind_int(stmt, 3, exam_id);
        sqlite3_bind_double(stmt, 4, score);
        sqlite3_bind_double(stmt, 5, total);
        sqlite3_bind_double(stmt, 6, total > 0 ? 100.0 * score / total : 0);
        sqlite3_bind_int64(stmt, 7, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        score_text(score_buf, sizeof(score_buf), score);
        score_text(total_buf, sizeof(total_buf), total);
        snprintf(message, sizeof(message), "Đã chấm %s: %s/%s", title, score_buf, total_buf);
        if (user_id) notify_user(user_id, message);
        audit_add(user_id, automatic ? "AUTO_SUBMIT" : "SUBMIT", "Chấm lượt thi", "server");
    }
}

void attempt_sweep(void) {
    sqlite3_stmt* stmt = db_prep(
        "SELECT a.id FROM attempts a JOIN exams e ON e.id=a.exam_id "
        "WHERE a.status NOT IN ('GRADED','AUTO_SUBMITTED','EXPIRED','SUBMITTED') AND a.deadline + e.sync_grace_sec < ?");
    int ids[128];
    int n = 0;
    int i;
    if (stmt) sqlite3_bind_int64(stmt, 1, now_sec());
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW && n < 128) ids[n++] = db_int(stmt, 0);
    sqlite3_finalize(stmt);
    for (i = 0; i < n; i++) finalize_attempt(ids[i], 1);
}

static int load_attempt_student(int attempt_id, int* student_id, int* exam_id, long long* deadline, char* status, size_t status_cap) {
    sqlite3_stmt* stmt = db_prep("SELECT student_id, exam_id, deadline, status FROM attempts WHERE id=?");
    int ok = 0;
    if (!stmt) return 0;
    sqlite3_bind_int(stmt, 1, attempt_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *student_id = db_int(stmt, 0);
        *exam_id = db_int(stmt, 1);
        *deadline = db_i64(stmt, 2);
        copy_str(status, status_cap, db_text(stmt, 3));
        ok = 1;
    }
    sqlite3_finalize(stmt);
    return ok;
}

static void close_if_late(int attempt_id) {
    int student_id = 0;
    int exam_id = 0;
    long long deadline = 0;
    char status[32];
    int grace;
    if (!load_attempt_student(attempt_id, &student_id, &exam_id, &deadline, status, sizeof(status))) return;
    if (is_final(status)) return;
    grace = scalar_int("SELECT sync_grace_sec FROM exams WHERE id=?", exam_id, -1);
    if (grace <= 0) grace = 300;
    if (now_sec() > deadline + grace) finalize_attempt(attempt_id, 1);
}

static void write_clock_fields(W* w, int attempt_id) {
    sqlite3_stmt* stmt = db_prep(
        "SELECT a.status, a.deadline, a.paper_code, a.score, a.paper_total, a.is_auto, a.start_time, e.id, e.title, e.duration_minutes, "
        "e.short_disconnect_sec, e.long_disconnect_sec, e.sync_grace_sec, IFNULL(s.name,'') "
        "FROM attempts a JOIN exams e ON e.id=a.exam_id LEFT JOIN subjects s ON s.id=e.subject_id WHERE a.id=?");
    long long now = now_sec();
    sqlite3_bind_int(stmt, 1, attempt_id);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        int final = is_final(db_text(stmt, 0));
        long long deadline = db_i64(stmt, 1);
        long long remaining = (!final && deadline > now) ? deadline - now : 0;
        w_key(w, "attemptId");
        w_num(w, attempt_id);
        w_key(w, "examId");
        w_num(w, db_int(stmt, 7));
        w_key(w, "examTitle");
        w_str(w, db_text(stmt, 8));
        w_key(w, "subjectName");
        w_str(w, db_text(stmt, 13));
        w_key(w, "paperCode");
        w_str(w, db_text(stmt, 2));
        w_key(w, "status");
        w_str(w, db_text(stmt, 0));
        w_key(w, "finalized");
        w_bool(w, final);
        w_key(w, "serverTime");
        w_num(w, (double)now);
        w_key(w, "startTime");
        w_num(w, (double)db_i64(stmt, 6));
        w_key(w, "deadline");
        w_num(w, (double)deadline);
        w_key(w, "remainingSec");
        w_num(w, (double)remaining);
        w_key(w, "durationMinutes");
        w_num(w, db_int(stmt, 9));
        w_key(w, "score");
        w_num(w, db_real(stmt, 3));
        w_key(w, "total");
        w_num(w, db_real(stmt, 4));
        w_key(w, "isAutoSubmitted");
        w_bool(w, db_int(stmt, 5));
        w_key(w, "policy");
        w_obj(w);
        w_key(w, "shortSec");
        w_num(w, db_int(stmt, 10));
        w_key(w, "longSec");
        w_num(w, db_int(stmt, 11));
        w_key(w, "graceSec");
        w_num(w, db_int(stmt, 12));
        w_end(w);
        w_key(w, "autosaveSec");
        w_num(w, setting_int("autosaveSec", 8));
    }
    sqlite3_finalize(stmt);
}

static void write_clock(W* w, int attempt_id) {
    w_obj(w);
    write_clock_fields(w, attempt_id);
    w_end(w);
}

static void write_questions(W* w, int attempt_id, int with_correct) {
    sqlite3_stmt* questions = db_prep(
        "SELECT aq.question_id, aq.order_index, q.text, q.score, IFNULL(q.explanation,'') FROM attempt_questions aq "
        "JOIN questions q ON q.id=aq.question_id WHERE aq.attempt_id=? ORDER BY aq.order_index");
    sqlite3_bind_int(questions, 1, attempt_id);
    w_key(w, "questions");
    w_arr(w);
    while (questions && sqlite3_step(questions) == SQLITE_ROW) {
        int question_id = db_int(questions, 0);
        int chosen = 0;
        int got = 0;
        sqlite3_stmt* chosen_stmt = db_prep("SELECT answer_id FROM student_answers WHERE attempt_id=? AND question_id=?");
        sqlite3_stmt* answers;
        sqlite3_bind_int(chosen_stmt, 1, attempt_id);
        sqlite3_bind_int(chosen_stmt, 2, question_id);
        if (chosen_stmt && sqlite3_step(chosen_stmt) == SQLITE_ROW) chosen = db_int(chosen_stmt, 0);
        sqlite3_finalize(chosen_stmt);
        w_obj(w);
        w_key(w, "questionId");
        w_num(w, question_id);
        w_key(w, "order");
        w_num(w, db_int(questions, 1));
        w_key(w, "text");
        w_str(w, db_text(questions, 2));
        w_key(w, "score");
        w_num(w, db_real(questions, 3));
        if (with_correct && db_text(questions, 4)[0]) {
            w_key(w, "explanation");
            w_str(w, db_text(questions, 4));
        }
        w_key(w, "answers");
        w_arr(w);
        answers = db_prep(
            "SELECT c.answer_id, a.text, a.is_correct FROM attempt_choices c JOIN answers a ON a.id=c.answer_id "
            "WHERE c.attempt_id=? AND c.question_id=? ORDER BY c.position");
        sqlite3_bind_int(answers, 1, attempt_id);
        sqlite3_bind_int(answers, 2, question_id);
        while (answers && sqlite3_step(answers) == SQLITE_ROW) {
            int answer_id = db_int(answers, 0);
            int correct = db_int(answers, 2);
            w_obj(w);
            w_key(w, "answerId");
            w_num(w, answer_id);
            w_key(w, "text");
            w_str(w, db_text(answers, 1));
            if (with_correct) {
                w_key(w, "chosen");
                w_bool(w, answer_id == chosen);
                w_key(w, "correct");
                w_bool(w, correct);
                if (answer_id == chosen && correct) got = 1;
            }
            w_end(w);
        }
        sqlite3_finalize(answers);
        w_end(w);
        if (with_correct) {
            w_key(w, "gotPoint");
            w_bool(w, got);
        }
        w_end(w);
    }
    sqlite3_finalize(questions);
    w_end(w);
}

static void write_paper(W* w, int attempt_id, int with_correct) {
    sqlite3_stmt* saved;
    w_obj(w);
    write_clock_fields(w, attempt_id);
    if (!with_correct) {
        int student_id = 0, exam_id = 0;
        long long deadline = 0;
        char status[32];
        load_attempt_student(attempt_id, &student_id, &exam_id, &deadline, status, sizeof(status));
        if (!is_final(status)) write_questions(w, attempt_id, 0);
        w_key(w, "savedAnswers");
        w_arr(w);
        saved = db_prep("SELECT question_id, answer_id FROM student_answers WHERE attempt_id=?");
        sqlite3_bind_int(saved, 1, attempt_id);
        while (saved && sqlite3_step(saved) == SQLITE_ROW) {
            w_obj(w);
            w_key(w, "questionId");
            w_num(w, db_int(saved, 0));
            w_key(w, "answerId");
            w_num(w, db_int(saved, 1));
            w_end(w);
        }
        sqlite3_finalize(saved);
        w_end(w);
    } else {
        sqlite3_stmt* meta = db_prep(
            "SELECT a.correct_count, a.wrong_count, a.skipped, a.submit_time, p.full_name, s.student_code "
            "FROM attempts a JOIN students s ON s.id=a.student_id JOIN profiles p ON p.id=s.profile_id WHERE a.id=?");
        sqlite3_bind_int(meta, 1, attempt_id);
        if (meta && sqlite3_step(meta) == SQLITE_ROW) {
            w_key(w, "correctAnswers");
            w_num(w, db_int(meta, 0));
            w_key(w, "wrongAnswers");
            w_num(w, db_int(meta, 1));
            w_key(w, "skipped");
            w_num(w, db_int(meta, 2));
            w_key(w, "submitTime");
            w_num(w, (double)db_i64(meta, 3));
            w_key(w, "studentName");
            w_str(w, db_text(meta, 4));
        w_key(w, "studentCode");
        w_str(w, db_text(meta, 5));
        }
        sqlite3_finalize(meta);
        w_key(w, "monitor");
        w_obj(w);
        w_key(w, "tab");
        w_num(w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='TAB'", attempt_id, -1));
        w_key(w, "blur");
        w_num(w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='BLUR'", attempt_id, -1));
        w_key(w, "refresh");
        w_num(w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='REFRESH'", attempt_id, -1));
        w_key(w, "reconnect");
        w_num(w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='RECONNECT'", attempt_id, -1));
        w_key(w, "offline");
        w_num(w, scalar_int("SELECT COUNT(*) FROM exam_events WHERE attempt_id=? AND kind='OFFLINE'", attempt_id, -1));
        w_end(w);
        write_questions(w, attempt_id, 1);
    }
    w_end(w);
}

static int apply_answers(int attempt_id, const Js* body, char* err, size_t err_cap) {
    const Js* answers = js_get(body, "answers");
    int pairs[64][2];
    int n = 0;
    int i;
    if (!answers) return 1;
    if (answers->type != JS_ARR) {
        snprintf(err, err_cap, "Danh sách đáp án không hợp lệ");
        return 0;
    }
    for (i = 0; i < js_len(answers) && n < 64; i++) {
        const Js* item = js_at(answers, i);
        int question_id = (int)js_num(item, "questionId", 0);
        int answer_id = (int)js_num(item, "answerId", 0);
        if (!question_id || !answer_id) continue;
        if (scalar_int("SELECT COUNT(*) FROM attempt_questions WHERE attempt_id=? AND question_id=?", attempt_id, question_id) == 0 ||
            scalar_int("SELECT COUNT(*) FROM attempt_choices WHERE attempt_id=? AND question_id=? AND answer_id=?", attempt_id, -1) == 0) {
            sqlite3_stmt* stmt = db_prep("SELECT COUNT(*) FROM attempt_choices WHERE attempt_id=? AND question_id=? AND answer_id=?");
            int ok = 0;
            sqlite3_bind_int(stmt, 1, attempt_id);
            sqlite3_bind_int(stmt, 2, question_id);
            sqlite3_bind_int(stmt, 3, answer_id);
            if (stmt && sqlite3_step(stmt) == SQLITE_ROW) ok = db_int(stmt, 0);
            sqlite3_finalize(stmt);
            if (!ok) {
                snprintf(err, err_cap, "Đáp án không thuộc đề của bạn");
                return 0;
            }
        }
        pairs[n][0] = question_id;
        pairs[n][1] = answer_id;
        n++;
    }
    for (i = 0; i < n; i++) {
        sqlite3_stmt* stmt = db_prep(
            "INSERT INTO student_answers(attempt_id, question_id, answer_id, updated_at) VALUES(?,?,?,?) "
            "ON CONFLICT(attempt_id, question_id) DO UPDATE SET answer_id=excluded.answer_id, updated_at=excluded.updated_at");
        sqlite3_bind_int(stmt, 1, attempt_id);
        sqlite3_bind_int(stmt, 2, pairs[i][0]);
        sqlite3_bind_int(stmt, 3, pairs[i][1]);
        sqlite3_bind_int64(stmt, 4, now_sec());
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    return 1;
}

static int require_student_attempt(Request* request, Response* response, Actor* actor) {
    int student_id = 0, exam_id = 0;
    long long deadline = 0;
    char status[32];
    if (!require_role(request, response, actor, "student")) return 0;
    if (!load_attempt_student(request->id, &student_id, &exam_id, &deadline, status, sizeof(status)) || student_id != actor->student_id) {
        reply_fail(response, 404, "Không tìm thấy lượt thi");
        return 0;
    }
    return 1;
}

void route_attempt_paper(Request* request, Response* response) {
    Actor actor;
    W w;
    if (!require_student_attempt(request, response, &actor)) return;
    close_if_late(request->id);
    reply_begin(&w);
    write_paper(&w, request->id, 0);
    reply_json(response, &w);
}

void route_attempt_time(Request* request, Response* response) {
    Actor actor;
    int student_id = 0, exam_id = 0;
    long long deadline = 0;
    char status[32];
    W w;
    if (!require_student_attempt(request, response, &actor)) return;
    load_attempt_student(request->id, &student_id, &exam_id, &deadline, status, sizeof(status));
    if (!is_final(status)) {
        int grace = scalar_int("SELECT sync_grace_sec FROM exams WHERE id=?", exam_id, -1);
        sqlite3_stmt* stmt;
        if (now_sec() > deadline + (grace > 0 ? grace : 300)) finalize_attempt(request->id, 1);
        else {
            stmt = db_prep("UPDATE attempts SET last_sync=? WHERE id=?");
            sqlite3_bind_int64(stmt, 1, now_sec());
            sqlite3_bind_int(stmt, 2, request->id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    reply_begin(&w);
    write_clock(&w, request->id);
    reply_json(response, &w);
}

static void sync_attempt(Request* request, Response* response, int finalize_requested) {
    Actor actor;
    int student_id = 0, exam_id = 0;
    long long deadline = 0;
    long long last_sync = 0;
    char status[32];
    int grace;
    int short_sec;
    long long now;
    char err[160];
    W w;
    sqlite3_stmt* stmt;
    if (!require_student_attempt(request, response, &actor)) return;
    stmt = db_prep("SELECT last_sync FROM attempts WHERE id=?");
    sqlite3_bind_int(stmt, 1, request->id);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) last_sync = db_i64(stmt, 0);
    sqlite3_finalize(stmt);
    load_attempt_student(request->id, &student_id, &exam_id, &deadline, status, sizeof(status));
    if (is_final(status)) {
        reply_begin(&w);
        write_clock(&w, request->id);
        reply_json(response, &w);
        return;
    }
    grace = scalar_int("SELECT sync_grace_sec FROM exams WHERE id=?", exam_id, -1);
    short_sec = scalar_int("SELECT short_disconnect_sec FROM exams WHERE id=?", exam_id, -1);
    now = now_sec();
    if (last_sync > 0 && now - last_sync > short_sec) audit_add(actor.user_id, "RECONNECT", "Nối lại bài thi sau khi mất kết nối", request->ip);
    if (now > deadline + grace) {
        finalize_attempt(request->id, 1);
        reply_begin(&w);
        write_clock(&w, request->id);
        reply_json(response, &w);
        return;
    }
    if (!apply_answers(request->id, request->json, err, sizeof(err))) {
        reply_fail(response, 400, err);
        return;
    }
    stmt = db_prep("UPDATE attempts SET last_sync=? WHERE id=?");
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_int(stmt, 2, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (now >= deadline || finalize_requested) finalize_attempt(request->id, now >= deadline);
    reply_begin(&w);
    write_clock(&w, request->id);
    reply_json(response, &w);
}

void route_attempt_sync(Request* request, Response* response) { sync_attempt(request, response, 0); }
void route_attempt_submit(Request* request, Response* response) { sync_attempt(request, response, 1); }

void route_attempt_result(Request* request, Response* response) {
    Actor actor;
    int student_id = 0, exam_id = 0;
    long long deadline = 0;
    char status[32];
    int subject_id;
    W w;
    if (!require_user(request, response, &actor)) return;
    if (!load_attempt_student(request->id, &student_id, &exam_id, &deadline, status, sizeof(status))) {
        reply_fail(response, 404, "Không tìm thấy lượt thi");
        return;
    }
    subject_id = scalar_int("SELECT subject_id FROM exams WHERE id=?", exam_id, -1);
    if (!(strcmp(actor.role, "admin") == 0 || strcmp(actor.role, "partner") == 0 ||
          (strcmp(actor.role, "student") == 0 && actor.student_id == student_id) ||
          (strcmp(actor.role, "lecturer") == 0 && owns_subject(&actor, subject_id)))) {
        reply_fail(response, 403, "Bạn không xem được kết quả này");
        return;
    }
    if (!is_final(status)) {
        reply_fail(response, 400, "Bài thi chưa được nộp");
        return;
    }
    reply_begin(&w);
    write_paper(&w, request->id, 1);
    reply_json(response, &w);
}

static void shuffle_ids(int* items, int count) {
    int i;
    for (i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = items[i];
        items[i] = items[j];
        items[j] = tmp;
    }
}

static int already_picked(const int* selected, int filled, int id) {
    int i;
    for (i = 0; i < filled; i++) if (selected[i] == id) return 1;
    return 0;
}

static int draw_questions(int subject_id, const char* difficulty, const char* chapter, int count, int* out, int filled) {
    sqlite3_stmt* stmt = db_prep(
        "SELECT id FROM questions WHERE subject_id=? AND difficulty=? AND is_active=1 "
        "AND (?='' OR chapter=?) "
        "AND EXISTS(SELECT 1 FROM answers a WHERE a.question_id=questions.id AND a.is_correct=1)");
    int ids[64];
    int n = 0;
    int i;
    int kept = 0;
    if (filled + count > 60) return 0;
    sqlite3_bind_int(stmt, 1, subject_id);
    db_bind_text(stmt, 2, difficulty);
    db_bind_text(stmt, 3, chapter ? chapter : "");
    db_bind_text(stmt, 4, chapter ? chapter : "");
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW && n < 64) {
        int id = db_int(stmt, 0);
        if (!already_picked(out, filled, id)) ids[n++] = id;
    }
    sqlite3_finalize(stmt);
    if (n < count) return 0;
    shuffle_ids(ids, n);
    for (i = 0; i < count; i++) out[filled + i] = ids[i];
    (void)kept;
    return 1;
}

static int draw_exam(int exam_id, int subject_id, int easy, int medium, int hard, int* selected, int* count) {
    sqlite3_stmt* stmt = db_prep("SELECT chapter, easy_count, medium_count, hard_count FROM exam_matrix WHERE exam_id=? ORDER BY id");
    int rows = 0;
    *count = 0;
    if (stmt) sqlite3_bind_int(stmt, 1, exam_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        char chapter[120];
        copy_str(chapter, sizeof(chapter), db_text(stmt, 0));
        rows++;
        if (db_int(stmt, 1) && !draw_questions(subject_id, "easy", chapter, db_int(stmt, 1), selected, *count)) {
            sqlite3_finalize(stmt);
            return 0;
        }
        *count += db_int(stmt, 1);
        if (db_int(stmt, 2) && !draw_questions(subject_id, "medium", chapter, db_int(stmt, 2), selected, *count)) {
            sqlite3_finalize(stmt);
            return 0;
        }
        *count += db_int(stmt, 2);
        if (db_int(stmt, 3) && !draw_questions(subject_id, "hard", chapter, db_int(stmt, 3), selected, *count)) {
            sqlite3_finalize(stmt);
            return 0;
        }
        *count += db_int(stmt, 3);
    }
    sqlite3_finalize(stmt);
    if (rows) return 1;
    if ((easy && !draw_questions(subject_id, "easy", "", easy, selected, 0)) ||
        (medium && !draw_questions(subject_id, "medium", "", medium, selected, easy)) ||
        (hard && !draw_questions(subject_id, "hard", "", hard, selected, easy + medium))) return 0;
    *count = easy + medium + hard;
    return 1;
}

void route_exam_start(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* exam;
    int open_id = 0;
    int finished = 0;
    int subject_id, minutes, easy, medium, hard, shuffle_q, shuffle_a, class_id, max_attempts;
    long long end_time;
    char title[200];
    char status[32];
    long long now;
    long long deadline;
    int selected[64];
    int count = 0;
    int order;
    int i;
    unsigned seed;
    char code[8];
    double paper_total = 0;
    int attempt_id;
    W w;
    if (!require_role(request, response, &actor, "student")) return;
    exam = db_prep("SELECT id FROM attempts WHERE exam_id=? AND student_id=? AND status NOT IN ('GRADED','AUTO_SUBMITTED','EXPIRED','SUBMITTED')");
    sqlite3_bind_int(exam, 1, request->id);
    sqlite3_bind_int(exam, 2, actor.student_id);
    if (exam && sqlite3_step(exam) == SQLITE_ROW) open_id = db_int(exam, 0);
    sqlite3_finalize(exam);
    if (open_id) {
        close_if_late(open_id);
        reply_begin(&w);
        write_paper(&w, open_id, 0);
        reply_json(response, &w);
        return;
    }
    exam = db_prep(
        "SELECT subject_id, duration_minutes, easy_count, medium_count, hard_count, shuffle_questions, shuffle_answers, class_id, "
        "max_attempts, end_time, title, status, start_time FROM exams WHERE id=?");
    sqlite3_bind_int(exam, 1, request->id);
    if (!exam || sqlite3_step(exam) != SQLITE_ROW) {
        sqlite3_finalize(exam);
        reply_fail(response, 404, "Không tìm thấy kỳ thi");
        return;
    }
    subject_id = db_int(exam, 0);
    minutes = db_int(exam, 1);
    easy = db_int(exam, 2);
    medium = db_int(exam, 3);
    hard = db_int(exam, 4);
    shuffle_q = db_int(exam, 5);
    shuffle_a = db_int(exam, 6);
    class_id = db_int(exam, 7);
    max_attempts = db_int(exam, 8);
    end_time = db_i64(exam, 9);
    copy_str(title, sizeof(title), db_text(exam, 10));
    copy_str(status, sizeof(status), db_text(exam, 11));
    now = now_sec();
    if (strcmp(status, "published") != 0) {
        sqlite3_finalize(exam);
        reply_fail(response, 400, "Kỳ thi chưa được mở");
        return;
    }
    if (now < db_i64(exam, 12)) {
        sqlite3_finalize(exam);
        reply_fail(response, 400, "Chưa đến giờ mở đề");
        return;
    }
    sqlite3_finalize(exam);
    if (now >= end_time) {
        reply_fail(response, 400, "Kỳ thi đã kết thúc");
        return;
    }
    if (class_id) {
        sqlite3_stmt* room = db_prep("SELECT name FROM classes WHERE id=?");
        char name[64] = "";
        sqlite3_bind_int(room, 1, class_id);
        if (room && sqlite3_step(room) == SQLITE_ROW) copy_str(name, sizeof(name), db_text(room, 0));
        sqlite3_finalize(room);
        if (strcmp(name, actor.class_name) != 0) {
            reply_fail(response, 403, "Bạn không thuộc đối tượng của kỳ thi này");
            return;
        }
    }
    finished = scalar_int("SELECT COUNT(*) FROM attempts WHERE exam_id=? AND student_id=? AND status IN ('GRADED','AUTO_SUBMITTED','EXPIRED','SUBMITTED')", request->id, actor.student_id);
    if (finished >= max_attempts) {
        reply_fail(response, 400, "Bạn đã hết số lần làm bài");
        return;
    }
    if (!draw_exam(request->id, subject_id, easy, medium, hard, selected, &count) || count < 1) {
        reply_fail(response, 400, "Ngân hàng không đủ câu cho ma trận đề");
        return;
    }
    if (shuffle_q) shuffle_ids(selected, count);
    deadline = now + (long long)minutes * 60;
    if (end_time < deadline) deadline = end_time;
    if (deadline - now < 30) {
        reply_fail(response, 400, "Kỳ thi sắp đóng, không đủ thời gian để bắt đầu");
        return;
    }
    seed = (unsigned)now ^ ((unsigned)rand() << 16) ^ (unsigned)rand() ^ (unsigned)actor.user_id;
    snprintf(code, sizeof(code), "%04X", seed & 0xFFFF);
    for (i = 0; i < count; i++) {
        sqlite3_stmt* score = db_prep("SELECT score FROM questions WHERE id=?");
        sqlite3_bind_int(score, 1, selected[i]);
        if (score && sqlite3_step(score) == SQLITE_ROW) paper_total += db_real(score, 0);
        sqlite3_finalize(score);
    }
    db_begin();
    {
        sqlite3_stmt* stmt = db_prep(
            "INSERT INTO attempts(exam_id, student_id, start_time, deadline, status, paper_code, paper_seed, paper_total, last_sync) "
            "VALUES(?,?,?,?,'IN_PROGRESS',?,?,?,?)");
        sqlite3_bind_int(stmt, 1, request->id);
        sqlite3_bind_int(stmt, 2, actor.student_id);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_bind_int64(stmt, 4, deadline);
        db_bind_text(stmt, 5, code);
        sqlite3_bind_int(stmt, 6, (int)seed);
        sqlite3_bind_double(stmt, 7, paper_total);
        sqlite3_bind_int64(stmt, 8, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        attempt_id = (int)sqlite3_last_insert_rowid(g_db);
    }
    for (order = 0; order < count; order++) {
        sqlite3_stmt* stmt = db_prep("INSERT INTO attempt_questions(attempt_id, question_id, order_index) VALUES(?,?,?)");
        int answers[8];
        int answer_count = 0;
        sqlite3_stmt* ans;
        sqlite3_bind_int(stmt, 1, attempt_id);
        sqlite3_bind_int(stmt, 2, selected[order]);
        sqlite3_bind_int(stmt, 3, order + 1);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        ans = db_prep("SELECT id FROM answers WHERE question_id=? ORDER BY display_order, id");
        sqlite3_bind_int(ans, 1, selected[order]);
        while (ans && sqlite3_step(ans) == SQLITE_ROW && answer_count < 8) answers[answer_count++] = db_int(ans, 0);
        sqlite3_finalize(ans);
        if (shuffle_a) shuffle_ids(answers, answer_count);
        for (i = 0; i < answer_count; i++) {
            stmt = db_prep("INSERT INTO attempt_choices(attempt_id, question_id, answer_id, position) VALUES(?,?,?,?)");
            sqlite3_bind_int(stmt, 1, attempt_id);
            sqlite3_bind_int(stmt, 2, selected[order]);
            sqlite3_bind_int(stmt, 3, answers[i]);
            sqlite3_bind_int(stmt, 4, i + 1);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    {
        sqlite3_stmt* stmt = db_prep("INSERT OR IGNORE INTO exam_participants(exam_id, student_id) VALUES(?,?)");
        sqlite3_bind_int(stmt, 1, request->id);
        sqlite3_bind_int(stmt, 2, actor.student_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    db_commit();
    audit_add(actor.user_id, "START_ATTEMPT", title, request->ip);
    reply_begin(&w);
    write_paper(&w, attempt_id, 0);
    reply_json(response, &w);
}

void route_attempt_event(Request* request, Response* response) {
    Actor actor;
    const char* raw = js_str(request->json, "kind", "");
    char kind[24];
    char status[32];
    sqlite3_stmt* stmt;
    int student_id;
    W w;
    if (!require_role(request, response, &actor, "student")) return;
    if (strcmp(raw, "TAB") != 0 && strcmp(raw, "BLUR") != 0 && strcmp(raw, "REFRESH") != 0 &&
        strcmp(raw, "RECONNECT") != 0 && strcmp(raw, "OFFLINE") != 0) {
        reply_fail(response, 400, "Sự kiện không hợp lệ");
        return;
    }
    copy_str(kind, sizeof(kind), raw);
    stmt = db_prep("SELECT student_id, status FROM attempts WHERE id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, request->id);
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không tìm thấy lượt thi");
        return;
    }
    student_id = db_int(stmt, 0);
    copy_str(status, sizeof(status), db_text(stmt, 1));
    sqlite3_finalize(stmt);
    if (student_id != actor.student_id) {
        reply_fail(response, 403, "Bạn không ghi được sự kiện này");
        return;
    }
    if (!is_final(status)) {
        stmt = db_prep("INSERT INTO exam_events(attempt_id, kind, created_at) VALUES(?,?,?)");
        sqlite3_bind_int(stmt, 1, request->id);
        db_bind_text(stmt, 2, kind);
        sqlite3_bind_int64(stmt, 3, now_sec());
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    reply_begin(&w);
    w_obj(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_student_exams(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    long long now = now_sec();
    W w;
    if (!require_role(request, response, &actor, "student")) return;
    attempt_sweep();
    stmt = db_prep(
        "SELECT e.id, e.title, e.description, s.name, s.code, e.start_time, e.end_time, e.duration_minutes, e.total_questions, e.total_score, "
        "e.max_attempts, e.shuffle_questions, e.shuffle_answers, e.auto_submit, e.status, e.class_id, IFNULL(c.name,'') "
        "FROM exams e JOIN subjects s ON s.id=e.subject_id LEFT JOIN classes c ON c.id=e.class_id ORDER BY e.start_time");
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        int exam_id = db_int(stmt, 0);
        int class_id = db_int(stmt, 15);
        int allowed = class_id == 0 || strcmp(db_text(stmt, 16), actor.class_name) == 0;
        int finished = 0;
        int open_id = 0;
        int needs = 0;
        int last_id = 0;
        long long last_submit = -1;
        char last_status[32] = "";
        double last_score = 0;
        double last_total = 0;
        int has = 0;
        int grace = scalar_int("SELECT sync_grace_sec FROM exams WHERE id=?", exam_id, -1);
        sqlite3_stmt* attempts;
        if (strcmp(db_text(stmt, 2), "__practice__") == 0) continue;
        attempts = db_prep("SELECT id, status, submit_time, score, paper_total, deadline FROM attempts WHERE exam_id=? AND student_id=?");
        sqlite3_bind_int(attempts, 1, exam_id);
        sqlite3_bind_int(attempts, 2, actor.student_id);
        while (attempts && sqlite3_step(attempts) == SQLITE_ROW) {
            has = 1;
            if (is_final(db_text(attempts, 1))) {
                finished++;
                if (db_i64(attempts, 2) >= last_submit) {
                    last_submit = db_i64(attempts, 2);
                    last_id = db_int(attempts, 0);
                    copy_str(last_status, sizeof(last_status), db_text(attempts, 1));
                    last_score = db_real(attempts, 3);
                    last_total = db_real(attempts, 4);
                }
            } else {
                open_id = db_int(attempts, 0);
                if (now >= db_i64(attempts, 5) && now <= db_i64(attempts, 5) + grace) needs = 1;
            }
        }
        sqlite3_finalize(attempts);
        if (!(has || (allowed && strcmp(db_text(stmt, 14), "published") == 0 && db_i64(stmt, 6) >= now))) continue;
        w_obj(&w);
        w_key(&w, "id");
        w_num(&w, exam_id);
        w_key(&w, "title");
        w_str(&w, db_text(stmt, 1));
        w_key(&w, "description");
        w_str(&w, db_text(stmt, 2));
        w_key(&w, "subjectName");
        w_str(&w, db_text(stmt, 3));
        w_key(&w, "subjectCode");
        w_str(&w, db_text(stmt, 4));
        w_key(&w, "startTime");
        w_num(&w, (double)db_i64(stmt, 5));
        w_key(&w, "endTime");
        w_num(&w, (double)db_i64(stmt, 6));
        w_key(&w, "durationMinutes");
        w_num(&w, db_int(stmt, 7));
        w_key(&w, "totalQuestions");
        w_num(&w, db_int(stmt, 8));
        w_key(&w, "totalScore");
        w_num(&w, db_real(stmt, 9));
        w_key(&w, "maxAttempts");
        w_num(&w, db_int(stmt, 10));
        w_key(&w, "attemptsUsed");
        w_num(&w, finished);
        w_key(&w, "openAttemptId");
        w_num(&w, open_id);
        w_key(&w, "canStart");
        w_bool(&w, allowed && strcmp(db_text(stmt, 14), "published") == 0 && now >= db_i64(stmt, 5) && now < db_i64(stmt, 6) && finished < db_int(stmt, 10) && open_id == 0);
        w_key(&w, "canResume");
        w_bool(&w, open_id != 0);
        w_key(&w, "needsFinalize");
        w_bool(&w, needs);
        w_key(&w, "lastAttemptId");
        w_num(&w, last_id);
        w_key(&w, "lastStatus");
        w_str(&w, last_status);
        w_key(&w, "lastScore");
        w_num(&w, last_score);
        w_key(&w, "lastTotal");
        w_num(&w, last_total);
        w_key(&w, "shuffleQuestions");
        w_bool(&w, db_int(stmt, 11));
        w_key(&w, "shuffleAnswers");
        w_bool(&w, db_int(stmt, 12));
        w_key(&w, "autoSubmit");
        w_bool(&w, db_int(stmt, 13));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

void route_practice_start(Request* request, Response* response) {
    Actor actor;
    const Js* ids;
    int selected[64];
    int count = 0;
    int i;
    int subject_id = 0;
    int minutes;
    int exam_id;
    int attempt_id;
    int order;
    long long now;
    long long deadline;
    unsigned seed;
    char code[8];
    double paper_total = 0;
    W w;
    if (!require_role(request, response, &actor, "student")) return;
    if (actor.must_change) {
        reply_fail(response, 403, "Bạn cần đổi mật khẩu trước khi làm bài");
        return;
    }
    ids = js_get(request->json, "questionIds");
    if (!ids || ids->type != JS_ARR || js_len(ids) < 1) {
        reply_fail(response, 400, "Chưa chọn câu hỏi để làm bài");
        return;
    }
    if (js_len(ids) > 60) {
        reply_fail(response, 400, "Tối đa 60 câu mỗi lần luyện tập");
        return;
    }
    for (i = 0; i < js_len(ids); i++) {
        const Js* item = js_at(ids, i);
        int qid = 0;
        int q_subject = 0;
        int active = 0;
        sqlite3_stmt* stmt;
        if (!item) continue;
        if (item->type == JS_NUM) qid = (int)item->n;
        else if (item->type == JS_STR) qid = atoi(item->s ? item->s : "0");
        if (qid <= 0 || already_picked(selected, count, qid)) continue;
        stmt = db_prep(
            "SELECT subject_id, is_active FROM questions WHERE id=? AND "
            "EXISTS(SELECT 1 FROM answers a WHERE a.question_id=questions.id AND a.is_correct=1)");
        sqlite3_bind_int(stmt, 1, qid);
        if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
            q_subject = db_int(stmt, 0);
            active = db_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
        if (!q_subject || !active) {
            reply_fail(response, 400, "Có câu hỏi không dùng được trong link");
            return;
        }
        if (!subject_id) subject_id = q_subject;
        selected[count++] = qid;
    }
    if (count < 1 || !subject_id) {
        reply_fail(response, 400, "Không có câu hỏi hợp lệ trong link");
        return;
    }
    minutes = (int)js_num(request->json, "minutes", 0);
    if (minutes < 5) minutes = count * 2;
    if (minutes < 10) minutes = 10;
    if (minutes > 180) minutes = 180;
    now = now_sec();
    deadline = now + (long long)minutes * 60;
    seed = (unsigned)now ^ ((unsigned)rand() << 16) ^ (unsigned)actor.user_id ^ (unsigned)count;
    snprintf(code, sizeof(code), "%04X", seed & 0xFFFF);
    for (i = 0; i < count; i++) {
        sqlite3_stmt* score = db_prep("SELECT score FROM questions WHERE id=?");
        sqlite3_bind_int(score, 1, selected[i]);
        if (score && sqlite3_step(score) == SQLITE_ROW) paper_total += db_real(score, 0);
        sqlite3_finalize(score);
    }
    db_begin();
    {
        sqlite3_stmt* stmt = db_prep(
            "INSERT INTO exams(subject_id, title, description, start_time, end_time, duration_minutes, "
            "easy_count, medium_count, hard_count, total_questions, total_score, shuffle_questions, shuffle_answers, "
            "auto_submit, max_attempts, status, class_id, short_disconnect_sec, long_disconnect_sec, sync_grace_sec, created_by, created_at) "
            "VALUES(?,?,?,?,?,?,0,0,0,?,?,0,1,1,50,'published',0,30,300,300,?,?)");
        char title[120];
        int mixed = 0;
        for (i = 0; i < count; i++) {
            int sid = scalar_int("SELECT subject_id FROM questions WHERE id=?", selected[i], 0);
            if (sid && sid != subject_id) { mixed = 1; break; }
        }
        snprintf(title, sizeof(title), mixed ? "Luyện tập %d câu (nhiều môn)" : "Luyện tập %d câu", count);
        sqlite3_bind_int(stmt, 1, subject_id);
        db_bind_text(stmt, 2, title);
        db_bind_text(stmt, 3, "__practice__");
        sqlite3_bind_int64(stmt, 4, now - 60);
        sqlite3_bind_int64(stmt, 5, now + 7 * 24 * 3600);
        sqlite3_bind_int(stmt, 6, minutes);
        sqlite3_bind_int(stmt, 7, count);
        sqlite3_bind_double(stmt, 8, paper_total);
        sqlite3_bind_int(stmt, 9, actor.user_id);
        sqlite3_bind_int64(stmt, 10, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        exam_id = (int)sqlite3_last_insert_rowid(g_db);
    }
    {
        sqlite3_stmt* stmt = db_prep(
            "INSERT INTO attempts(exam_id, student_id, start_time, deadline, status, paper_code, paper_seed, paper_total, last_sync) "
            "VALUES(?,?,?,?,'IN_PROGRESS',?,?,?,?)");
        sqlite3_bind_int(stmt, 1, exam_id);
        sqlite3_bind_int(stmt, 2, actor.student_id);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_bind_int64(stmt, 4, deadline);
        db_bind_text(stmt, 5, code);
        sqlite3_bind_int(stmt, 6, (int)seed);
        sqlite3_bind_double(stmt, 7, paper_total);
        sqlite3_bind_int64(stmt, 8, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        attempt_id = (int)sqlite3_last_insert_rowid(g_db);
    }
    for (order = 0; order < count; order++) {
        sqlite3_stmt* stmt = db_prep("INSERT INTO attempt_questions(attempt_id, question_id, order_index) VALUES(?,?,?)");
        int answers[8];
        int answer_count = 0;
        sqlite3_stmt* ans;
        sqlite3_bind_int(stmt, 1, attempt_id);
        sqlite3_bind_int(stmt, 2, selected[order]);
        sqlite3_bind_int(stmt, 3, order + 1);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        ans = db_prep("SELECT id FROM answers WHERE question_id=? ORDER BY display_order, id");
        sqlite3_bind_int(ans, 1, selected[order]);
        while (ans && sqlite3_step(ans) == SQLITE_ROW && answer_count < 8) answers[answer_count++] = db_int(ans, 0);
        sqlite3_finalize(ans);
        shuffle_ids(answers, answer_count);
        for (i = 0; i < answer_count; i++) {
            stmt = db_prep("INSERT INTO attempt_choices(attempt_id, question_id, answer_id, position) VALUES(?,?,?,?)");
            sqlite3_bind_int(stmt, 1, attempt_id);
            sqlite3_bind_int(stmt, 2, selected[order]);
            sqlite3_bind_int(stmt, 3, answers[i]);
            sqlite3_bind_int(stmt, 4, i + 1);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    {
        sqlite3_stmt* stmt = db_prep("INSERT OR IGNORE INTO exam_participants(exam_id, student_id) VALUES(?,?)");
        sqlite3_bind_int(stmt, 1, exam_id);
        sqlite3_bind_int(stmt, 2, actor.student_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    db_commit();
    audit_add(actor.user_id, "START_PRACTICE", "Luyện tập từ ngân hàng câu hỏi", request->ip);
    reply_begin(&w);
    write_paper(&w, attempt_id, 0);
    reply_json(response, &w);
}
