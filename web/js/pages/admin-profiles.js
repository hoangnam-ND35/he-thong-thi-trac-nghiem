(async function () {
  const me = await guard(["admin", "lecturer"]);
  if (!me) return;
  const isAdmin = me.role === "admin";
  const ui = mount({
    title: isAdmin ? "Hồ sơ và phân quyền" : "Học sinh",
    lead: isAdmin ? "Admin đứng trên giáo viên và học sinh. Giáo viên do admin tạo. Học sinh có thể tự đăng ký." : "Giáo viên chỉ xem học sinh cùng khoa. Không tạo được giáo viên hay admin.",
    me
  });
  if (isAdmin) {
    const add = document.createElement("button");
    add.className = "btn primary";
    add.textContent = "Thêm hồ sơ";
    add.onclick = () => openForm(null);
    ui.actions.appendChild(add);
  }

  function badge(row) {
    if (row.accountStatus !== "active") return `<span class="badge bad">Đã khóa</span>`;
    if (row.status !== "active") return `<span class="badge warn">Vô hiệu</span>`;
    return `<span class="badge ok">Hoạt động</span>`;
  }

  function permissionBoard() {
    if (!isAdmin) return "";
    return `<section class="card panel">
      <h2>Phân quyền</h2>
      <p class="muted">Học sinh làm bài. Giáo viên soạn môn của mình. Admin làm được mọi việc của hai vai dưới và là quyền cao nhất.</p>
      <div class="table-wrap"><table class="data">
        <thead><tr><th>Việc được làm</th><th>Học sinh</th><th>Giáo viên</th><th>Admin</th></tr></thead>
        <tbody>
          <tr><td>Tự đăng ký và làm bài thi</td><td>Có</td><td>Không</td><td>Không</td></tr>
          <tr><td>Soạn câu hỏi, mở kỳ thi, xem kết quả</td><td>Không</td><td>Môn của mình</td><td>Mọi môn</td></tr>
          <tr><td>Xem danh sách học sinh</td><td>Không</td><td>Cùng khoa</td><td>Mọi khoa</td></tr>
          <tr><td>Tạo giáo viên hoặc admin</td><td>Không</td><td>Không</td><td>Có</td></tr>
          <tr><td>Xin lên giáo viên bằng CCCD</td><td>Gửi đơn</td><td>Không</td><td>Duyệt hoặc từ chối</td></tr>
          <tr><td>Khóa tài khoản, cấu hình, nhật ký, nhận diện trường</td><td>Không</td><td>Không</td><td>Có</td></tr>
        </tbody>
      </table></div>
    </section>`;
  }

  function paintShell() {
    ui.content.innerHTML = `
      ${permissionBoard()}
      ${isAdmin ? `<section class="card panel" id="upgrade-box"><h2>Đơn xin lên giáo viên</h2><p class="muted">Đang tải đơn...</p></section>` : ""}
      <div class="toolbar">
        <div class="filters" style="margin:0">
          <input id="q" placeholder="Tìm tên, mã, email" aria-label="Tìm hồ sơ">
          ${isAdmin ? `<select id="role" aria-label="Vai trò"><option value="">Mọi vai trò</option><option value="admin">Admin</option><option value="lecturer">Giáo viên</option><option value="student">Học sinh</option></select>
          <select id="state" aria-label="Trạng thái"><option value="">Mọi trạng thái</option><option value="active">Hoạt động</option><option value="locked">Đã khóa</option><option value="disabled">Vô hiệu</option></select>` : ""}
          <input id="faculty" placeholder="Khoa" aria-label="Khoa">
          <input id="className" placeholder="Lớp" aria-label="Lớp">
        </div>
        <p class="count-line" id="profile-count">Đang tải...</p>
      </div>
      <div class="card panel table-wrap"><table class="data">
        <thead><tr><th>Tài khoản</th><th>Họ tên</th><th>Lớp / bộ môn</th><th>Liên hệ</th><th>Trạng thái</th><th></th></tr></thead>
        <tbody id="profile-rows"><tr><td colspan="6">Đang tải hồ sơ...</td></tr></tbody>
      </table></div>`;
    let timer = 0;
    const refresh = () => load().catch((error) => toast(error.message, "bad"));
    ui.content.querySelectorAll("input").forEach((input) => {
      input.oninput = () => {
        clearTimeout(timer);
        timer = setTimeout(refresh, 250);
      };
    });
    ui.content.querySelectorAll("select").forEach((select) => { select.onchange = refresh; });
  }

  function upgradeStatus(row) {
    return `<tr>
      <td>${esc(row.username)}<div class="muted">${esc(row.fullName)}</div></td>
      <td>${esc(row.cccd)}</td>
      <td>${esc(row.lecturerCode)}<div class="muted">${esc(row.department)} · ${esc(row.faculty)}</div></td>
      <td><button class="btn small" data-approve="${row.id}" type="button">Duyệt</button>
        <button class="btn small" data-reject="${row.id}" type="button">Từ chối</button></td>
    </tr>`;
  }

  async function loadUpgrades() {
    const box = document.getElementById("upgrade-box");
    if (!box) return;
    const rows = await API.get("/api/admin/teacher-upgrades");
    box.innerHTML = `<h2>Đơn xin lên giáo viên</h2>
      <p class="muted">Số CCCD chỉ hiện ở đây để đối chiếu. Học sinh chỉ thấy số đã che. Duyệt xong tài khoản thành giáo viên, lịch sử thi cũ vẫn giữ.</p>
      <div class="table-wrap"><table class="data">
        <thead><tr><th>Tài khoản</th><th>CCCD</th><th>Mã giáo viên</th><th></th></tr></thead>
        <tbody>${rows.map(upgradeStatus).join("") || "<tr><td colspan='4'>Chưa có đơn chờ duyệt</td></tr>"}</tbody>
      </table></div>`;
    box.querySelectorAll("[data-approve]").forEach((button) => {
      button.onclick = () => decide(Number(button.dataset.approve), true);
    });
    box.querySelectorAll("[data-reject]").forEach((button) => {
      button.onclick = () => decide(Number(button.dataset.reject), false);
    });
  }

  async function decide(id, approve) {
    try {
      if (approve) {
        if (!(await confirmBox("Duyệt đơn này và đổi tài khoản thành giáo viên?"))) return;
        await API.post("/api/admin/teacher-upgrades/" + id + "/approve", {});
        toast("Đã duyệt. Người dùng tải lại trang để vào quyền giáo viên");
      } else {
        const body = openModal("Từ chối đơn", `<form id="reject-form" class="stack">
          <label>Lý do<textarea name="note" required></textarea></label>
          <button class="btn primary" type="submit">Từ chối</button>
        </form>`);
        const form = body.querySelector("#reject-form");
        form.onsubmit = async (event) => {
          event.preventDefault();
          try {
            await API.post("/api/admin/teacher-upgrades/" + id + "/reject", { note: form.note.value.trim() });
            closeModal();
            toast("Đã từ chối đơn");
            await loadUpgrades();
          } catch (error) {
            toast(error.message, "bad");
          }
        };
        return;
      }
      await loadUpgrades();
      await load();
    } catch (error) {
      toast(error.message, "bad");
    }
  }

  async function load() {
    const params = new URLSearchParams();
    const q = document.getElementById("q")?.value.trim() || "";
    const role = document.getElementById("role")?.value || "";
    const faculty = document.getElementById("faculty")?.value.trim() || "";
    const className = document.getElementById("className")?.value.trim() || "";
    const state = document.getElementById("state")?.value || "";
    if (q) params.set("q", q);
    if (role) params.set("role", role);
    if (faculty) params.set("faculty", faculty);
    if (className) params.set("className", className);
    let rows = await API.get("/api/profiles" + (params.toString() ? "?" + params : ""));
    if (state === "active") rows = rows.filter((row) => row.accountStatus === "active" && row.status === "active");
    if (state === "locked") rows = rows.filter((row) => row.accountStatus !== "active");
    if (state === "disabled") rows = rows.filter((row) => row.status !== "active");
    const body = document.getElementById("profile-rows");
    const count = document.getElementById("profile-count");
    if (!body) return;
    if (count) count.textContent = rows.length + " hồ sơ";
    body.innerHTML = rows.map((row) => `<tr>
      <td>${esc(row.username)}<div class="muted">${esc(row.role === "lecturer" ? (row.lecturerCode || "") : (row.studentCode || ""))}</div></td>
      <td>${esc(row.fullName)}<div class="muted">${esc(roleLabel(row.role))}${row.role === "admin" ? " · cao nhất" : ""}</div></td>
      <td>${esc(row.role === "lecturer" ? (row.department || "—") : (row.className || "—"))}<div class="muted">${esc(row.faculty || "")}</div></td>
      <td>${esc(row.email)}<div class="muted">${esc(row.phone || "")}</div></td>
      <td>${badge(row)}</td>
      <td><button class="btn small" data-open="${row.profileId}" type="button">Thao tác</button></td>
    </tr>`).join("") || "<tr><td colspan='6'>Không có hồ sơ phù hợp</td></tr>";
    body.querySelectorAll("[data-open]").forEach((button) => {
      button.onclick = () => openActions(rows.find((row) => row.profileId === Number(button.dataset.open)));
    });
  }

  function openActions(row) {
    if (!row) return;
    const body = openModal(row.fullName, `<p class="muted">${esc(roleLabel(row.role))} · ${esc(row.username)}</p>
      <div class="stack">
        ${row.studentId ? `<button class="btn" id="act-history" type="button">Lịch sử thi</button>` : ""}
        ${isAdmin ? `<button class="btn" id="act-edit" type="button">Sửa hồ sơ</button>
          <button class="btn" id="act-reset" type="button">Đặt lại mật khẩu</button>
          <button class="btn" id="act-lock" type="button">${row.accountStatus === "active" ? "Khóa tài khoản" : "Mở khóa tài khoản"}</button>
          <button class="btn" id="act-disable" type="button">${row.status === "active" ? "Vô hiệu hồ sơ" : "Mở lại hồ sơ"}</button>` : ""}
      </div>`);
    const go = (id, fn) => {
      const button = body.querySelector(id);
      if (button) button.onclick = fn;
    };
    go("#act-history", () => showHistory(row.studentId));
    go("#act-edit", () => openForm(row));
    go("#act-reset", () => resetPassword(row.profileId));
    go("#act-lock", () => account(row.profileId, row.accountStatus === "active" ? "lock" : "unlock"));
    go("#act-disable", () => account(row.profileId, row.status === "active" ? "disable" : "enable"));
  }

  function formFields(row) {
    const role = row?.role || "student";
    return `<form id="profile-form" class="stack">
      ${row ? "" : `<label>Vai trò<select name="role"><option value="student">Học sinh</option><option value="lecturer">Giáo viên</option><option value="admin">Admin</option></select></label>
      <label>Tên đăng nhập<input name="username" required></label>
      <label>Mật khẩu<input name="password" type="password" minlength="8" required></label>`}
      <label>Họ tên<input name="fullName" value="${esc(row?.fullName || "")}" required></label>
      <label>Email<input name="email" value="${esc(row?.email || "")}" required></label>
      <label>Số điện thoại<input name="phone" value="${esc(row?.phone || "")}"></label>
      <div id="role-fields"></div>
      <button class="btn primary" type="submit">Lưu</button>
    </form>`;
  }

  function roleFields(role, row) {
    if (role === "student") {
      return `<label>Mã học sinh<input name="studentCode" value="${esc(row?.studentCode || "")}" required></label>
        <label>Ngày sinh<input name="dob" type="date" value="${esc(row?.dob || "")}"></label>
        <label>Giới tính<select name="gender"><option ${row?.gender === "Nam" ? "selected" : ""}>Nam</option><option ${row?.gender === "Nữ" ? "selected" : ""}>Nữ</option><option ${row?.gender === "Khác" ? "selected" : ""}>Khác</option></select></label>
        <label>Lớp<input name="className" value="${esc(row?.className || "")}" required></label>
        <label>Khoa<input name="faculty" value="${esc(row?.faculty || "")}" placeholder="Tên khoa" required></label>`;
    }
    if (role === "lecturer") {
      return `<label>Mã giáo viên<input name="lecturerCode" value="${esc(row?.lecturerCode || "")}" required></label>
        <label>Bộ môn<input name="department" value="${esc(row?.department || "")}" required></label>
        <label>Khoa<input name="faculty" value="${esc(row?.faculty || "")}" placeholder="Tên khoa" required></label>`;
    }
    return "";
  }

  function openForm(row) {
    const body = openModal(row ? "Sửa hồ sơ" : "Thêm hồ sơ", formFields(row));
    const form = body.querySelector("#profile-form");
    const extra = body.querySelector("#role-fields");
    const paint = () => { extra.innerHTML = roleFields(row?.role || form.role?.value || "student", row); };
    paint();
    if (form.role) form.role.onchange = paint;
    form.onsubmit = async (event) => {
      event.preventDefault();
      const data = Object.fromEntries(new FormData(form).entries());
      try {
        if (row) await API.put("/api/profiles/" + row.profileId, data);
        else await API.post("/api/profiles", data);
        closeModal();
        toast("Đã lưu hồ sơ");
        await load();
      } catch (error) {
        toast(error.message, "bad");
      }
    };
  }

  async function account(id, mode) {
    if (!(await confirmBox("Thay đổi trạng thái tài khoản này?"))) return;
    try {
      await API.post(`/api/profiles/${id}/${mode}`, {});
      toast("Đã cập nhật tài khoản");
      await load();
    } catch (error) {
      toast(error.message, "bad");
    }
  }

  async function resetPassword(id) {
    if (!(await confirmBox("Đặt lại mật khẩu và buộc đổi ở lần đăng nhập sau?"))) return;
    try {
      const data = await API.post("/api/profiles/" + id + "/reset-password", {});
      openModal("Mật khẩu tạm", `<p>Đưa mật khẩu này cho người dùng:</p><p><strong>${esc(data.temporaryPassword)}</strong></p>`);
      await load();
    } catch (error) {
      toast(error.message, "bad");
    }
  }

  async function showHistory(studentId) {
    try {
      const rows = await API.get("/api/students/" + studentId + "/history");
      openModal("Lịch sử thi", `<div class="table-wrap"><table class="data"><thead><tr><th>Kỳ thi</th><th>Trạng thái</th><th>Điểm</th><th>Mã đề</th></tr></thead><tbody>
        ${rows.map((row) => `<tr><td>${esc(row.examTitle)}</td><td>${esc(attemptLabel(row.status))}</td><td>${formatScore(row.score)}/${formatScore(row.total)}</td><td>${esc(row.paperCode || "—")}</td></tr>`).join("") || "<tr><td colspan='4'>Chưa có bài thi</td></tr>"}
      </tbody></table></div>`);
    } catch (error) {
      toast(error.message, "bad");
    }
  }

  paintShell();
  load().catch((error) => { ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`; });
  if (isAdmin) loadUpgrades().catch((error) => toast(error.message, "bad"));
})();
