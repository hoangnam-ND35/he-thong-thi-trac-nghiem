#include "app.h"

#include <string.h>

void route_dash_admin(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    int graded = 0;
    double sum = 0;
    double highest = 0;
    double lowest = 0;
    int any = 0;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    stmt = db_prep("SELECT percent FROM results");
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        double percent = db_real(stmt, 0);
        if (!any || percent > highest) highest = percent;
        if (!any || percent < lowest) lowest = percent;
        sum += percent;
        graded++;
        any = 1;
    }
    sqlite3_finalize(stmt);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "users");
    w_num(&w, db_count("SELECT COUNT(*) FROM users"));
    w_key(&w, "students");
    w_num(&w, db_count("SELECT COUNT(*) FROM students"));
    w_key(&w, "lecturers");
    w_num(&w, db_count("SELECT COUNT(*) FROM lecturers"));
    w_key(&w, "exams");
    w_num(&w, db_count("SELECT COUNT(*) FROM exams"));
    w_key(&w, "subjects");
    w_num(&w, db_count("SELECT COUNT(*) FROM subjects"));
    w_key(&w, "questions");
    w_num(&w, db_count("SELECT COUNT(*) FROM questions"));
    w_key(&w, "attempts");
    w_num(&w, db_count("SELECT COUNT(*) FROM attempts"));
    w_key(&w, "graded");
    w_num(&w, graded);
    w_key(&w, "averagePercent");
    w_num(&w, graded ? sum / graded : 0);
    w_key(&w, "highestPercent");
    w_num(&w, any ? highest : 0);
    w_key(&w, "lowestPercent");
    w_num(&w, any ? lowest : 0);
    w_key(&w, "recent");
    w_arr(&w);
    stmt = db_prep(
        "SELECT IFNULL(p.full_name,''), a.action, a.created_at FROM audit_logs a "
        "LEFT JOIN users u ON u.id=a.user_id LEFT JOIN profiles p ON p.id=u.profile_id ORDER BY a.id DESC LIMIT 8");
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "fullName");
        w_str(&w, db_text(stmt, 0));
        w_key(&w, "action");
        w_str(&w, db_text(stmt, 1));
        w_key(&w, "createdAt");
        w_num(&w, (double)db_i64(stmt, 2));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_dash_lecturer(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* subjects;
    long long now = now_sec();
    int submitted = 0;
    int graded = 0;
    double sum = 0;
    W ongoing;
    W upcoming;
    W subject_json;
    W w;
    if (!require_role(request, response, &actor, "lecturer,admin,partner")) return;
    w_init(&ongoing);
    w_arr(&ongoing);
    w_init(&upcoming);
    w_arr(&upcoming);
    w_init(&subject_json);
    w_arr(&subject_json);
    subjects = db_prep("SELECT id, code, name FROM subjects WHERE (?=0 OR lecturer_id=?) ORDER BY code");
    sqlite3_bind_int(subjects, 1, strcmp(actor.role, "lecturer") == 0 ? actor.lecturer_id : 0);
    sqlite3_bind_int(subjects, 2, actor.lecturer_id);
    while (subjects && sqlite3_step(subjects) == SQLITE_ROW) {
        int subject_id = db_int(subjects, 0);
        int questions = scalar_int("SELECT COUNT(*) FROM questions WHERE subject_id=? AND is_active=1", subject_id, -1);
        sqlite3_stmt* exams = db_prep("SELECT id, title, start_time, end_time FROM exams WHERE subject_id=? AND status='published'");
        w_obj(&subject_json);
        w_key(&subject_json, "id");
        w_num(&subject_json, subject_id);
        w_key(&subject_json, "code");
        w_str(&subject_json, db_text(subjects, 1));
        w_key(&subject_json, "name");
        w_str(&subject_json, db_text(subjects, 2));
        w_key(&subject_json, "questions");
        w_num(&subject_json, questions);
        w_end(&subject_json);
        sqlite3_bind_int(exams, 1, subject_id);
        while (exams && sqlite3_step(exams) == SQLITE_ROW) {
            int exam_id = db_int(exams, 0);
            long long start = db_i64(exams, 2);
            long long end = db_i64(exams, 3);
            int took = 0;
            sqlite3_stmt* attempts = db_prep("SELECT score, paper_total FROM attempts WHERE exam_id=? AND status IN ('GRADED','AUTO_SUBMITTED','EXPIRED')");
            W* target = NULL;
            sqlite3_bind_int(attempts, 1, exam_id);
            while (attempts && sqlite3_step(attempts) == SQLITE_ROW) {
                double paper = db_real(attempts, 1);
                took++;
                submitted++;
                graded++;
                sum += paper > 0 ? 100.0 * db_real(attempts, 0) / paper : 0;
            }
            sqlite3_finalize(attempts);
            if (now >= start && now < end) target = &ongoing;
            else if (now < start) target = &upcoming;
            if (target) {
                w_obj(target);
                w_key(target, "id");
                w_num(target, exam_id);
                w_key(target, "title");
                w_str(target, db_text(exams, 1));
                w_key(target, "subjectCode");
                w_str(target, db_text(subjects, 1));
                w_key(target, "startTime");
                w_num(target, (double)start);
                w_key(target, "endTime");
                w_num(target, (double)end);
                w_key(target, "submitted");
                w_num(target, took);
                w_end(target);
            }
        }
        sqlite3_finalize(exams);
    }
    sqlite3_finalize(subjects);
    w_end(&ongoing);
    w_end(&upcoming);
    w_end(&subject_json);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "ongoing");
    w_raw(&w, ongoing.s);
    w.need[w.depth - 1] = 1;
    w_key(&w, "upcoming");
    w_raw(&w, upcoming.s);
    w.need[w.depth - 1] = 1;
    w_key(&w, "subjects");
    w_raw(&w, subject_json.s);
    w.need[w.depth - 1] = 1;
    w_key(&w, "submittedCount");
    w_num(&w, submitted);
    w_key(&w, "averagePercent");
    w_num(&w, graded ? sum / graded : 0);
    w_key(&w, "inProgress");
    w_num(&w, scalar_int(
        "SELECT COUNT(*) FROM attempts a JOIN exams e ON e.id=a.exam_id JOIN subjects s ON s.id=e.subject_id "
        "WHERE a.status='IN_PROGRESS' AND (?=0 OR s.lecturer_id=?)",
        strcmp(actor.role, "lecturer") == 0 ? actor.lecturer_id : 0, actor.lecturer_id));
    w_end(&w);
    reply_json(response, &w);
    w_free(&ongoing);
    w_free(&upcoming);
    w_free(&subject_json);
}

