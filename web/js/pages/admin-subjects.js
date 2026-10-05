(async function () {
  const me = await guard(["admin", "lecturer"]);
  if (!me) return;
  const isAdmin = me.role === "admin";
  const ui = mount({
    title: "Môn học",
    lead: isAdmin ? "Khoa, bộ môn, lớp và môn học giao cho giáo viên. Chỉ admin tạo được khoa và lớp." : "Giáo viên chỉ thấy môn mình phụ trách.",
    me
  });
  let departments = [];
  let classes = [];
  let subjects = [];
  let lecturers = [];

  async function load() {
    [departments, classes, subjects] = await Promise.all([
      API.get("/api/departments"),
      API.get("/api/classes"),
      API.get("/api/subjects")
    ]);
    if (isAdmin) {
      const profiles = await API.get("/api/profiles?role=lecturer");
      lecturers = profiles;
    }
    const query = (document.getElementById("subject-q")?.value || "").trim().toLowerCase();
    const shown = subjects.filter((row) => !query || `${row.code} ${row.name} ${row.lecturerName}`.toLowerCase().includes(query));
    ui.content.innerHTML = `
      ${isAdmin ? `<div class="stats">
        <article class="card stat"><span>Bộ môn</span><b>${departments.length}</b></article>
        <article class="card stat"><span>Lớp</span><b>${classes.length}</b></article>
        <article class="card stat"><span>Môn học</span><b>${subjects.length}</b></article>
        <article class="card stat"><span>Giáo viên</span><b>${lecturers.length}</b></article>
      </div>
      <div class="grid-2">
        <section class="card panel"><h2>Bộ môn</h2><form id="dep-form" class="stack">
          <label>Tên bộ môn<input name="name" required></label>
          <label>Khoa<input name="faculty" placeholder="Tên khoa" required></label>
          <button class="btn" type="submit">Thêm bộ môn</button>
        </form>
        <div class="table-wrap"><table class="data"><tbody>${departments.map((row) => `<tr><td>${esc(row.name)}</td><td class="muted">${esc(row.faculty)}</td></tr>`).join("") || "<tr><td>Chưa có bộ môn</td></tr>"}</tbody></table></div></section>
        <section class="card panel"><h2>Lớp</h2><form id="class-form" class="stack">
          <label>Tên lớp<input name="name" required></label>
          <label>Khoa<input name="faculty" placeholder="Tên khoa" required></label>
          <label>Bộ môn<select name="departmentId">${departments.map((row) => `<option value="${row.id}">${esc(row.name)}</option>`).join("")}</select></label>
          <button class="btn" type="submit">Thêm lớp</button>
        </form>
        <div class="table-wrap"><table class="data"><tbody>${classes.map((row) => `<tr><td>${esc(row.name)}</td><td class="muted">${esc(row.departmentName)} · ${esc(row.faculty)}</td></tr>`).join("") || "<tr><td>Chưa có lớp</td></tr>"}</tbody></table></div></section>
      </div>` : ""}
      <section class="card panel"><div class="toolbar"><h2 style="margin:0">Danh sách môn</h2>
        <div class="filters" style="margin:0"><input id="subject-q" value="${esc(query)}" placeholder="Tìm mã, tên, giáo viên" aria-label="Tìm môn học"><button class="btn primary" id="add-subject" type="button">Thêm môn học</button></div></div>
        <div class="table-wrap"><table class="data"><thead><tr><th>Mã</th><th>Tên</th><th>Tín chỉ</th><th>Giáo viên</th><th>Bộ môn</th><th></th></tr></thead>
        <tbody id="subject-rows">${shown.map((row) => `<tr><td>${esc(row.code)}</td><td>${esc(row.name)}<div class="muted">${esc(row.description || "")}</div></td><td>${row.credits}</td><td>${esc(row.lecturerName)}</td><td>${esc(row.departmentName)}</td>
          <td>${row.canEdit ? `<button class="btn small" data-edit="${row.id}">Sửa</button>` : ""}</td></tr>`).join("") || "<tr><td colspan='6'>Không có môn phù hợp</td></tr>"}</tbody></table></div>
        <p class="count-line">${shown.length} / ${subjects.length} môn</p>
      </section>`;
    const dep = document.getElementById("dep-form");
    if (dep) dep.onsubmit = async (event) => {
      event.preventDefault();
      try {
        await API.post("/api/departments", Object.fromEntries(new FormData(dep).entries()));
        toast("Đã thêm bộ môn");
        await load();
      } catch (error) { toast(error.message, "bad"); }
    };
    const room = document.getElementById("class-form");
    if (room) room.onsubmit = async (event) => {
      event.preventDefault();
      const data = Object.fromEntries(new FormData(room).entries());
      data.departmentId = Number(data.departmentId);
      try {
        await API.post("/api/classes", data);
        toast("Đã thêm lớp");
        await load();
      } catch (error) { toast(error.message, "bad"); }
    };
    const finder = document.getElementById("subject-q");
    if (finder) finder.oninput = () => {
      const query = finder.value.trim().toLowerCase();
      const shown = subjects.filter((row) => !query || `${row.code} ${row.name} ${row.lecturerName}`.toLowerCase().includes(query));
      const body = document.getElementById("subject-rows");
      const count = ui.content.querySelector("#subject-rows")?.closest("section")?.querySelector(".count-line");
      if (body) body.innerHTML = shown.map((row) => `<tr><td>${esc(row.code)}</td><td>${esc(row.name)}<div class="muted">${esc(row.description || "")}</div></td><td>${row.credits}</td><td>${esc(row.lecturerName)}</td><td>${esc(row.departmentName)}</td>
          <td>${row.canEdit ? `<button class="btn small" data-edit="${row.id}">Sửa</button>` : ""}</td></tr>`).join("") || "<tr><td colspan='6'>Không có môn phù hợp</td></tr>";
      if (count) count.textContent = shown.length + " / " + subjects.length + " môn";
      ui.content.querySelectorAll("[data-edit]").forEach((button) => {
        button.onclick = () => openSubject(subjects.find((row) => row.id === Number(button.dataset.edit)));
      });
    };
    document.getElementById("add-subject").onclick = () => openSubject(null);
    ui.content.querySelectorAll("[data-edit]").forEach((button) => {
      button.onclick = () => openSubject(subjects.find((row) => row.id === Number(button.dataset.edit)));
    });
  }

  function openSubject(row) {
    const body = openModal(row ? "Sửa môn học" : "Thêm môn học", `<form id="subject-form" class="stack">
      <label>Mã môn<input name="code" value="${esc(row?.code || "")}" required></label>
      <label>Tên môn<input name="name" value="${esc(row?.name || "")}" required></label>
      <label>Số tín chỉ<input name="credits" type="number" min="1" max="10" value="${row?.credits || 3}" required></label>
      <label>Bộ môn<select name="departmentId">${departments.map((item) => `<option value="${item.id}" ${row?.departmentId === item.id ? "selected" : ""}>${esc(item.name)}</option>`).join("")}</select></label>
      ${isAdmin ? `<label>Giáo viên<select name="lecturerId">${lecturers.map((item) => `<option value="${item.lecturerId}" ${row?.lecturerId === item.lecturerId ? "selected" : ""}>${esc(item.fullName)}</option>`).join("")}</select></label>` : ""}
      <label>Mô tả<textarea name="description">${esc(row?.description || "")}</textarea></label>
      <button class="btn primary" type="submit">Lưu</button>
    </form>`);
    body.querySelector("#subject-form").onsubmit = async (event) => {
      event.preventDefault();
      const data = Object.fromEntries(new FormData(event.target).entries());
      data.credits = Number(data.credits);
      data.departmentId = Number(data.departmentId);
      if (data.lecturerId) data.lecturerId = Number(data.lecturerId);
      try {
        if (row) await API.put("/api/subjects/" + row.id, data);
        else await API.post("/api/subjects", data);
        closeModal();
        toast("Đã lưu môn học");
        await load();
      } catch (error) { toast(error.message, "bad"); }
    };
  }

  load().catch((error) => { ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`; });
})();
