#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static CRITICAL_SECTION g_health_lock;
static long long g_started = 0;
static long long g_requests = 0;
static double g_total_ms = 0;
static long long g_last_backup = 0;
static int g_kept = 0;
static int g_ready = 0;
static char g_note[160] = "Chưa sao lưu";

static void setting_text(const char* key, char* out, size_t cap, const char* fallback) {
    sqlite3_stmt* stmt = db_prep("SELECT value FROM system_settings WHERE key=?");
    copy_str(out, cap, fallback ? fallback : "");
    if (!stmt) return;
    db_bind_text(stmt, 1, key);
    if (sqlite3_step(stmt) == SQLITE_ROW && db_text(stmt, 0)[0]) copy_str(out, cap, db_text(stmt, 0));
    sqlite3_finalize(stmt);
}

void stats_start(void) {
    InitializeCriticalSection(&g_health_lock);
    g_started = now_sec();
    g_ready = 1;
}

void stats_note(double ms) {
    EnterCriticalSection(&g_health_lock);
    g_requests++;
    g_total_ms += ms;
    LeaveCriticalSection(&g_health_lock);
}

void write_health(W* w) {
    long long started, requests, last_backup, now;
    double total_ms;
    int kept, ready;
    char note[160];
    EnterCriticalSection(&g_health_lock);
    started = g_started;
    requests = g_requests;
    total_ms = g_total_ms;
    last_backup = g_last_backup;
    kept = g_kept;
    ready = g_ready;
    copy_str(note, sizeof(note), g_note);
    LeaveCriticalSection(&g_health_lock);
    now = now_sec();
    w_obj(w);
    w_key(w, "status");
    w_str(w, ready ? "up" : "down");
    w_key(w, "serverTime");
    w_num(w, (double)now);
    w_key(w, "uptimeSec");
    w_num(w, (double)(now - started));
    w_key(w, "requests");
    w_num(w, (double)requests);
    w_key(w, "avgHandleMs");
    w_num(w, requests ? total_ms / (double)requests : 0);
    w_key(w, "lastBackupAt");
    w_num(w, (double)last_backup);
    w_key(w, "backupsKept");
    w_num(w, kept);
    w_key(w, "backupNote");
    w_str(w, note);
    w_key(w, "dataReady");
    w_bool(w, ready);
    w_key(w, "name");
    {
        char school[160];
        setting_text("schoolName", school, sizeof(school), "Phòng thi trực tuyến");
        w_str(w, school);
    }
    w_end(w);
}

static int count_backups(void) {
    char dir[MAX_PATH];
    char names[32][32];
    path_under(dir, sizeof(dir), "data\\backups");
    return list_subdirs(dir, names, 32);
}

static void prune_backups(void) {
    char dir[MAX_PATH];
    char names[32][32];
    int n;
    int i;
    int j;
    path_under(dir, sizeof(dir), "data\\backups");
    n = list_subdirs(dir, names, 32);
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (strcmp(names[j], names[i]) > 0) {
                char tmp[32];
                copy_str(tmp, sizeof(tmp), names[i]);
                copy_str(names[i], sizeof(names[i]), names[j]);
                copy_str(names[j], sizeof(names[j]), tmp);
            }
        }
    }
    for (i = 5; i < n && i < 32; i++) {
        char relative[160];
        char child[MAX_PATH];
        snprintf(relative, sizeof(relative), "data\\backups\\%s", names[i]);
        path_under(child, sizeof(child), relative);
        remove_dir_contents(child);
    }
}

int ops_backup(void) {
    char stamp[32];
    char relative[160];
    char dir[MAX_PATH];
    char file[MAX_PATH];
    time_t now = time(NULL);
    struct tm local;
    sqlite3* dest = NULL;
    sqlite3_backup* backup;
    int kept;
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    snprintf(relative, sizeof(relative), "data\\backups\\%s", stamp);
    path_under(dir, sizeof(dir), relative);
    ensure_dir(dir);
    snprintf(relative, sizeof(relative), "data\\backups\\%s\\exam.db", stamp);
    path_under(file, sizeof(file), relative);
    if (sqlite3_open(file, &dest) != SQLITE_OK) {
        sqlite3_close(dest);
        return 0;
    }
    backup = sqlite3_backup_init(dest, "main", g_db, "main");
    if (!backup || sqlite3_backup_step(backup, -1) != SQLITE_DONE) {
        if (backup) sqlite3_backup_finish(backup);
        sqlite3_close(dest);
        return 0;
    }
    sqlite3_backup_finish(backup);
    sqlite3_close(dest);
    prune_backups();
    kept = count_backups();
    EnterCriticalSection(&g_health_lock);
    g_last_backup = now_sec();
    g_kept = kept;
    copy_str(g_note, sizeof(g_note), "Đã sao lưu cơ sở dữ liệu");
    LeaveCriticalSection(&g_health_lock);
    return 1;
}

