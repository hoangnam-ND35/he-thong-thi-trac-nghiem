#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_answers(const Js* body, char texts[4][500], int* correct, char* err, size_t err_cap) {
    const Js* answers = js_get(body, "answers");
    int i;
    int found = 0;
    *correct = 0;
    if (!answers || answers->type != JS_ARR || js_len(answers) != 4) {
        snprintf(err, err_cap, "Cần đúng 4 đáp án");
        return 0;
    }
    for (i = 0; i < 4; i++) {
        const Js* item = js_at(answers, i);
        trim_copy(texts[i], 500, js_str(item, "text", ""));
        if (!texts[i][0]) {
            snprintf(err, err_cap, "Đáp án không được trống");
            return 0;
        }
        if (js_bool(item, "correct", 0)) {
            *correct = i;
            found++;
        }
    }
    if (found != 1) {
        snprintf(err, err_cap, "Cần đúng một đáp án đúng");
        return 0;
    }
    return 1;
}

static void write_question(W* w, int question_id) {
    sqlite3_stmt* stmt = db_prep(
        "SELECT q.id, q.subject_id, q.chapter, q.text, q.difficulty, q.score, q.is_active, s.code, "
        "IFNULL(q.topic,''), IFNULL(q.explanation,'') "
        "FROM questions q JOIN subjects s ON s.id=q.subject_id WHERE q.id=?");
    if (stmt) sqlite3_bind_int(stmt, 1, question_id);
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        w_obj(w);
        w_end(w);
        return;
    }
    w_obj(w);
    w_key(w, "id");
    w_num(w, db_int(stmt, 0));
    w_key(w, "subjectId");
    w_num(w, db_int(stmt, 1));
    w_key(w, "chapter");
    w_str(w, db_text(stmt, 2));
    w_key(w, "topic");
    w_str(w, db_text(stmt, 8));
    w_key(w, "explanation");
    w_str(w, db_text(stmt, 9));
    w_key(w, "text");
    w_str(w, db_text(stmt, 3));
    w_key(w, "difficulty");
    w_str(w, db_text(stmt, 4));
    w_key(w, "score");
    w_num(w, db_real(stmt, 5));
    w_key(w, "isActive");
    w_bool(w, db_int(stmt, 6));
    w_key(w, "subjectCode");
    w_str(w, db_text(stmt, 7));
    sqlite3_finalize(stmt);
    w_key(w, "answers");
    w_arr(w);
    stmt = db_prep("SELECT id, text, is_correct FROM answers WHERE question_id=? ORDER BY display_order, id");
    if (stmt) sqlite3_bind_int(stmt, 1, question_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(w);
        w_key(w, "id");
        w_num(w, db_int(stmt, 0));
        w_key(w, "text");
        w_str(w, db_text(stmt, 1));
        w_key(w, "correct");
        w_bool(w, db_int(stmt, 2));
        w_end(w);
    }
    sqlite3_finalize(stmt);
    w_end(w);
    w_end(w);
}

static int difficulty_ok(const char* value) {
    return strcmp(value, "easy") == 0 || strcmp(value, "medium") == 0 || strcmp(value, "hard") == 0;
}

void route_questions_list(Request* request, Response* response) {
    Actor actor;
    int subject_id = query_int(request->query, "subjectId");
    char difficulty[16];
    char q[80];
    char like[96];
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    query_get(request->query, "difficulty", difficulty, sizeof(difficulty));
    query_get(request->query, "q", q, sizeof(q));
    snprintf(like, sizeof(like), "%%%s%%", q);
    stmt = db_prep(
        "SELECT q.id FROM questions q JOIN subjects s ON s.id=q.subject_id "
        "WHERE (?=0 OR s.lecturer_id=?) AND (?=0 OR q.subject_id=?) AND (?='' OR q.difficulty=?) AND (?='' OR q.text LIKE ?) "
        "ORDER BY q.id DESC");
    sqlite3_bind_int(stmt, 1, strcmp(actor.role, "lecturer") == 0 ? actor.lecturer_id : 0);
    sqlite3_bind_int(stmt, 2, actor.lecturer_id);
    sqlite3_bind_int(stmt, 3, subject_id);
    sqlite3_bind_int(stmt, 4, subject_id);
    db_bind_text(stmt, 5, difficulty);
    db_bind_text(stmt, 6, difficulty);
    db_bind_text(stmt, 7, q);
    db_bind_text(stmt, 8, like);
    reply_begin(&w);
    w_arr(&w);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        int id = db_int(stmt, 0);
        sqlite3_stmt* held = stmt;
        stmt = NULL;
        write_question(&w, id);
        stmt = held;
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    reply_json(response, &w);
}

