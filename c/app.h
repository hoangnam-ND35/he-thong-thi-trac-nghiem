#ifndef OES_APP_H
#define OES_APP_H

#include "http.h"
#include "db.h"
#include "util.h"

typedef struct Actor {
    int user_id;
    int profile_id;
    int student_id;
    int lecturer_id;
    int must_change;
    char role[16];
    char username[64];
    char full_name[160];
    char email[160];
    char phone[40];
    char status[24];
    char account_status[24];
    char student_code[40];
    char dob[16];
    char gender[16];
    char class_name[64];
    char faculty[128];
    char lecturer_code[40];
    char department[128];
} Actor;

int actor_load(const char* token, Actor* actor);
int require_user(Request* request, Response* response, Actor* actor);
int require_role(Request* request, Response* response, Actor* actor, const char* roles);
void write_user(W* w, int user_id);
int owns_subject(const Actor* actor, int subject_id);
const char* make_session(int user_id);
int role_is(const char* role, const char* allowed);
int create_account(const Js* body, int self_register, int* user_id, char* err, size_t err_cap);

void seed_if_empty(void);
void stats_start(void);
void stats_note(double ms);
void write_health(W* w);
int ops_backup(void);
int setting_int(const char* key, int fallback);
int package_on(const char* key);
void route_package_get(Request* request, Response* response);
void route_package_put(Request* request, Response* response);
void route_orders_list(Request* request, Response* response);
void route_order_pay(Request* request, Response* response);
void attempt_sweep(void);

void route_health(Request* request, Response* response);
void route_backup(Request* request, Response* response);
void route_settings_get(Request* request, Response* response);
void route_settings_put(Request* request, Response* response);
void route_brand_get(Request* request, Response* response);
void route_brand_put(Request* request, Response* response);
void route_backups_list(Request* request, Response* response);
void route_restore(Request* request, Response* response);
void route_login(Request* request, Response* response);
void route_logout(Request* request, Response* response);
void route_me(Request* request, Response* response);
void route_change_password(Request* request, Response* response);
void route_register(Request* request, Response* response);
void route_register_options(Request* request, Response* response);
void route_profiles_list(Request* request, Response* response);
void route_profiles_create(Request* request, Response* response);
void route_profile_me(Request* request, Response* response);
void route_avatar(Request* request, Response* response);
void route_profile_update(Request* request, Response* response);
void route_profile_lock(Request* request, Response* response);
void route_profile_unlock(Request* request, Response* response);
void route_profile_disable(Request* request, Response* response);
void route_profile_enable(Request* request, Response* response);
void route_profile_reset(Request* request, Response* response);
void route_profile_delete(Request* request, Response* response);
void route_history(Request* request, Response* response);
void route_departments_list(Request* request, Response* response);
void route_departments_create(Request* request, Response* response);
void route_department_update(Request* request, Response* response);
void route_classes_list(Request* request, Response* response);
void route_classes_create(Request* request, Response* response);
void route_class_update(Request* request, Response* response);
void route_subjects_list(Request* request, Response* response);
void route_subjects_create(Request* request, Response* response);
void route_subject_update(Request* request, Response* response);
void route_subject_delete(Request* request, Response* response);
void route_questions_list(Request* request, Response* response);
void route_question_create(Request* request, Response* response);
void route_question_update(Request* request, Response* response);
void route_question_disable(Request* request, Response* response);
void route_question_bank(Request* request, Response* response);
void route_question_template(Request* request, Response* response);
void route_question_export(Request* request, Response* response);
void route_question_import(Request* request, Response* response);
void route_exams_list(Request* request, Response* response);
void route_exam_create(Request* request, Response* response);
void route_exam_update(Request* request, Response* response);
void route_exam_publish(Request* request, Response* response);
void route_exam_close(Request* request, Response* response);
void route_exam_delete(Request* request, Response* response);
void route_exam_analysis(Request* request, Response* response);
void route_exam_results(Request* request, Response* response);
void route_exam_start(Request* request, Response* response);
void route_practice_start(Request* request, Response* response);
void route_student_exams(Request* request, Response* response);
void route_attempt_paper(Request* request, Response* response);
void route_attempt_time(Request* request, Response* response);
void route_attempt_sync(Request* request, Response* response);
void route_attempt_submit(Request* request, Response* response);
void route_attempt_event(Request* request, Response* response);
void route_attempt_result(Request* request, Response* response);
void route_dash_admin(Request* request, Response* response);
void route_dash_lecturer(Request* request, Response* response);
void route_dash_student(Request* request, Response* response);
void route_teacher_upgrade_get(Request* request, Response* response);
void route_teacher_upgrade_submit(Request* request, Response* response);
void route_teacher_upgrades_list(Request* request, Response* response);
void route_teacher_upgrade_approve(Request* request, Response* response);
void route_teacher_upgrade_reject(Request* request, Response* response);
void route_teacher_codes_list(Request* request, Response* response);
void route_teacher_codes_create(Request* request, Response* response);
void route_teacher_code_disable(Request* request, Response* response);
void route_teacher_code_redeem(Request* request, Response* response);
void route_audit(Request* request, Response* response);
void register_routes(void);

#endif