void route_health(Request* request, Response* response) {
    W w;
    (void)request;
    reply_begin(&w);
    write_health(&w);
    reply_json(response, &w);
}

int setting_int(const char* key, int fallback) {
    sqlite3_stmt* stmt = db_prep("SELECT value FROM system_settings WHERE key=?");
    int value = fallback;
    if (!stmt) return fallback;
    db_bind_text(stmt, 1, key);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = atoi(db_text(stmt, 0));
        sqlite3_finalize(stmt);
        return value;
    }
    sqlite3_finalize(stmt);
    return fallback;
}

static void put_setting(const char* key, int value) {
    char text[16];
    sqlite3_stmt* stmt = db_prep("INSERT INTO system_settings(key, value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
    snprintf(text, sizeof(text), "%d", value);
    if (!stmt) return;
    db_bind_text(stmt, 1, key);
    db_bind_text(stmt, 2, text);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void write_settings(W* w) {
    w_obj(w);
    w_key(w, "defaultDuration");
    w_num(w, setting_int("defaultDuration", 45));
    w_key(w, "defaultEasy");
    w_num(w, setting_int("defaultEasy", 8));
    w_key(w, "defaultMedium");
    w_num(w, setting_int("defaultMedium", 8));
    w_key(w, "defaultHard");
    w_num(w, setting_int("defaultHard", 4));
    w_key(w, "autosaveSec");
    w_num(w, setting_int("autosaveSec", 8));
    w_key(w, "shortDisconnect");
    w_num(w, setting_int("shortDisconnect", 30));
    w_key(w, "longDisconnect");
    w_num(w, setting_int("longDisconnect", 300));
    w_key(w, "syncGrace");
    w_num(w, setting_int("syncGrace", 300));
    w_end(w);
}

void route_settings_get(Request* request, Response* response) {
    Actor actor;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    reply_begin(&w);
    write_settings(&w);
    reply_json(response, &w);
}

void route_settings_put(Request* request, Response* response) {
    Actor actor;
    int duration = (int)js_num(request->json, "defaultDuration", 45);
    int easy = (int)js_num(request->json, "defaultEasy", 8);
    int medium = (int)js_num(request->json, "defaultMedium", 8);
    int hard = (int)js_num(request->json, "defaultHard", 4);
    int autosave = (int)js_num(request->json, "autosaveSec", 8);
    int short_s = (int)js_num(request->json, "shortDisconnect", 30);
    int long_s = (int)js_num(request->json, "longDisconnect", 300);
    int grace = (int)js_num(request->json, "syncGrace", 300);
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    if (duration < 1 || duration > 300 || easy < 0 || medium < 0 || hard < 0 || easy + medium + hard < 1 || easy + medium + hard > 60) {
        reply_fail(response, 400, "Cấu hình ma trận hoặc thời lượng không hợp lệ");
        return;
    }
    if (autosave < 5 || autosave > 30 || short_s < 5 || short_s > 180 || long_s <= short_s || long_s > 1800 || grace < 30 || grace > 900) {
        reply_fail(response, 400, "Cấu hình lưu bài hoặc mất kết nối không hợp lệ");
        return;
    }
    put_setting("defaultDuration", duration);
    put_setting("defaultEasy", easy);
    put_setting("defaultMedium", medium);
    put_setting("defaultHard", hard);
    put_setting("autosaveSec", autosave);
    put_setting("shortDisconnect", short_s);
    put_setting("longDisconnect", long_s);
    put_setting("syncGrace", grace);
    audit_add(actor.user_id, "UPDATE_SETTINGS", "Cập nhật cấu hình hệ thống", request->ip);
    reply_begin(&w);
    write_settings(&w);
    reply_json(response, &w);
}

static void put_text(const char* key, const char* value) {
    sqlite3_stmt* stmt = db_prep("INSERT INTO system_settings(key, value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
    if (!stmt) return;
    db_bind_text(stmt, 1, key);
    db_bind_text(stmt, 2, value ? value : "");
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static int one_of(const char* value, const char* a, const char* b, const char* c, const char* d) {
    return strcmp(value, a) == 0 || strcmp(value, b) == 0 || strcmp(value, c) == 0 || strcmp(value, d) == 0;
}

static const char* level_label(const char* level) {
    if (strcmp(level, "college") == 0) return "Cao đẳng";
    if (strcmp(level, "school") == 0) return "Phổ thông";
    if (strcmp(level, "center") == 0) return "Trung tâm";
    return "Đại học";
}

static const char* student_word(const char* level) {
    if (strcmp(level, "school") == 0) return "Học sinh";
    if (strcmp(level, "center") == 0) return "Học viên";
    return "Sinh viên";
}

static const char* teacher_word(const char* level) {
    if (strcmp(level, "school") == 0) return "Giáo viên";
    return "Giảng viên";
}

static int logo_ok(const char* logo) {
    if (!logo || !logo[0]) return 1;
    if (strlen(logo) > 120000) return 0;
    if (strncmp(logo, "data:image/jpeg", 15) != 0 && strncmp(logo, "data:image/png", 14) != 0 && strncmp(logo, "data:image/webp", 15) != 0) return 0;
    return strstr(logo, ";base64,") != NULL;
}

static int site_ok(const char* url) {
    if (!url[0]) return 1;
    if (strlen(url) > 180) return 0;
    if (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0) return 0;
    return strchr(url, ' ') == NULL && strchr(url, '"') == NULL && strchr(url, '<') == NULL;
}

static void write_brand(W* w) {
    char name[160];
    char short_name[64];
    char level[24];
    char motto[200];
    char address[200];
    char phone[40];
    char email[120];
    char website[200];
    char theme[16];
    char* logo = (char*)malloc(120016);
    if (!logo) logo = short_name;
    setting_text("schoolName", name, sizeof(name), "Phòng thi trực tuyến");
    setting_text("schoolShort", short_name, sizeof(short_name), "THI");
    setting_text("schoolLevel", level, sizeof(level), "university");
    setting_text("schoolMotto", motto, sizeof(motto), "");
    setting_text("schoolAddress", address, sizeof(address), "");
    setting_text("schoolPhone", phone, sizeof(phone), "");
    setting_text("schoolEmail", email, sizeof(email), "");
    setting_text("schoolWebsite", website, sizeof(website), "");
    setting_text("schoolTheme", theme, sizeof(theme), "navy");
    if (logo != short_name) setting_text("schoolLogo", logo, 120008, "");
    if (!one_of(level, "university", "college", "school", "center")) copy_str(level, sizeof(level), "university");
    if (!one_of(theme, "navy", "forest", "wine", "teal")) copy_str(theme, sizeof(theme), "navy");
    w_obj(w);
    w_key(w, "schoolName");
    w_str(w, name);
    w_key(w, "schoolShort");
    w_str(w, short_name);
    w_key(w, "level");
    w_str(w, level);
    w_key(w, "levelLabel");
    w_str(w, level_label(level));
    w_key(w, "studentWord");
    w_str(w, student_word(level));
    w_key(w, "teacherWord");
    w_str(w, teacher_word(level));
    w_key(w, "motto");
    w_str(w, motto);
    w_key(w, "address");
    w_str(w, address);
    w_key(w, "phone");
    w_str(w, phone);
    w_key(w, "email");
    w_str(w, email);
    w_key(w, "website");
    w_str(w, website);
    w_key(w, "theme");
    w_str(w, theme);
    w_key(w, "logo");
    w_str(w, logo == short_name ? "" : logo);
    w_end(w);
    if (logo != short_name) free(logo);
}

void route_brand_get(Request* request, Response* response) {
    W w;
    (void)request;
    reply_begin(&w);
    write_brand(&w);
    reply_json(response, &w);
}

void route_brand_put(Request* request, Response* response) {
    Actor actor;
    char name[160];
    char short_name[64];
    char level[24];
    char motto[200];
    char address[200];
    char phone[40];
    char email[120];
    char website[200];
    char theme[16];
    const char* logo = js_str(request->json, "logo", "");
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    trim_copy(name, sizeof(name), js_str(request->json, "schoolName", ""));
    trim_copy(short_name, sizeof(short_name), js_str(request->json, "schoolShort", ""));
    trim_copy(level, sizeof(level), js_str(request->json, "level", "university"));
    trim_copy(motto, sizeof(motto), js_str(request->json, "motto", ""));
    trim_copy(address, sizeof(address), js_str(request->json, "address", ""));
    trim_copy(phone, sizeof(phone), js_str(request->json, "phone", ""));
    trim_copy(email, sizeof(email), js_str(request->json, "email", ""));
    trim_copy(website, sizeof(website), js_str(request->json, "website", ""));
    trim_copy(theme, sizeof(theme), js_str(request->json, "theme", "navy"));
    if (strlen(name) < 2 || strlen(name) > 140) {
        reply_fail(response, 400, "Tên trường từ 2 đến 140 ký tự");
        return;
    }
    if (!short_name[0] || strlen(short_name) > 48) {
        reply_fail(response, 400, "Tên viết tắt quá dài");
        return;
    }
    if (!one_of(level, "university", "college", "school", "center")) {
        reply_fail(response, 400, "Cấp trường không hợp lệ");
        return;
    }
    if (!one_of(theme, "navy", "forest", "wine", "teal")) {
        reply_fail(response, 400, "Màu mẫu không hợp lệ");
        return;
    }
    if (strlen(motto) > 180 || strlen(address) > 180 || strlen(phone) > 32) {
        reply_fail(response, 400, "Khẩu hiệu, địa chỉ hoặc điện thoại quá dài");
        return;
    }
    if (email[0] && (strlen(email) > 100 || !strchr(email, '@') || strchr(email, ' '))) {
        reply_fail(response, 400, "Email nhà trường không hợp lệ");
        return;
    }
    if (!site_ok(website)) {
        reply_fail(response, 400, "Website cần bắt đầu bằng http:// hoặc https://");
        return;
    }
    if (!logo_ok(logo)) {
        reply_fail(response, 400, "Logo phải là ảnh JPG, PNG hoặc WebP");
        return;
    }
    put_text("schoolName", name);
    put_text("schoolShort", short_name);
    put_text("schoolLevel", level);
    put_text("schoolMotto", motto);
    put_text("schoolAddress", address);
    put_text("schoolPhone", phone);
    put_text("schoolEmail", email);
    put_text("schoolWebsite", website);
    put_text("schoolTheme", theme);
    put_text("schoolLogo", logo);
    audit_add(actor.user_id, "UPDATE_BRAND", name, request->ip);
    reply_begin(&w);
    write_brand(&w);
    reply_json(response, &w);
}

static int backup_name_ok(const char* name) {
    int i;
    int dash = 0;
    if (!name || strlen(name) != 15) return 0;
    for (i = 0; name[i]; i++) {
        if (name[i] == '-') dash++;
        else if (name[i] < '0' || name[i] > '9') return 0;
    }
    return dash == 1;
}

void route_backups_list(Request* request, Response* response) {
    Actor actor;
    char dir[MAX_PATH];
    char found[32][32];
    char names[32][32];
    int found_count;
    int n = 0;
    int i;
    int j;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    path_under(dir, sizeof(dir), "data\\backups");
    found_count = list_subdirs(dir, found, 32);
    for (i = 0; i < found_count && i < 32; i++) {
        if (backup_name_ok(found[i]) && n < 32) {
            copy_str(names[n], sizeof(names[n]), found[i]);
            n++;
        }
    }
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (strcmp(names[j], names[i]) > 0) {
                char tmp[32];
                copy_str(tmp, sizeof(tmp), names[i]);
                copy_str(names[i], sizeof(names[i]), names[j]);
                copy_str(names[j], sizeof(names[j]), tmp);
            }
        }
    }
    reply_begin(&w);
    w_arr(&w);
    for (i = 0; i < n && i < 5; i++) {
        w_obj(&w);
        w_key(&w, "name");
        w_str(&w, names[i]);
        w_end(&w);
    }
    w_end(&w);
    reply_json(response, &w);
}

void route_restore(Request* request, Response* response) {
    Actor actor;
    char name[32];
    char relative[180];
    char file[MAX_PATH];
    sqlite3* src = NULL;
    sqlite3_backup* backup = NULL;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    trim_copy(name, sizeof(name), js_str(request->json, "name", ""));
    if (!backup_name_ok(name)) {
        reply_fail(response, 400, "Bản sao lưu không hợp lệ");
        return;
    }
    snprintf(relative, sizeof(relative), "data\\backups\\%s\\exam.db", name);
    path_under(file, sizeof(file), relative);
    if (!file_exists(file) || sqlite3_open_v2(file, &src, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (src) sqlite3_close(src);
        reply_fail(response, 404, "Không tìm thấy bản sao lưu");
        return;
    }
    backup = sqlite3_backup_init(g_db, "main", src, "main");
    if (!backup || sqlite3_backup_step(backup, -1) != SQLITE_DONE) {
        if (backup) sqlite3_backup_finish(backup);
        sqlite3_close(src);
        reply_fail(response, 500, "Không khôi phục được dữ liệu");
        return;
    }
    sqlite3_backup_finish(backup);
    sqlite3_close(src);
    audit_add(actor.user_id, "RESTORE", name, request->ip);
    reply_begin(&w);
    w_obj(&w);
    w_key(&w, "restored");
    w_str(&w, name);
    w_end(&w);
    reply_json(response, &w);
}

void route_backup(Request* request, Response* response) {
    Actor actor;
    W w;
    if (!require_role(request, response, &actor, "admin")) return;
    if (!ops_backup()) {
        reply_fail(response, 500, "Không sao lưu được dữ liệu");
        return;
    }
    audit_add(actor.user_id, "BACKUP", "Sao lưu cơ sở dữ liệu", request->ip);
    reply_begin(&w);
    write_health(&w);
    reply_json(response, &w);
}