static int insert_answers(int question_id, char texts[4][500], int correct) {
    int i;
    sqlite3_stmt* stmt = db_prep("DELETE FROM answers WHERE question_id=?");
    if (stmt) {
        sqlite3_bind_int(stmt, 1, question_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    for (i = 0; i < 4; i++) {
        stmt = db_prep("INSERT INTO answers(question_id, text, is_correct, display_order) VALUES(?,?,?,?)");
        sqlite3_bind_int(stmt, 1, question_id);
        db_bind_text(stmt, 2, texts[i]);
        sqlite3_bind_int(stmt, 3, i == correct);
        sqlite3_bind_int(stmt, 4, i + 1);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    return 1;
}

void route_question_create(Request* request, Response* response) {
    Actor actor;
    char texts[4][500];
    char chapter[120];
    char topic[160];
    char explanation[800];
    char difficulty[16];
    char text[2000];
    char err[160];
    int correct = 0;
    int subject_id;
    double score;
    sqlite3_stmt* stmt;
    int question_id;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    subject_id = (int)js_num(request->json, "subjectId", 0);
    if (!owns_subject(&actor, subject_id)) {
        reply_fail(response, 403, "Bạn không thêm câu cho môn này");
        return;
    }
    if (!read_answers(request->json, texts, &correct, err, sizeof(err))) {
        reply_fail(response, 400, err);
        return;
    }
    trim_copy(chapter, sizeof(chapter), js_str(request->json, "chapter", ""));
    trim_copy(topic, sizeof(topic), js_str(request->json, "topic", ""));
    trim_copy(explanation, sizeof(explanation), js_str(request->json, "explanation", ""));
    trim_copy(difficulty, sizeof(difficulty), js_str(request->json, "difficulty", ""));
    trim_copy(text, sizeof(text), js_str(request->json, "text", ""));
    score = js_num(request->json, "score", 1);
    if (!chapter[0] || !text[0] || !difficulty_ok(difficulty) || score <= 0) {
        reply_fail(response, 400, "Nội dung câu hỏi chưa hợp lệ");
        return;
    }
    stmt = db_prep(
        "INSERT INTO questions(subject_id, chapter, topic, text, difficulty, score, explanation, is_active, created_by, created_at, updated_at) "
        "VALUES(?,?,?,?,?,?,?,1,?,?,?)");
    sqlite3_bind_int(stmt, 1, subject_id);
    db_bind_text(stmt, 2, chapter);
    db_bind_text(stmt, 3, topic);
    db_bind_text(stmt, 4, text);
    db_bind_text(stmt, 5, difficulty);
    sqlite3_bind_double(stmt, 6, score);
    db_bind_text(stmt, 7, explanation);
    sqlite3_bind_int(stmt, 8, actor.user_id);
    sqlite3_bind_int64(stmt, 9, now_sec());
    sqlite3_bind_int64(stmt, 10, now_sec());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    question_id = (int)sqlite3_last_insert_rowid(g_db);
    insert_answers(question_id, texts, correct);
    audit_add(actor.user_id, "CREATE_QUESTION", "Thêm câu hỏi", request->ip);
    reply_begin(&w);
    write_question(&w, question_id);
    reply_json(response, &w);
}

void route_question_update(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    int subject_id = 0;
    int used;
    char text[2000];
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    stmt = db_prep("SELECT subject_id FROM questions WHERE id=?");
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không tìm thấy câu hỏi");
        return;
    }
    subject_id = db_int(stmt, 0);
    sqlite3_finalize(stmt);
    if (!owns_subject(&actor, subject_id)) {
        reply_fail(response, 403, "Bạn không sửa được câu này");
        return;
    }
    trim_copy(text, sizeof(text), js_str(request->json, "text", ""));
    if (!text[0]) {
        reply_fail(response, 400, "Thiếu nội dung câu hỏi");
        return;
    }
    used = scalar_int("SELECT COUNT(*) FROM attempt_questions WHERE question_id=?", request->id, -1);
    if (used) {
        char topic[160];
        char explanation[800];
        trim_copy(topic, sizeof(topic), js_str(request->json, "topic", ""));
        trim_copy(explanation, sizeof(explanation), js_str(request->json, "explanation", ""));
        stmt = db_prep("UPDATE questions SET text=?, topic=?, explanation=?, updated_at=? WHERE id=?");
        db_bind_text(stmt, 1, text);
        db_bind_text(stmt, 2, topic);
        db_bind_text(stmt, 3, explanation);
        sqlite3_bind_int64(stmt, 4, now_sec());
        sqlite3_bind_int(stmt, 5, request->id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    } else {
        char texts[4][500];
        char chapter[120];
        char topic[160];
        char explanation[800];
        char difficulty[16];
        char err[160];
        int correct = 0;
        double score = js_num(request->json, "score", 1);
        int new_subject = (int)js_num(request->json, "subjectId", subject_id);
        if (!read_answers(request->json, texts, &correct, err, sizeof(err))) {
            reply_fail(response, 400, err);
            return;
        }
        trim_copy(chapter, sizeof(chapter), js_str(request->json, "chapter", ""));
        trim_copy(topic, sizeof(topic), js_str(request->json, "topic", ""));
        trim_copy(explanation, sizeof(explanation), js_str(request->json, "explanation", ""));
        trim_copy(difficulty, sizeof(difficulty), js_str(request->json, "difficulty", ""));
        if (!owns_subject(&actor, new_subject) || !difficulty_ok(difficulty) || score <= 0) {
            reply_fail(response, 400, "Nội dung câu hỏi chưa hợp lệ");
            return;
        }
        stmt = db_prep("UPDATE questions SET subject_id=?, chapter=?, topic=?, text=?, difficulty=?, score=?, explanation=?, is_active=?, updated_at=? WHERE id=?");
        sqlite3_bind_int(stmt, 1, new_subject);
        db_bind_text(stmt, 2, chapter);
        db_bind_text(stmt, 3, topic);
        db_bind_text(stmt, 4, text);
        db_bind_text(stmt, 5, difficulty);
        sqlite3_bind_double(stmt, 6, score);
        db_bind_text(stmt, 7, explanation);
        sqlite3_bind_int(stmt, 8, js_bool(request->json, "isActive", 1));
        sqlite3_bind_int64(stmt, 9, now_sec());
        sqlite3_bind_int(stmt, 10, request->id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        insert_answers(request->id, texts, correct);
    }
    audit_add(actor.user_id, "UPDATE_QUESTION", "Sửa câu hỏi", request->ip);
    reply_begin(&w);
    write_question(&w, request->id);
    reply_json(response, &w);
}

void route_question_disable(Request* request, Response* response) {
    Actor actor;
    sqlite3_stmt* stmt;
    int subject_id = 0;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    stmt = db_prep("SELECT subject_id FROM questions WHERE id=?");
    if (!stmt || sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        reply_fail(response, 404, "Không tìm thấy câu hỏi");
        return;
    }
    subject_id = db_int(stmt, 0);
    sqlite3_finalize(stmt);
    if (!owns_subject(&actor, subject_id)) {
        reply_fail(response, 403, "Bạn không đổi được câu này");
        return;
    }
    stmt = db_prep("UPDATE questions SET is_active=CASE WHEN is_active=1 THEN 0 ELSE 1 END WHERE id=?");
    sqlite3_bind_int(stmt, 1, request->id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    audit_add(actor.user_id, "TOGGLE_QUESTION", "Đổi trạng thái câu hỏi", request->ip);
    reply_begin(&w);
    write_question(&w, request->id);
    reply_json(response, &w);
}

void route_question_bank(Request* request, Response* response) {
    Actor actor;
    int subject_id = query_int(request->query, "subjectId");
    sqlite3_stmt* stmt;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    if (!owns_subject(&actor, subject_id)) {
        reply_fail(response, 403, "Bạn không xem được ngân hàng môn này");
        return;
    }
    stmt = db_prep(
        "SELECT "
        "SUM(CASE WHEN difficulty='easy' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN difficulty='medium' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN difficulty='hard' THEN 1 ELSE 0 END) "
        "FROM questions WHERE subject_id=? AND is_active=1 "
        "AND EXISTS(SELECT 1 FROM answers a WHERE a.question_id=questions.id AND a.is_correct=1)");
    sqlite3_bind_int(stmt, 1, subject_id);
    reply_begin(&w);
    w_obj(&w);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_key(&w, "easy");
        w_num(&w, db_int(stmt, 0));
        w_key(&w, "medium");
        w_num(&w, db_int(stmt, 1));
        w_key(&w, "hard");
        w_num(&w, db_int(stmt, 2));
    }
    sqlite3_finalize(stmt);
    w_key(&w, "chapters");
    w_arr(&w);
    stmt = db_prep(
        "SELECT chapter, "
        "SUM(CASE WHEN difficulty='easy' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN difficulty='medium' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN difficulty='hard' THEN 1 ELSE 0 END) "
        "FROM questions WHERE subject_id=? AND is_active=1 AND chapter<>'' "
        "AND EXISTS(SELECT 1 FROM answers a WHERE a.question_id=questions.id AND a.is_correct=1) "
        "GROUP BY chapter ORDER BY chapter");
    sqlite3_bind_int(stmt, 1, subject_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        w_obj(&w);
        w_key(&w, "chapter");
        w_str(&w, db_text(stmt, 0));
        w_key(&w, "easy");
        w_num(&w, db_int(stmt, 1));
        w_key(&w, "medium");
        w_num(&w, db_int(stmt, 2));
        w_key(&w, "hard");
        w_num(&w, db_int(stmt, 3));
        w_end(&w);
    }
    sqlite3_finalize(stmt);
    w_end(&w);
    w_end(&w);
    reply_json(response, &w);
}

void route_question_template(Request* request, Response* response) {
    Actor actor;
    const char* csv = "\xEF\xBB\xBF"
                      "subjectCode,chapter,difficulty,score,question,answerA,answerB,answerC,answerD,correct\n"
                      "JAVA101,Chương 1,easy,1,\"Java chạy trên môi trường nào?\",JVM,Browser,BIOS,DNS,A\n";
    char* body;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    body = (char*)malloc(strlen(csv) + 1);
    if (!body) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    memcpy(body, csv, strlen(csv) + 1);
    reply_raw(response, 200, "text/csv; charset=utf-8", body, (int)strlen(body), "mau-cau-hoi.csv");
}

static void csv_field(char** out, size_t* n, size_t* cap, const char* text) {
    int quote = text && (strchr(text, ',') || strchr(text, '"') || strchr(text, '\n'));
    size_t i;
    if (*n && (*out)[*n - 1] != '\n') {
        *out = (char*)realloc(*out, *cap + 2);
        (*out)[(*n)++] = ',';
    }
    if (!quote) {
        size_t len = text ? strlen(text) : 0;
        if (*n + len + 1 >= *cap) {
            *cap = *n + len + 64;
            *out = (char*)realloc(*out, *cap);
        }
        if (len) memcpy(*out + *n, text, len);
        *n += len;
        (*out)[*n] = 0;
        return;
    }
    if (*n + 2 >= *cap) {
        *cap = *cap * 2 + 32;
        *out = (char*)realloc(*out, *cap);
    }
    (*out)[(*n)++] = '"';
    for (i = 0; text[i]; i++) {
        if (*n + 3 >= *cap) {
            *cap *= 2;
            *out = (char*)realloc(*out, *cap);
        }
        if (text[i] == '"') (*out)[(*n)++] = '"';
        (*out)[(*n)++] = text[i];
    }
    (*out)[(*n)++] = '"';
    (*out)[*n] = 0;
}

void route_question_export(Request* request, Response* response) {
    Actor actor;
    int subject_id = query_int(request->query, "subjectId");
    sqlite3_stmt* stmt;
    char* csv = (char*)malloc(256);
    size_t n = 0;
    size_t cap = 256;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    memcpy(csv, "\xEF\xBB\xBF", 3);
    n = 3;
    csv[n] = 0;
    {
        const char* header = "subjectCode,chapter,difficulty,score,question,answerA,answerB,answerC,answerD,correct\n";
        size_t len = strlen(header);
        csv = (char*)realloc(csv, n + len + 1);
        memcpy(csv + n, header, len + 1);
        n += len;
        cap = n + 1;
    }
    stmt = db_prep(
        "SELECT q.id, s.code, q.chapter, q.difficulty, q.score, q.text FROM questions q "
        "JOIN subjects s ON s.id=q.subject_id WHERE (?=0 OR s.lecturer_id=?) AND (?=0 OR q.subject_id=?) ORDER BY q.id");
    sqlite3_bind_int(stmt, 1, strcmp(actor.role, "lecturer") == 0 ? actor.lecturer_id : 0);
    sqlite3_bind_int(stmt, 2, actor.lecturer_id);
    sqlite3_bind_int(stmt, 3, subject_id);
    sqlite3_bind_int(stmt, 4, subject_id);
    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        int qid = db_int(stmt, 0);
        char score[32];
        char answers[4][500];
        int correct = 0;
        int i = 0;
        sqlite3_stmt* ans = db_prep("SELECT text, is_correct FROM answers WHERE question_id=? ORDER BY display_order, id");
        snprintf(score, sizeof(score), "%.10g", db_real(stmt, 4));
        memset(answers, 0, sizeof(answers));
        if (ans) sqlite3_bind_int(ans, 1, qid);
        while (ans && sqlite3_step(ans) == SQLITE_ROW && i < 4) {
            copy_str(answers[i], sizeof(answers[i]), db_text(ans, 0));
            if (db_int(ans, 1)) correct = i;
            i++;
        }
        sqlite3_finalize(ans);
        csv_field(&csv, &n, &cap, db_text(stmt, 1));
        csv_field(&csv, &n, &cap, db_text(stmt, 2));
        csv_field(&csv, &n, &cap, db_text(stmt, 3));
        csv_field(&csv, &n, &cap, score);
        csv_field(&csv, &n, &cap, db_text(stmt, 5));
        for (i = 0; i < 4; i++) csv_field(&csv, &n, &cap, answers[i]);
        {
            char mark[2] = {(char)('A' + correct), 0};
            csv_field(&csv, &n, &cap, mark);
        }
        if (n + 2 >= cap) {
            cap *= 2;
            csv = (char*)realloc(csv, cap);
        }
        csv[n++] = '\n';
        csv[n] = 0;
    }
    sqlite3_finalize(stmt);
    reply_raw(response, 200, "text/csv; charset=utf-8", csv, (int)n, "ngan-hang-cau-hoi.csv");
}

static int csv_split(char* line, char* fields[], int max_fields) {
    int count = 0;
    char* p = line;
    if (p[0] == '\xEF' && (unsigned char)p[1] == 0xBB) p += 3;
    while (*p && count < max_fields) {
        if (*p == '"') {
            char* start = ++p;
            char* write = start;
            while (*p) {
                if (*p == '"') {
                    if (p[1] == '"') {
                        *write++ = '"';
                        p += 2;
                        continue;
                    }
                    break;
                }
                *write++ = *p++;
            }
            *write = 0;
            fields[count++] = start;
            if (*p == '"') p++;
            if (*p == ',') p++;
        } else {
            fields[count++] = p;
            while (*p && *p != ',') p++;
            if (*p == ',') {
                *p = 0;
                p++;
            }
        }
    }
    return count;
}

void route_question_import(Request* request, Response* response) {
    Actor actor;
    char* csv;
    char* line;
    int imported = 0;
    W w;
    if (!require_role(request, response, &actor, "admin,lecturer")) return;
    csv = (char*)malloc(strlen(js_str(request->json, "csv", "")) + 1);
    if (!csv) {
        reply_fail(response, 500, "Lỗi dữ liệu");
        return;
    }
    strcpy(csv, js_str(request->json, "csv", ""));
    line = strtok(csv, "\n");
    if (line) line = strtok(NULL, "\n");
    while (line) {
        char* fields[10];
        int count;
        char* cr = strchr(line, '\r');
        int subject_id;
        int correct;
        char texts[4][500];
        sqlite3_stmt* stmt;
        int question_id;
        int i;
        if (cr) *cr = 0;
        count = csv_split(line, fields, 10);
        if (count >= 10) {
            subject_id = 0;
            stmt = db_prep("SELECT id, lecturer_id FROM subjects WHERE code=?");
            db_bind_text(stmt, 1, fields[0]);
            if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
                subject_id = db_int(stmt, 0);
                if (strcmp(actor.role, "lecturer") == 0 && db_int(stmt, 1) != actor.lecturer_id) subject_id = 0;
            }
            sqlite3_finalize(stmt);
            correct = fields[9][0] ? fields[9][0] - 'A' : -1;
            if (correct < 0 || correct > 3) {
                if (strcmp(fields[9], "B") == 0) correct = 1;
            }
            if (subject_id && difficulty_ok(fields[2]) && correct >= 0 && correct <= 3) {
                for (i = 0; i < 4; i++) trim_copy(texts[i], sizeof(texts[i]), fields[5 + i]);
                stmt = db_prep("INSERT INTO questions(subject_id, chapter, text, difficulty, score, is_active, created_by, created_at) VALUES(?,?,?,?,?,1,?,?)");
                sqlite3_bind_int(stmt, 1, subject_id);
                db_bind_text(stmt, 2, fields[1]);
                db_bind_text(stmt, 3, fields[4]);
                db_bind_text(stmt, 4, fields[2]);
                sqlite3_bind_double(stmt, 5, atof(fields[3]));
                sqlite3_bind_int(stmt, 6, actor.user_id);
                sqlite3_bind_int64(stmt, 7, now_sec());
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                question_id = (int)sqlite3_last_insert_rowid(g_db);
                insert_answers(question_id, texts, correct);
                imported++;
            }
        }
        line = strtok(NULL, "\n");
    }
    free(csv);
    audit_add(actor.user_id, "IMPORT_QUESTIONS", "Nhập câu hỏi từ CSV", request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "imported");
    w_num(&w, imported);
    w_end(&w);
    reply_json(response, &w);
}
