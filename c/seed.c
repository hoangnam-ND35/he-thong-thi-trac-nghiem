#include "app.h"

#include <stdio.h>
#include <string.h>

static int add_user(const char* username, const char* password, const char* role, const char* name, const char* email, const char* phone) {
    char salt[40];
    char hash[65];
    sqlite3_stmt* stmt;
    int profile_id;
    int user_id;
    random_hex(16, salt);
    hash_password(salt, password, hash);
    stmt = db_prep("INSERT INTO profiles(user_id, role, full_name, email, phone, avatar, status) VALUES(0,?,?,?,?,'','active')");
    db_bind_text(stmt, 1, role);
    db_bind_text(stmt, 2, name);
    db_bind_text(stmt, 3, email);
    db_bind_text(stmt, 4, phone);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    profile_id = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("INSERT INTO users(username, password_hash, salt, role, status, profile_id, created_at, must_change) VALUES(?,?,?,?,'active',?,?,0)");
    db_bind_text(stmt, 1, username);
    db_bind_text(stmt, 2, hash);
    db_bind_text(stmt, 3, salt);
    db_bind_text(stmt, 4, role);
    sqlite3_bind_int(stmt, 5, profile_id);
    sqlite3_bind_int64(stmt, 6, now_sec());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    user_id = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("UPDATE profiles SET user_id=? WHERE id=?");
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, profile_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return user_id;
}

static int profile_of(int user_id) {
    return scalar_int("SELECT profile_id FROM users WHERE id=?", user_id, -1);
}

