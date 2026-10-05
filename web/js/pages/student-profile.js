(async function () {
  const me = await guard(["student", "lecturer", "admin"]);
  if (!me) return;
  const isStudent = me.role === "student";
  const ui = mount({
    title: "Thông tin tài khoản",
    lead: isStudent
      ? "Xem tài khoản, lớp và lịch sử thi. Số điện thoại và ảnh đại diện có thể tự cập nhật."
      : "Xem tài khoản và nơi công tác. Số điện thoại và ảnh đại diện có thể tự cập nhật.",
    me
  });
  const letter = (me.fullName || "?").slice(0, 1);
  const accountState = me.accountStatus && me.accountStatus !== "active"
    ? `<span class="badge bad">Đã khóa</span>`
    : me.status !== "active"
      ? `<span class="badge warn">Vô hiệu</span>`
      : `<span class="badge ok">Hoạt động</span>`;
  const born = me.dob ? new Date(me.dob + "T00:00:00").toLocaleDateString("vi-VN") : "—";
  const academic = isStudent
    ? `<section class="card panel">
        <h2>Thông tin học vụ</h2>
        <dl class="facts">
          <dt>Họ và tên</dt><dd>${esc(me.fullName)}</dd>
          <dt>Mã học sinh</dt><dd>${esc(me.studentCode || "—")}</dd>
          <dt>Ngày sinh</dt><dd>${esc(born)}</dd>
          <dt>Giới tính</dt><dd>${esc(me.gender || "—")}</dd>
          <dt>Lớp</dt><dd>${esc(me.className || "—")}</dd>
          <dt>Khoa</dt><dd>${esc(me.faculty || "—")}</dd>
        </dl>
      </section>`
    : `<section class="card panel">
        <h2>Thông tin công tác</h2>
        <dl class="facts">
          <dt>Họ và tên</dt><dd>${esc(me.fullName)}</dd>
          <dt>Mã giáo viên</dt><dd>${esc(me.lecturerCode || "—")}</dd>
          <dt>Bộ môn</dt><dd>${esc(me.department || "—")}</dd>
          <dt>Khoa</dt><dd>${esc(me.faculty || "—")}</dd>
          <dt>Email</dt><dd>${esc(me.email || "—")}</dd>
        </dl>
      </section>`;

  ui.content.innerHTML = `
    <div class="grid-2">
      <section class="card panel">
        <div class="profile-head">
          <button class="avatar-hit" id="pick-avatar" type="button" aria-label="Chọn ảnh làm nền đại diện">
            <span class="avatar large" id="avatar-face"></span>
          </button>
          <div>
            <h2 style="margin:0">${esc(me.fullName)}</h2>
            <p class="muted">${esc(roleLabel(me.role))} · ${esc(me.username)}</p>
            <div class="row-actions">
              <button class="btn small" id="choose-photo" type="button">Chọn ảnh</button>
              <button class="btn small" id="clear-photo" type="button">Dùng chữ cái</button>
            </div>
            <p class="muted" id="avatar-note">Ảnh được cắt vuông và phủ làm nền vòng đại diện.</p>
          </div>
        </div>
        <input id="avatar-file" type="file" accept="image/*" hidden>
        <h2>Thông tin tài khoản</h2>
        <dl class="facts">
          <dt>Tên đăng nhập</dt><dd>${esc(me.username)}</dd>
          <dt>Vai trò</dt><dd>${esc(roleLabel(me.role))}</dd>
          <dt>Email</dt><dd>${esc(me.email || "—")}</dd>
          <dt>Trạng thái</dt><dd>${accountState}</dd>
          <dt>Mật khẩu</dt><dd><button class="btn small" id="open-password" type="button">Đổi mật khẩu</button></dd>
        </dl>
        ${me.mustChangePassword ? `<p class="note">Bạn cần đổi mật khẩu trước khi tiếp tục dùng hệ thống.</p>` : ""}
        <form id="phone-form" class="stack" style="margin-top:14px">
          <label>Số điện thoại<input name="phone" value="${esc(me.phone || "")}"></label>
          <button class="btn primary" type="submit">Lưu số điện thoại</button>
        </form>
      </section>
      ${academic}
    </div>
    ${isStudent ? `<section class="card panel" id="upgrade"><h2>Nâng cấp giáo viên</h2><p class="muted">Đang tải đơn...</p></section>
    <section class="card panel" id="history"><h2>Lịch sử thi</h2></section>` : ""}`;

  const face = document.getElementById("avatar-face");
  const fileInput = document.getElementById("avatar-file");
  const clearButton = document.getElementById("clear-photo");
  let currentAvatar = me.avatar || "";
  paintAvatar(face, currentAvatar, letter);
  clearButton.hidden = !currentAvatar;

  function showAvatar(src) {
    currentAvatar = src || "";
    paintAvatar(face, currentAvatar, letter);
    paintAvatar(document.getElementById("side-avatar"), currentAvatar, letter);
    clearButton.hidden = !currentAvatar;
  }

  async function saveAvatar(src) {
    const previous = currentAvatar;
    showAvatar(src);
    clearButton.disabled = true;
    try {
      const data = await API.post("/api/profiles/me/avatar", { avatar: src || "" });
      showAvatar(data.avatar || "");
      toast(src ? "Đã dùng ảnh làm nền đại diện" : "Đã trở lại chữ cái");
    } catch (error) {
      showAvatar(previous);
      toast(error.message, "bad");
    }
    clearButton.disabled = false;
  }

  const openPicker = () => fileInput.click();
  document.getElementById("pick-avatar").onclick = openPicker;
  document.getElementById("choose-photo").onclick = openPicker;
  clearButton.onclick = () => saveAvatar("");
  fileInput.onchange = async () => {
    const file = fileInput.files && fileInput.files[0];
    fileInput.value = "";
    if (!file) return;
    if (!file.type.startsWith("image/")) {
      toast("Hãy chọn một file ảnh", "bad");
      return;
    }
    try {
      const avatarData = await resizeImage(file);
      await saveAvatar(avatarData);
    } catch (error) {
      toast(error.message, "bad");
    }
  };

  document.getElementById("open-password").onclick = () => openPassword(false);
  document.getElementById("phone-form").onsubmit = async (event) => {
    event.preventDefault();
    const button = event.target.querySelector("button");
    button.disabled = true;
    try {
      await API.put("/api/profiles/me", { phone: event.target.phone.value });
      toast("Đã cập nhật số điện thoại");
    } catch (error) {
      toast(error.message, "bad");
    }
    button.disabled = false;
  };

  function upgradeForm(row) {
    const pending = row && row.status === "pending";
    const rejected = row && row.status === "rejected";
    return `<p class="muted">Hệ thống kiểm tra số CCCD đủ 12 số, mã tỉnh, năm sinh và giới tính khớp hồ sơ, họ tên trùng tài khoản. Admin xác nhận trước khi quyền đổi thành giáo viên. Không có kết nối cơ sở CCCD nhà nước.</p>
      ${pending ? `<p class="note">Đơn đang chờ duyệt. CCCD đã che: ${esc(row.cccd)}. Mã giáo viên ${esc(row.lecturerCode)} · ${esc(row.department)} · ${esc(row.faculty)}.</p>` : ""}
      ${rejected ? `<p class="note bad">Đơn bị từ chối: ${esc(row.note || "Không có lý do")}. Bạn có thể gửi lại.</p>` : ""}
      <form id="upgrade-form" class="stack">
        <label>Số CCCD<input name="cccd" inputmode="numeric" maxlength="12" required placeholder="12 chữ số"></label>
        <label>Họ tên trên CCCD<input name="fullName" value="${esc(me.fullName)}" readonly></label>
        <label>Mã giáo viên muốn dùng<input name="lecturerCode" value="${esc(row?.lecturerCode || "")}" required></label>
        <label>Bộ môn<input name="department" value="${esc(row?.department || "")}" required></label>
        <label>Khoa<input name="faculty" value="${esc(row?.faculty || me.faculty || "")}" required></label>
        <button class="btn primary" type="submit">${pending ? "Gửi lại đơn" : "Gửi đơn xác minh"}</button>
      </form>`;
  }

  async function paintUpgrade() {
    const box = document.getElementById("upgrade");
    if (!box) return;
    try {
      const fresh = await API.get("/api/auth/me");
      if (fresh.role === "lecturer") {
        box.innerHTML = `<h2>Nâng cấp giáo viên</h2><p class="note">Đơn đã được duyệt. Tài khoản của bạn là giáo viên.</p><a class="btn primary" href="${esc(roleHome("lecturer"))}">Vào trang giáo viên</a>`;
        return;
      }
      const row = await API.get("/api/profiles/me/teacher-upgrade");
      box.innerHTML = `<h2>Nâng cấp giáo viên</h2>${upgradeForm(row.status ? row : null)}`;
      box.querySelector("#upgrade-form").onsubmit = async (event) => {
        event.preventDefault();
        const button = event.target.querySelector("button");
        const data = Object.fromEntries(new FormData(event.target).entries());
        button.disabled = true;
        try {
          await API.post("/api/profiles/me/teacher-upgrade", data);
          toast("Đã gửi đơn. Chờ admin duyệt");
          await paintUpgrade();
        } catch (error) {
          toast(error.message, "bad");
          button.disabled = false;
        }
      };
    } catch (error) {
      box.innerHTML = `<h2>Nâng cấp giáo viên</h2><p class="note bad">${esc(error.message)}</p>`;
    }
  }

  if (isStudent) {
    paintUpgrade();
    try {
      const rows = await API.get("/api/students/" + me.studentId + "/history");
      document.getElementById("history").insertAdjacentHTML("beforeend", `<div class="table-wrap"><table class="data"><thead><tr><th>Kỳ thi</th><th>Trạng thái</th><th>Điểm</th><th>Mã đề</th><th></th></tr></thead><tbody>
        ${rows.map((row) => `<tr><td>${esc(row.examTitle)}</td><td>${esc(attemptLabel(row.status))}</td><td>${formatScore(row.score)}/${formatScore(row.total)}</td><td>${esc(row.paperCode || "—")}</td><td>${row.submitTime ? `<a href="/exam/result.html?attemptId=${row.attemptId}">Xem</a>` : ""}</td></tr>`).join("") || "<tr><td colspan='5'>Chưa có bài thi</td></tr>"}
      </tbody></table></div>`);
    } catch (error) {
      document.getElementById("history").insertAdjacentHTML("beforeend", `<p class="note bad">${esc(error.message)}</p>`);
    }
  }

  function resizeImage(file) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onerror = () => reject(new Error("Không đọc được ảnh"));
      reader.onload = () => {
        const image = new Image();
        image.onload = () => {
          const size = 128;
          const canvas = document.createElement("canvas");
          canvas.width = size;
          canvas.height = size;
          const context = canvas.getContext("2d");
          const scale = Math.max(size / image.width, size / image.height);
          const width = image.width * scale;
          const height = image.height * scale;
          context.drawImage(image, (size - width) / 2, (size - height) / 2, width, height);
          resolve(canvas.toDataURL("image/jpeg", 0.72));
        };
        image.onerror = () => reject(new Error("Không đọc được ảnh"));
        image.src = reader.result;
      };
      reader.readAsDataURL(file);
    });
  }
})();
