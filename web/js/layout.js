const NAV = {
  admin: [
    ["/admin/dashboard.html", "Tổng quan"],
    ["/admin/profiles.html", "Hồ sơ"],
    ["/admin/subjects.html", "Môn học"],
    ["/lecturer/questions.html", "Ngân hàng câu hỏi"],
    ["/lecturer/exams.html", "Kỳ thi"],
    ["/lecturer/results.html", "Kết quả"],
    ["/admin/brand.html", "Nhận diện trường"],
    ["/admin/settings.html", "Cấu hình"],
    ["/admin/audit.html", "Nhật ký"]
  ],
  lecturer: [
    ["/lecturer/dashboard.html", "Tổng quan"],
    ["/admin/subjects.html", "Môn học"],
    ["/lecturer/questions.html", "Ngân hàng câu hỏi"],
    ["/lecturer/exams.html", "Kỳ thi"],
    ["/lecturer/results.html", "Kết quả"],
    ["/admin/profiles.html", "Học sinh"],
    ["/student/profile.html", "Tài khoản"]
  ],
  student: [
    ["/student/dashboard.html", "Kỳ thi"],
    ["/student/profile.html", "Tài khoản"]
  ]
};

function esc(value) {
  return String(value ?? "").replace(/[&<>"']/g, (ch) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  }[ch]));
}

function roleHome(role) {
  if (role === "admin") return "/admin/dashboard.html";
  if (role === "lecturer") return "/lecturer/dashboard.html";
  return "/student/dashboard.html";
}

function roleLabel(role) {
  return { admin: "Admin", lecturer: "Giáo viên", student: "Học sinh" }[role] || role;
}

function paintAvatar(el, src, letter) {
  if (!el) return;
  if (src) {
    el.textContent = "";
    el.style.backgroundImage = `url("${String(src).replace(/["\\]/g, "")}")`;
    el.classList.add("has-photo");
  } else {
    el.textContent = letter || "?";
    el.style.backgroundImage = "";
    el.classList.remove("has-photo");
  }
}

function formatTime(epoch) {
  if (!epoch) return "—";
  return new Date(epoch * 1000).toLocaleString("vi-VN");
}

function formatScore(value) {
  if (value === undefined || value === null || value === "") return "—";
  const number = Number(value);
  if (!Number.isFinite(number)) return "—";
  return number.toLocaleString("vi-VN", { maximumFractionDigits: 2 });
}

function formatClock(seconds) {
  const total = Math.max(0, Math.ceil(seconds));
  const hour = Math.floor(total / 3600);
  const minute = Math.floor((total % 3600) / 60);
  const second = total % 60;
  const pad = (n) => String(n).padStart(2, "0");
  return hour > 0 ? `${pad(hour)}:${pad(minute)}:${pad(second)}` : `${pad(minute)}:${pad(second)}`;
}

function toEpoch(value) {
  const time = new Date(value).getTime();
  return Number.isFinite(time) ? Math.floor(time / 1000) : 0;
}

function toInputTime(epoch) {
  const date = new Date(epoch * 1000);
  const pad = (n) => String(n).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

function attemptLabel(status) {
  return {
    IN_PROGRESS: "Đang làm",
    SUBMITTING: "Đang nộp",
    GRADED: "Đã chấm",
    AUTO_SUBMITTED: "Tự động nộp",
    EXPIRED: "Hết hạn đồng bộ",
    SYNC_PENDING: "Chờ đồng bộ",
    OFFLINE: "Mất kết nối",
    NOT_STARTED: "Chưa thi"
  }[status] || status || "—";
}

function examLabel(status) {
  return { draft: "Nháp", published: "Đang mở", closed: "Đã đóng" }[status] || status;
}

function difficultyLabel(key) {
  return { easy: "Dễ", medium: "Trung bình", hard: "Khó" }[key] || key;
}

function actionLabel(action) {
  return {
    LOGIN: "Đăng nhập",
    LOGOUT: "Đăng xuất",
    SEED: "Khởi tạo dữ liệu",
    BACKUP: "Sao lưu",
    REGISTER: "Đăng ký",
    REQUEST_TEACHER: "Xin lên giáo viên",
    APPROVE_TEACHER: "Duyệt giáo viên",
    REJECT_TEACHER: "Từ chối giáo viên",
    CHANGE_PASSWORD: "Đổi mật khẩu",
    CREATE_PROFILE: "Tạo hồ sơ",
    UPDATE_PROFILE: "Sửa hồ sơ",
    LOCK: "Khóa tài khoản",
    UNLOCK: "Mở khóa",
    DISABLE: "Vô hiệu hồ sơ",
    ENABLE: "Mở hồ sơ",
    RESET_PASSWORD: "Đặt lại mật khẩu",
    DELETE_PROFILE: "Xóa hồ sơ",
    CREATE_DEPARTMENT: "Thêm bộ môn",
    CREATE_CLASS: "Thêm lớp",
    CREATE_SUBJECT: "Thêm môn",
    UPDATE_SUBJECT: "Sửa môn",
    CREATE_QUESTION: "Thêm câu hỏi",
    UPDATE_QUESTION: "Sửa câu hỏi",
    TOGGLE_QUESTION: "Ẩn/hiện câu hỏi",
    IMPORT_QUESTIONS: "Nhập câu hỏi",
    CREATE_EXAM: "Tạo kỳ thi",
    UPDATE_EXAM: "Sửa kỳ thi",
    UPDATE_SETTINGS: "Cập nhật cấu hình",
    UPDATE_BRAND: "Đổi nhận diện trường",
    RESTORE: "Khôi phục dữ liệu",
    PUBLISH_EXAM: "Mở kỳ thi",
    CLOSE_EXAM: "Đóng kỳ thi",
    DELETE_EXAM: "Xóa kỳ thi",
    START_ATTEMPT: "Bắt đầu bài thi",
    SUBMIT: "Nộp bài",
    AUTO_SUBMIT: "Tự nộp bài",
    RECONNECT: "Nối lại bài thi"
  }[action] || action || "—";
}

function toast(message, kind) {
  let box = document.getElementById("toasts");
  if (!box) {
    box = document.createElement("div");
    box.id = "toasts";
    document.body.appendChild(box);
  }
  const item = document.createElement("div");
  item.className = "toast" + (kind === "bad" ? " bad" : "");
  item.textContent = message;
  box.appendChild(item);
  setTimeout(() => item.remove(), 4200);
}

function shareLink(title, url, hint) {
  const body = openModal(title, `<p class="muted">${esc(hint || "Gửi link này cho người cần mở.")}</p>
    <label>Link<input id="share-url" readonly value="${esc(url)}"></label>
    <div class="row-actions"><button class="btn primary" id="share-copy" type="button">Sao chép link</button></div>`);
  const input = body.querySelector("#share-url");
  input.focus();
  input.select();
  body.querySelector("#share-copy").onclick = async () => {
    input.focus();
    input.select();
    let copied = false;
    try {
      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(url);
        copied = true;
      }
    } catch (error) {
      copied = false;
    }
    if (!copied) {
      try { copied = document.execCommand("copy"); } catch (error) { copied = false; }
    }
    toast(copied ? "Đã chép link" : "Hãy chọn ô link và chép thủ công", copied ? "" : "bad");
  };
}

function closeModal() {
  const wrap = document.querySelector(".modal-back");
  if (wrap && wrap._onclose) wrap._onclose();
  if (wrap) wrap.remove();
}

function openModal(title, html, options = {}) {
  closeModal();
  const wrap = document.createElement("div");
  wrap.className = "modal-back";
  wrap.innerHTML = `<div class="modal card" role="dialog">
      <header><h2>${esc(title)}</h2>${options.locked ? "" : '<button class="btn small" type="button" data-close>×</button>'}</header>
      <div class="modal-body">${html}</div>
    </div>`;
  wrap._onclose = options.onclose;
  document.body.appendChild(wrap);
  if (!options.locked) wrap.querySelector("[data-close]").onclick = () => closeModal();
  return wrap.querySelector(".modal-body");
}

function confirmBox(message) {
  return new Promise((resolve) => {
    let settled = false;
    const done = (value) => {
      if (settled) return;
      settled = true;
      closeModal();
      resolve(value);
    };
    const body = openModal("Xác nhận", `<p>${esc(message)}</p>
      <div class="row-actions"><button class="btn" id="no" type="button">Huỷ</button>
      <button class="btn primary" id="yes" type="button">Đồng ý</button></div>`, { onclose: () => done(false) });
    body.querySelector("#no").onclick = () => done(false);
    body.querySelector("#yes").onclick = () => done(true);
  });
}

function eyeButton() {
  return `<button class="eye" type="button" aria-label="Hiện mật khẩu" aria-pressed="false">
    <svg class="open" viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" d="M2.5 12S6 6.5 12 6.5 21.5 12 21.5 12 18 17.5 12 17.5 2.5 12 2.5 12z"/><circle cx="12" cy="12" r="2.6" fill="none" stroke="currentColor" stroke-width="1.8"/></svg>
    <svg class="shut" viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><path fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" d="M4 5l16 14M9.5 9.8A3 3 0 0 0 12 15a3 3 0 0 0 2.3-1.1M7 7.2C5.2 8.4 3.6 10.4 2.5 12c1.5 2.6 5 6 9.5 6 1.5 0 2.9-.4 4.1-1M10 5.4A12 12 0 0 1 12 5.2c6.5 0 9.5 6.8 9.5 6.8a16 16 0 0 1-2.8 3.4"/></svg>
  </button>`;
}

function bindSecretToggles(root) {
  (root || document).querySelectorAll(".secret").forEach((wrap) => {
    const input = wrap.querySelector("input");
    const button = wrap.querySelector(".eye");
    if (!input || !button || button.dataset.bound) return;
    button.dataset.bound = "1";
    button.onclick = () => {
      const show = input.type === "password";
      input.type = show ? "text" : "password";
      button.setAttribute("aria-label", show ? "Ẩn mật khẩu" : "Hiện mật khẩu");
      button.setAttribute("aria-pressed", show ? "true" : "false");
      wrap.classList.toggle("shown", show);
    };
  });
}

function openPassword(forced) {
  const body = openModal(forced ? "Bạn cần đổi mật khẩu trước khi tiếp tục" : "Đổi mật khẩu", `
    <form id="pw-form" class="stack">
      <label>Mật khẩu hiện tại<span class="secret"><input name="old" type="password" autocomplete="current-password" required>${eyeButton()}</span></label>
      <label>Mật khẩu mới<span class="secret"><input name="next" type="password" autocomplete="new-password" minlength="8" required>${eyeButton()}</span></label>
      <button class="btn primary" type="submit">Cập nhật</button>
    </form>`, { locked: forced });
  bindSecretToggles(body);
  body.querySelector("#pw-form").onsubmit = async (event) => {
    event.preventDefault();
    try {
      await API.post("/api/auth/change-password", {
        oldPassword: event.target.old.value,
        newPassword: event.target.next.value
      });
      toast("Đã đổi mật khẩu");
      location.reload();
    } catch (error) {
      toast(error.message, "bad");
    }
  };
}

async function guard(roles) {
  try {
    const me = await API.get("/api/auth/me");
    if (roles && !roles.includes(me.role)) {
      location.href = roleHome(me.role);
      return null;
    }
    return me;
  } catch (error) {
    if (!location.pathname.endsWith("/login.html")) location.href = "/login.html";
    return null;
  }
}

const BRAND_FALLBACK = {
  schoolName: "Phòng thi trực tuyến",
  schoolShort: "THI",
  level: "university",
  levelLabel: "Đại học",
  studentWord: "Sinh viên",
  teacherWord: "Giảng viên",
  motto: "Trộn câu hỏi, trộn đáp án, tính giờ theo máy chủ và vẫn giữ bài khi mất kết nối.",
  address: "",
  phone: "",
  email: "",
  website: "",
  theme: "navy",
  logo: ""
};

function loadBrand() {
  return API.get("/api/brand").catch(() => BRAND_FALLBACK);
}

function paintSeal(el, brand) {
  if (!el) return;
  if (brand.logo) {
    el.classList.add("has-logo");
    el.style.backgroundImage = `url("${String(brand.logo).replace(/["\\()]/g, "")}")`;
    el.textContent = "";
  } else {
    el.classList.remove("has-logo");
    el.style.backgroundImage = "";
    el.textContent = brand.schoolShort || "THI";
  }
}

function applyBrand(brand) {
  const data = brand && brand.schoolName ? brand : BRAND_FALLBACK;
  const themes = ["navy", "forest", "wine", "teal"];
  document.body.dataset.theme = themes.indexOf(data.theme) >= 0 ? data.theme : "navy";
  paintSeal(document.getElementById("brand-seal"), data);
  paintSeal(document.querySelector(".brand .seal"), data);
  const level = document.getElementById("brand-level");
  if (level) level.textContent = data.levelLabel || "";
  const name = document.getElementById("brand-name");
  if (name) name.textContent = data.schoolName;
  const motto = document.getElementById("brand-motto");
  if (motto) motto.textContent = data.motto || "";
  const school = document.getElementById("brand-school");
  if (school) school.textContent = data.schoolName;
  const foot = document.getElementById("brand-foot");
  if (foot) {
    const bits = [data.address, data.phone, data.email].filter(Boolean);
    foot.textContent = bits.length ? bits.join(" · ") : "Mẫu phòng thi giao được cho đại học, cao đẳng, phổ thông hoặc trung tâm.";
  }
  const sideName = document.querySelector(".brand strong");
  const sideNote = document.querySelector(".brand span");
  if (sideName) sideName.textContent = data.schoolName;
  if (sideNote) sideNote.textContent = data.levelLabel || "Phòng thi";
  const note = document.getElementById("brand-note");
  if (note) note.textContent = "Ai cũng tạo được tài khoản thường và vào ngay. Muốn lên giáo viên thì xác minh số CCCD, admin duyệt rồi quyền mới đổi.";
  const registerTitle = document.getElementById("register-title");
  if (registerTitle) registerTitle.textContent = "Tạo tài khoản";
  const registerLead = document.getElementById("register-lead");
  if (registerLead) registerLead.textContent = "Điền hồ sơ. Mật khẩu từ 8 ký tự, có cả chữ và số. Đây là tài khoản thường. Muốn lên giáo viên thì xác minh danh tính sau khi đăng nhập.";
  const codeCaption = document.getElementById("code-caption");
  if (codeCaption) codeCaption.textContent = "Mã " + String(data.studentWord || "học sinh").toLowerCase() + " (nếu có)";
  if (document.getElementById("login-form")) document.title = "Đăng nhập · " + data.schoolName;
  if (document.getElementById("register-form")) document.title = "Đăng ký · " + data.schoolName;
  return data;
}

function mount(options) {
  const me = options.me;
  const admin = me.role === "admin";
  const link = ([href, label]) => `<a class="${location.pathname === href ? "active" : ""}" href="${href}">${esc(label)}</a>`;
  const links = admin
    ? [["Điều hành", [
        ["/admin/dashboard.html", "Tổng quan"],
        ["/admin/brand.html", "Nhận diện trường"],
        ["/admin/settings.html", "Cấu hình"],
        ["/admin/audit.html", "Nhật ký"]
      ]], ["Học vụ", [
        ["/admin/profiles.html", "Hồ sơ"],
        ["/admin/subjects.html", "Môn học"],
        ["/lecturer/questions.html", "Ngân hàng câu hỏi"],
        ["/lecturer/exams.html", "Kỳ thi"],
        ["/lecturer/results.html", "Kết quả"]
      ]]].map(([title, items]) => `<p class="nav-label">${esc(title)}</p>${items.map(link).join("")}`).join("")
    : (NAV[me.role] || []).map(link).join("");
  document.body.innerHTML = `<a class="skip" href="#content">Tới nội dung</a>
    <div class="shell${admin ? " admin" : ""}">
    <aside class="sidebar">
      <div class="brand"><div class="seal" style="width:46px;height:46px;font-size:11px">THI</div>
        <div><strong>Phòng thi trực tuyến</strong><span>Đại học</span></div></div>
      <nav class="nav">${links}</nav>
      <div class="aside-user">
        <div class="aside-id">
          <span class="avatar small" id="side-avatar"></span>
          <div><div class="name">${esc(me.fullName)}</div>
          <div class="role">${esc(roleLabel(me.role))}${me.role === "admin" ? " · cao nhất" : ""} · ${esc(me.username)}</div></div>
        </div>
        <div class="row-actions">
          <button class="btn small" id="change-pw" type="button">Đổi mật khẩu</button>
          <button class="btn small" id="logout" type="button">Đăng xuất</button>
        </div>
      </div>
    </aside>
    <section class="workspace">
      <div class="topbar"><div><h1>${esc(options.title)}</h1>${options.lead ? `<p class="page-lead">${esc(options.lead)}</p>` : ""}</div><div id="top-actions" class="row-actions"></div></div>
      <div id="content"></div>
    </section>
  </div>`;
  paintAvatar(document.getElementById("side-avatar"), me.avatar, (me.fullName || "?").slice(0, 1));
  loadBrand().then(applyBrand);
  document.getElementById("logout").onclick = async () => {
    await API.post("/api/auth/logout", {});
    location.href = "/login.html";
  };
  document.getElementById("change-pw").onclick = () => openPassword(false);
  if (me.mustChangePassword) openPassword(true);
  return { content: document.getElementById("content"), actions: document.getElementById("top-actions"), me };
}