static void add_question(int subject_id, int creator, const char* chapter, const char* difficulty, double score, const char* text,
                         int correct, const char* a, const char* b, const char* c, const char* d) {
    const char* options[4] = {a, b, c, d};
    sqlite3_stmt* stmt = db_prep("INSERT INTO questions(subject_id, chapter, text, difficulty, score, is_active, created_by, created_at) VALUES(?,?,?,?,?,1,?,?)");
    int question_id;
    int i;
    db_bind_text(stmt, 2, chapter);
    db_bind_text(stmt, 3, text);
    db_bind_text(stmt, 4, difficulty);
    sqlite3_bind_int(stmt, 1, subject_id);
    sqlite3_bind_double(stmt, 5, score);
    sqlite3_bind_int(stmt, 6, creator);
    sqlite3_bind_int64(stmt, 7, now_sec());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    question_id = (int)sqlite3_last_insert_rowid(g_db);
    for (i = 0; i < 4; i++) {
        stmt = db_prep("INSERT INTO answers(question_id, text, is_correct, display_order) VALUES(?,?,?,?)");
        sqlite3_bind_int(stmt, 1, question_id);
        db_bind_text(stmt, 2, options[i]);
        sqlite3_bind_int(stmt, 3, i == correct);
        sqlite3_bind_int(stmt, 4, i + 1);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

static void add_exam(int subject_id, int creator, int class_id, const char* title, const char* description, int minutes,
                     int easy, int medium, int hard, int max_attempts, int grace, double total) {
    sqlite3_stmt* stmt = db_prep(
        "INSERT INTO exams(subject_id, title, description, start_time, end_time, duration_minutes, easy_count, medium_count, hard_count, "
        "total_questions, total_score, shuffle_questions, shuffle_answers, auto_submit, max_attempts, status, class_id, "
        "short_disconnect_sec, long_disconnect_sec, sync_grace_sec, created_by, created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,1,1,1,?,'published',?,30,300,?,?,?)");
    long long now = now_sec();
    sqlite3_bind_int(stmt, 1, subject_id);
    db_bind_text(stmt, 2, title);
    db_bind_text(stmt, 3, description);
    sqlite3_bind_int64(stmt, 4, now - 3600);
    sqlite3_bind_int64(stmt, 5, now + 14 * 24 * 3600);
    sqlite3_bind_int(stmt, 6, minutes);
    sqlite3_bind_int(stmt, 7, easy);
    sqlite3_bind_int(stmt, 8, medium);
    sqlite3_bind_int(stmt, 9, hard);
    sqlite3_bind_int(stmt, 10, easy + medium + hard);
    sqlite3_bind_double(stmt, 11, total);
    sqlite3_bind_int(stmt, 12, max_attempts);
    sqlite3_bind_int(stmt, 13, class_id);
    sqlite3_bind_int(stmt, 14, grace);
    sqlite3_bind_int(stmt, 15, creator);
    sqlite3_bind_int64(stmt, 16, now);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void seed_if_empty(void) {
    const char* faculty = "Công nghệ thông tin";
    int admin_id;
    int gv_a;
    int gv_b;
    int software;
    int systems;
    int class1;
    int java_id;
    int db_id;
    int students_n;
    struct { const char* user; const char* name; const char* code; const char* email; const char* phone; const char* dob; const char* gender; const char* room; } students[] = {
        {"sv.an", "Nguyễn An", "SV22001", "an@student.edu", "0912000001", "2004-03-12", "Nam", "DH22TIN01"},
        {"sv.binh", "Trần Bình", "SV22002", "binh@student.edu", "0912000002", "2004-07-02", "Nam", "DH22TIN01"},
        {"sv.chi", "Lê Chi", "SV22003", "chi@student.edu", "0912000003", "2004-11-21", "Nữ", "DH22TIN01"},
        {"sv.dung", "Phạm Dung", "SV22004", "dung@student.edu", "0912000004", "2004-01-09", "Nam", "DH22TIN02"},
        {"sv.em", "Hoàng Em", "SV22005", "em@student.edu", "0912000005", "2004-05-30", "Nữ", "DH22TIN02"}};
    sqlite3_stmt* stmt;
    if (db_count("SELECT COUNT(*) FROM users") > 0) return;
    db_begin();
    admin_id = add_user("admin", "Admin@123", "admin", "Quản trị hệ thống", "admin@school.edu", "0901000001");
    gv_a = add_user("gv.anva", "Gv@12345", "lecturer", "Nguyễn Văn A", "anva@school.edu", "0901000002");
    gv_b = add_user("gv.thib", "Gv@12345", "lecturer", "Trần Thị B", "thib@school.edu", "0901000003");
    stmt = db_prep("INSERT INTO lecturers(user_id, profile_id, lecturer_code, department, faculty, status) VALUES(?,?,?,?,?,'active')");
    sqlite3_bind_int(stmt, 1, gv_a);
    sqlite3_bind_int(stmt, 2, profile_of(gv_a));
    db_bind_text(stmt, 3, "GV001");
    db_bind_text(stmt, 4, "Công nghệ phần mềm");
    db_bind_text(stmt, 5, faculty);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = db_prep("INSERT INTO lecturers(user_id, profile_id, lecturer_code, department, faculty, status) VALUES(?,?,?,?,?,'active')");
    sqlite3_bind_int(stmt, 1, gv_b);
    sqlite3_bind_int(stmt, 2, profile_of(gv_b));
    db_bind_text(stmt, 3, "GV002");
    db_bind_text(stmt, 4, "Hệ thống thông tin");
    db_bind_text(stmt, 5, faculty);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = db_prep("INSERT INTO departments(name, faculty) VALUES(?,?)");
    db_bind_text(stmt, 1, "Công nghệ phần mềm");
    db_bind_text(stmt, 2, faculty);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    software = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("INSERT INTO departments(name, faculty) VALUES(?,?)");
    db_bind_text(stmt, 1, "Hệ thống thông tin");
    db_bind_text(stmt, 2, faculty);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    systems = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("INSERT INTO classes(name, faculty, department_id) VALUES(?,?,?)");
    db_bind_text(stmt, 1, "DH22TIN01");
    db_bind_text(stmt, 2, faculty);
    sqlite3_bind_int(stmt, 3, software);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    class1 = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("INSERT INTO classes(name, faculty, department_id) VALUES(?,?,?)");
    db_bind_text(stmt, 1, "DH22TIN02");
    db_bind_text(stmt, 2, faculty);
    sqlite3_bind_int(stmt, 3, systems);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    for (students_n = 0; students_n < 5; students_n++) {
        int user_id = add_user(students[students_n].user, "Sv@12345", "student", students[students_n].name, students[students_n].email, students[students_n].phone);
        stmt = db_prep("INSERT INTO students(user_id, profile_id, student_code, dob, gender, class_name, faculty, status) VALUES(?,?,?,?,?,?,?,'active')");
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, profile_of(user_id));
        db_bind_text(stmt, 3, students[students_n].code);
        db_bind_text(stmt, 4, students[students_n].dob);
        db_bind_text(stmt, 5, students[students_n].gender);
        db_bind_text(stmt, 6, students[students_n].room);
        db_bind_text(stmt, 7, faculty);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    stmt = db_prep("INSERT INTO subjects(code, name, credits, department_id, lecturer_id, description) VALUES('JAVA101','Lập trình Java',3,?,?,'Ngôn ngữ Java, hướng đối tượng và các thư viện cơ bản.')");
    sqlite3_bind_int(stmt, 1, software);
    sqlite3_bind_int(stmt, 2, scalar_int("SELECT id FROM lecturers WHERE user_id=?", gv_a, -1));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    java_id = (int)sqlite3_last_insert_rowid(g_db);
    stmt = db_prep("INSERT INTO subjects(code, name, credits, department_id, lecturer_id, description) VALUES('CSDL01','Cơ sở dữ liệu',3,?,?,'Mô hình dữ liệu quan hệ và ngôn ngữ SQL.')");
    sqlite3_bind_int(stmt, 1, systems);
    sqlite3_bind_int(stmt, 2, scalar_int("SELECT id FROM lecturers WHERE user_id=?", gv_b, -1));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_id = (int)sqlite3_last_insert_rowid(g_db);

    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Java là ngôn ngữ lập trình nào?", 0, "Hướng đối tượng, chương trình chạy trên JVM", "Một hệ điều hành", "Một hệ quản trị cơ sở dữ liệu", "Một trình duyệt web");
    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Câu lệnh nào dùng để in một dòng ra màn hình?", 0, "System.out.println", "echo", "console.writeLine", "print.screen");
    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Kiểu dữ liệu nào lưu số nguyên 32-bit?", 0, "int", "double", "boolean", "String");
    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Chỉ số của phần tử đầu tiên trong mảng Java là bao nhiêu?", 0, "0", "1", "-1", "Tùy hệ điều hành");
    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Từ khóa nào dùng để khai báo một hằng số?", 0, "final", "define", "fixed", "const");
    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Kiểu dữ liệu nào chỉ nhận giá trị true hoặc false?", 0, "boolean", "bit", "char", "int");
    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Ký hiệu nào kết thúc một câu lệnh Java?", 0, "Dấu chấm phẩy ;", "Dấu chấm .", "Dấu hai chấm :", "Dấu phẩy ,");
    add_question(java_id, gv_a, "Chương 1. Cơ bản", "easy", 1, "Lớp Scanner nằm trong gói nào?", 0, "java.util", "java.io", "java.lang", "java.net");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "medium", 1.5, "Kế thừa một lớp trong Java dùng từ khóa nào?", 0, "extends", "implements", "inherit", "using");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "medium", 1.5, "Một lớp triển khai interface bằng từ khóa nào?", 0, "implements", "extends", "imports", "instance");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "medium", 1.5, "Nạp chồng phương thức (overloading) là gì?", 0, "Các phương thức cùng tên nhưng khác danh sách tham số", "Lớp con viết lại phương thức của lớp cha với cùng tham số", "Xóa một phương thức khỏi lớp", "Đổi tên lớp cho trùng phương thức");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "medium", 1.5, "Ghi đè phương thức (overriding) xảy ra khi nào?", 0, "Lớp con định nghĩa lại phương thức của lớp cha", "Hai phương thức cùng lớp chỉ khác kiểu tham số", "Phương thức được khai báo static", "Chỉ xảy ra với hàm tạo");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "medium", 1.5, "Cách đúng để so sánh nội dung hai chuỗi là gì?", 0, "Gọi phương thức equals", "Dùng toán tử ==", "Dùng toán tử =", "So sánh độ dài là đủ");
    add_question(java_id, gv_a, "Chương 3. Collections", "medium", 1.5, "Phương thức nào thêm một phần tử vào ArrayList?", 0, "add", "push", "append", "insertLast");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "medium", 1.5, "Khối nào dùng để xử lý ngoại lệ?", 0, "try-catch", "if-else", "for-while", "do-error");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "medium", 1.5, "Phương thức static được gọi như thế nào?", 0, "Qua tên lớp, không bắt buộc phải có đối tượng", "Chỉ gọi được qua đối tượng đã new", "Chỉ gọi được bên trong constructor", "Không thể gọi từ bên ngoài lớp");
    add_question(java_id, gv_a, "Chương 3. Collections", "hard", 2, "HashMap cho phép bao nhiêu khóa null?", 0, "Một khóa null", "Không cho phép khóa null", "Không giới hạn khóa null", "Chỉ cho phép giá trị null, cấm khóa null");
    add_question(java_id, gv_a, "Chương 4. Nâng cao", "hard", 2, "Khối finally được thực hiện khi nào?", 0, "Gần như luôn chạy, dù có ngoại lệ hay không", "Chỉ chạy khi có ngoại lệ", "Chỉ chạy khi không có ngoại lệ", "Không chạy nếu đã có catch");
    add_question(java_id, gv_a, "Chương 2. Hướng đối tượng", "hard", 2, "Đa hình trong Java cho phép điều gì?", 0, "Biến kiểu lớp cha tham chiếu tới đối tượng lớp con", "Một file có nhiều lớp public", "Cấm kế thừa giữa các lớp", "Chỉ dùng được với thành viên static");
    add_question(java_id, gv_a, "Chương 4. Nâng cao", "hard", 2, "Checked exception đòi hỏi lập trình viên phải làm gì?", 0, "Khai báo throws hoặc bắt bằng try-catch", "Bỏ qua hoàn toàn, trình biên dịch không kiểm tra", "Chỉ được dùng với RuntimeException", "Chỉ được bắt bên trong hàm main");

    add_question(db_id, gv_b, "Chương 1. SQL", "easy", 1, "SQL là viết tắt của cụm nào?", 0, "Structured Query Language", "Simple Question List", "System Quality Layer", "Standard Queue Library");
    add_question(db_id, gv_b, "Chương 1. SQL", "easy", 1, "Khóa chính của một bảng dùng để làm gì?", 0, "Xác định duy nhất mỗi dòng", "Luôn cho phép giá trị trùng", "Chỉ dùng để sắp xếp giao diện", "Thay thế mọi khóa ngoại");
    add_question(db_id, gv_b, "Chương 1. SQL", "easy", 1, "Lệnh nào dùng để truy vấn dữ liệu?", 0, "SELECT", "UPDATE", "DROP", "GRANT");
    add_question(db_id, gv_b, "Chương 1. SQL", "easy", 1, "Lệnh nào xóa các dòng dữ liệu khỏi bảng?", 0, "DELETE", "REMOVE FILE", "ERASE", "CUT");
    add_question(db_id, gv_b, "Chương 2. Quan hệ", "medium", 1.5, "INNER JOIN trả về những dòng nào?", 0, "Dòng khớp nhau giữa hai bảng theo điều kiện nối", "Mọi dòng của bảng bên trái, kể cả không khớp", "Chỉ các dòng không có khóa ngoại", "Toàn bộ tích Descartes không lọc");
    add_question(db_id, gv_b, "Chương 2. Quan hệ", "medium", 1.5, "HAVING khác WHERE ở điểm nào?", 0, "HAVING lọc sau khi nhóm, WHERE lọc trước khi nhóm", "Hai mệnh đề luôn giống nhau", "WHERE chỉ dùng với JOIN", "HAVING không dùng được với COUNT");
    add_question(db_id, gv_b, "Chương 2. Quan hệ", "medium", 1.5, "Khóa ngoại dùng để làm gì?", 0, "Tham chiếu tới khóa của bảng khác, bảo đảm toàn vẹn", "Mã hóa mật khẩu người dùng", "Tăng tốc mọi câu SELECT mà không cần chỉ mục", "Thay thế khóa chính của chính bảng đó");
    add_question(db_id, gv_b, "Chương 3. Giao tác", "hard", 2, "Thuộc tính ACID của giao tác gồm những gì?", 0, "Atomicity, Consistency, Isolation, Durability", "Access, Cache, Index, Disk", "Add, Commit, Insert, Delete", "Auth, Control, Identity, Domain");

    add_exam(java_id, gv_a, class1, "Thi giữa kỳ Java", "45 phút, ma trận 8 dễ - 8 trung bình - 4 khó. Trộn câu hỏi và trộn đáp án.", 45, 8, 8, 4, 1, 300, 28);
    add_exam(java_id, gv_a, 0, "Thi thử Java - mất kết nối", "Bài ngắn để thử đồng hồ máy chủ, trộn đề và lưu bài khi mất mạng.", 10, 3, 1, 1, 5, 120, 6.5);
    add_exam(db_id, gv_b, 0, "Kiểm tra SQL", "Bốn câu theo ma trận 2 dễ, 1 trung bình, 1 khó.", 20, 2, 1, 1, 2, 300, 5.5);
    audit_add(admin_id, "SEED", "Khởi tạo dữ liệu mẫu cho hệ thống thi", "127.0.0.1");
    db_commit();
}
