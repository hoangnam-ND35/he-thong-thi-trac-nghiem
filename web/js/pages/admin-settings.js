(async function () {
  const me = await guard(["admin"]);
  if (!me) return;
  const ui = mount({
    title: "Cấu hình hệ thống",
    lead: "Giá trị mặc định khi tạo kỳ thi, chu kỳ lưu bài và khôi phục dữ liệu.",
    me
  });
  try {
    const [settings, backups] = await Promise.all([
      API.get("/api/admin/settings"),
      API.get("/api/admin/backups")
    ]);
    ui.content.innerHTML = `
      <div class="grid-2">
        <section class="card panel">
          <h2>Chính sách mặc định</h2>
          <form id="settings-form" class="stack">
            <label>Thời lượng mặc định (phút)<input name="defaultDuration" type="number" min="1" max="300" value="${settings.defaultDuration}" required></label>
            <div class="filters">
              <label>Dễ<input name="defaultEasy" type="number" min="0" value="${settings.defaultEasy}"></label>
              <label>Trung bình<input name="defaultMedium" type="number" min="0" value="${settings.defaultMedium}"></label>
              <label>Khó<input name="defaultHard" type="number" min="0" value="${settings.defaultHard}"></label>
            </div>
            <label>Lưu bài định kỳ (giây)<input name="autosaveSec" type="number" min="5" max="30" value="${settings.autosaveSec}" required></label>
            <label>Cảnh báo mất mạng ngắn (giây)<input name="shortDisconnect" type="number" min="5" max="180" value="${settings.shortDisconnect}" required></label>
            <label>Cảnh báo mất mạng dài (giây)<input name="longDisconnect" type="number" min="6" max="1800" value="${settings.longDisconnect}" required></label>
            <label>Ân hạn nộp sau hết giờ (giây)<input name="syncGrace" type="number" min="30" max="900" value="${settings.syncGrace}" required></label>
            <p class="muted">Thời gian thi vẫn tính theo máy chủ và không được dừng khi mất mạng.</p>
            <button class="btn primary" type="submit">Lưu cấu hình</button>
          </form>
        </section>
        <section class="card panel">
          <h2>Khôi phục dữ liệu</h2>
          <p class="muted">Chọn một bản sao lưu. Dữ liệu hiện tại sẽ được thay bằng bản đó.</p>
          <form id="restore-form" class="stack">
            <label>Bản sao lưu<select name="name">${(backups || []).map((row) => `<option value="${esc(row.name)}">${esc(row.name)}</option>`).join("") || "<option value=''>Chưa có bản sao lưu</option>"}</select></label>
            <button class="btn" type="submit">Khôi phục</button>
          </form>
        </section>
      </div>`;
    document.getElementById("settings-form").onsubmit = async (event) => {
      event.preventDefault();
      const form = event.target;
      const button = form.querySelector("button");
      button.disabled = true;
      try {
        await API.put("/api/admin/settings", {
          defaultDuration: Number(form.defaultDuration.value),
          defaultEasy: Number(form.defaultEasy.value),
          defaultMedium: Number(form.defaultMedium.value),
          defaultHard: Number(form.defaultHard.value),
          autosaveSec: Number(form.autosaveSec.value),
          shortDisconnect: Number(form.shortDisconnect.value),
          longDisconnect: Number(form.longDisconnect.value),
          syncGrace: Number(form.syncGrace.value)
        });
        toast("Đã lưu cấu hình");
      } catch (error) {
        toast(error.message, "bad");
      }
      button.disabled = false;
    };
    document.getElementById("restore-form").onsubmit = async (event) => {
      event.preventDefault();
      const name = event.target.name.value;
      if (!name) return;
      if (!(await confirmBox("Khôi phục bản " + name + "? Dữ liệu hiện tại sẽ bị thay."))) return;
      try {
        await API.post("/api/admin/restore", { name });
        toast("Đã khôi phục dữ liệu");
        location.href = "/admin/dashboard.html";
      } catch (error) {
        toast(error.message, "bad");
      }
    };
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }
})();