void route_dash_student(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "student")) return;
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "notifications");
    w_arr(&w);
    stmt = db_prep("SELECT message, created_at FROM notifications WHERE user_id=? ORDER BY id DESC LIMIT 6");
    sqlite3_bind_int(stmt, 1, actor.user_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "message");
        w_str(&w, db_text(stmt, 0));
        w_key(&w, "createdAt");
        w_num(&w, (double)db_i64(stmt, 1));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_audit(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    stmt = db_prep(
        "SELECT a.created_at, IFNULL(p.full_name,''), IFNULL(u.username,''), a.action, a.detail, a.ip "
        "FROM audit_logs a LEFT JOIN users u ON u.id=a.user_id LEFT JOIN profiles p ON p.id=u.profile_id "
        "ORDER BY a.id DESC LIMIT 200");
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "createdAt");
        w_num(&w, (double)db_i64(stmt, 0));
        w_key(&w, "fullName");
        w_str(&w, db_text(stmt, 1));
        w_key(&w, "username");
        w_str(&w, db_text(stmt, 2));
        w_key(&w, "action");
        w_str(&w, db_text(stmt, 3));
        w_key(&w, "detail");
        w_str(&w, db_text(stmt, 4));
        w_key(&w, "ip");
        w_str(&w, db_text(stmt, 5));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}
