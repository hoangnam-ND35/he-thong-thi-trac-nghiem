PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA busy_timeout = 3000;

CREATE TABLE IF NOT EXISTS profiles (
  id INTEGER PRIMARY KEY,
  user_id INTEGER NOT NULL DEFAULT 0,
  role TEXT NOT NULL,
  full_name TEXT NOT NULL,
  email TEXT NOT NULL DEFAULT '',
  phone TEXT NOT NULL DEFAULT '',
  avatar TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'active'
);

CREATE TABLE IF NOT EXISTS users (
  id INTEGER PRIMARY KEY,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  salt TEXT NOT NULL,
  role TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'active',
  profile_id INTEGER NOT NULL,
  created_at INTEGER NOT NULL,
  must_change INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS students (
  id INTEGER PRIMARY KEY,
  user_id INTEGER NOT NULL UNIQUE,
  profile_id INTEGER NOT NULL,
  student_code TEXT NOT NULL UNIQUE,
  dob TEXT NOT NULL DEFAULT '',
  gender TEXT NOT NULL DEFAULT '',
  class_name TEXT NOT NULL DEFAULT '',
  faculty TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'active'
);

CREATE TABLE IF NOT EXISTS lecturers (
  id INTEGER PRIMARY KEY,
  user_id INTEGER NOT NULL UNIQUE,
  profile_id INTEGER NOT NULL,
  lecturer_code TEXT NOT NULL UNIQUE,
  department TEXT NOT NULL DEFAULT '',
  faculty TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'active'
);

CREATE TABLE IF NOT EXISTS departments (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  faculty TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS classes (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  faculty TEXT NOT NULL,
  department_id INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS subjects (
  id INTEGER PRIMARY KEY,
  code TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL,
  credits INTEGER NOT NULL DEFAULT 3,
  department_id INTEGER NOT NULL,
  lecturer_id INTEGER NOT NULL,
  description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS questions (
  id INTEGER PRIMARY KEY,
  subject_id INTEGER NOT NULL,
  chapter TEXT NOT NULL DEFAULT '',
  text TEXT NOT NULL,
  difficulty TEXT NOT NULL,
  score REAL NOT NULL,
  is_active INTEGER NOT NULL DEFAULT 1,
  created_by INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS answers (
  id INTEGER PRIMARY KEY,
  question_id INTEGER NOT NULL,
  text TEXT NOT NULL,
  is_correct INTEGER NOT NULL DEFAULT 0,
  display_order INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS exams (
  id INTEGER PRIMARY KEY,
  subject_id INTEGER NOT NULL,
  title TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  start_time INTEGER NOT NULL,
  end_time INTEGER NOT NULL,
  duration_minutes INTEGER NOT NULL,
  easy_count INTEGER NOT NULL,
  medium_count INTEGER NOT NULL,
  hard_count INTEGER NOT NULL,
  total_questions INTEGER NOT NULL,
  total_score REAL NOT NULL,
  shuffle_questions INTEGER NOT NULL DEFAULT 1,
  shuffle_answers INTEGER NOT NULL DEFAULT 1,
  auto_submit INTEGER NOT NULL DEFAULT 1,
  max_attempts INTEGER NOT NULL DEFAULT 1,
  status TEXT NOT NULL DEFAULT 'draft',
  class_id INTEGER NOT NULL DEFAULT 0,
  short_disconnect_sec INTEGER NOT NULL DEFAULT 30,
  long_disconnect_sec INTEGER NOT NULL DEFAULT 300,
  sync_grace_sec INTEGER NOT NULL DEFAULT 300,
  created_by INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS exam_participants (
  exam_id INTEGER NOT NULL,
  student_id INTEGER NOT NULL,
  PRIMARY KEY (exam_id, student_id)
);

CREATE TABLE IF NOT EXISTS attempts (
  id INTEGER PRIMARY KEY,
  exam_id INTEGER NOT NULL,
  student_id INTEGER NOT NULL,
  start_time INTEGER NOT NULL,
  end_time INTEGER NOT NULL DEFAULT 0,
  deadline INTEGER NOT NULL,
  submit_time INTEGER NOT NULL DEFAULT 0,
  status TEXT NOT NULL,
  paper_code TEXT NOT NULL,
  paper_seed INTEGER NOT NULL DEFAULT 0,
  paper_total REAL NOT NULL DEFAULT 0,
  score REAL NOT NULL DEFAULT 0,
  correct_count INTEGER NOT NULL DEFAULT 0,
  wrong_count INTEGER NOT NULL DEFAULT 0,
  skipped INTEGER NOT NULL DEFAULT 0,
  is_auto INTEGER NOT NULL DEFAULT 0,
  last_sync INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS attempt_questions (
  id INTEGER PRIMARY KEY,
  attempt_id INTEGER NOT NULL,
  question_id INTEGER NOT NULL,
  order_index INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS attempt_choices (
  attempt_id INTEGER NOT NULL,
  question_id INTEGER NOT NULL,
  answer_id INTEGER NOT NULL,
  position INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS student_answers (
  attempt_id INTEGER NOT NULL,
  question_id INTEGER NOT NULL,
  answer_id INTEGER NOT NULL,
  updated_at INTEGER NOT NULL,
  PRIMARY KEY (attempt_id, question_id)
);

CREATE TABLE IF NOT EXISTS results (
  id INTEGER PRIMARY KEY,
  attempt_id INTEGER NOT NULL UNIQUE,
  student_id INTEGER NOT NULL,
  exam_id INTEGER NOT NULL,
  score REAL NOT NULL,
  total REAL NOT NULL,
  percent REAL NOT NULL,
  graded_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS notifications (
  id INTEGER PRIMARY KEY,
  user_id INTEGER NOT NULL,
  message TEXT NOT NULL,
  created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS audit_logs (
  id INTEGER PRIMARY KEY,
  user_id INTEGER NOT NULL DEFAULT 0,
  action TEXT NOT NULL,
  detail TEXT NOT NULL DEFAULT '',
  ip TEXT NOT NULL DEFAULT '',
  created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
  token TEXT PRIMARY KEY,
  user_id INTEGER NOT NULL,
  expires_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_questions_subject ON questions(subject_id, difficulty, is_active);
CREATE INDEX IF NOT EXISTS idx_answers_question ON answers(question_id, display_order);
CREATE INDEX IF NOT EXISTS idx_attempts_exam ON attempts(exam_id, student_id);
CREATE INDEX IF NOT EXISTS idx_attempt_questions ON attempt_questions(attempt_id, order_index);
CREATE INDEX IF NOT EXISTS idx_choices ON attempt_choices(attempt_id, question_id, position);
CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
CREATE TABLE IF NOT EXISTS exam_matrix (
  id INTEGER PRIMARY KEY,
  exam_id INTEGER NOT NULL,
  chapter TEXT NOT NULL,
  easy_count INTEGER NOT NULL DEFAULT 0,
  medium_count INTEGER NOT NULL DEFAULT 0,
  hard_count INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS exam_events (
  id INTEGER PRIMARY KEY,
  attempt_id INTEGER NOT NULL,
  kind TEXT NOT NULL,
  created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS system_settings (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS teacher_upgrades (
  id INTEGER PRIMARY KEY,
  user_id INTEGER NOT NULL UNIQUE,
  profile_id INTEGER NOT NULL,
  cccd TEXT NOT NULL,
  full_name TEXT NOT NULL,
  lecturer_code TEXT NOT NULL,
  department TEXT NOT NULL,
  faculty TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'pending',
  note TEXT NOT NULL DEFAULT '',
  created_at INTEGER NOT NULL,
  reviewed_at INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS package_orders (
  id INTEGER PRIMARY KEY,
  code TEXT NOT NULL UNIQUE,
  school_name TEXT NOT NULL,
  buyer_name TEXT NOT NULL,
  contact TEXT NOT NULL,
  method TEXT NOT NULL,
  amount INTEGER NOT NULL,
  exam INTEGER NOT NULL,
  question INTEGER NOT NULL,
  result INTEGER NOT NULL,
  upgrade INTEGER NOT NULL,
  seller_id INTEGER NOT NULL,
  created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS teacher_codes (
  id INTEGER PRIMARY KEY,
  code TEXT NOT NULL UNIQUE,
  max_uses INTEGER NOT NULL DEFAULT 1,
  used_count INTEGER NOT NULL DEFAULT 0,
  note TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'active',
  created_by INTEGER NOT NULL,
  created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_logs(created_at);
CREATE INDEX IF NOT EXISTS idx_matrix_exam ON exam_matrix(exam_id);
CREATE INDEX IF NOT EXISTS idx_events_attempt ON exam_events(attempt_id, kind);
