(async function () {
  const me = await guard(["admin", "lecturer"]);
  if (!me) return;
  const ui = mount({ title: "Ngân hàng câu hỏi", me });
  let subjects = [];
  let questions = [];

  ui.actions.innerHTML = `<a class="btn" id="template" href="/api/questions/template">Tải mẫu Excel</a><a class="btn" id="export" href="/api/questions/export">Xuất Excel</a>`;
  const importBtn = document.createElement("button");
  importBtn.className = "btn";
  importBtn.textContent = "Nhập Excel";
  importBtn.onclick = openImport;
  const addBtn = document.createElement("button");
  addBtn.className = "btn primary";
  addBtn.textContent = "Thêm câu hỏi";
  addBtn.onclick = () => openForm(null);
  ui.actions.append(importBtn, addBtn);

  async function load() {
    subjects = await API.get("/api/subjects");
    const subjectId = document.getElementById("subjectId")?.value || "";
    const difficulty = document.getElementById("difficulty")?.value || "";
    const q = document.getElementById("q")?.value.trim() || "";
    const params = new URLSearchParams();
    if (subjectId) params.set("subjectId", subjectId);
    if (difficulty) params.set("difficulty", difficulty);
    if (q) params.set("q", q);
    questions = await API.get("/api/questions" + (params.toString() ? "?" + params : ""));
    document.getElementById("export").href = "/api/questions/export" + (subjectId ? "?subjectId=" + subjectId : "");
    ui.content.innerHTML = `
      <div class="filters">
        <select id="subjectId"><option value="">Mọi môn</option>${subjects.map((row) => `<option value="${row.id}" ${String(row.id) === subjectId ? "selected" : ""}>${esc(row.code)} · ${esc(row.name)}</option>`).join("")}</select>
        <select id="difficulty"><option value="">Mọi độ khó</option><option value="easy" ${difficulty === "easy" ? "selected" : ""}>Dễ</option><option value="medium" ${difficulty === "medium" ? "selected" : ""}>Trung bình</option><option value="hard" ${difficulty === "hard" ? "selected" : ""}>Khó</option></select>
        <input id="q" placeholder="Tìm nội dung" value="${esc(q)}">
        <button class="btn" id="filter" type="button">Lọc</button>
      </div>
      <div class="card panel table-wrap"><table class="data"><thead><tr><th>Câu hỏi</th><th>Môn</th><th>Chương</th><th>Độ khó</th><th>Điểm</th><th></th></tr></thead>
      <tbody>${questions.map((row) => `<tr>
        <td>${esc(row.text)}<div class="muted">${esc(row.topic || "")}</div></td><td>${esc(row.subjectCode)}</td><td>${esc(row.chapter)}</td>
        <td>${esc(difficultyLabel(row.difficulty))}</td><td>${formatScore(row.score)}</td>
        <td class="row-actions"><button class="btn small" data-edit="${row.id}">Sửa</button><button class="btn small" data-hide="${row.id}">${row.isActive ? "Ẩn" : "Hiện"}</button></td>
      </tr>`).join("") || "<tr><td colspan='6'>Chưa có câu hỏi</td></tr>"}</tbody></table></div>`;
    document.getElementById("filter").onclick = () => load().catch((error) => toast(error.message, "bad"));
    ui.content.querySelectorAll("[data-edit]").forEach((button) => button.onclick = () => openForm(questions.find((row) => row.id === Number(button.dataset.edit))));
    ui.content.querySelectorAll("[data-hide]").forEach((button) => button.onclick = async () => {
      try { await API.post("/api/questions/" + button.dataset.hide + "/disable", {}); await load(); }
      catch (error) { toast(error.message, "bad"); }
    });
  }

  function openForm(row) {
    const source = row?.answers?.length ? row.answers.slice() : [];
    while (source.length < 4) source.push({ text: "" });
    const answers = source.slice(0, 4);
    const body = openModal(row ? "Sửa câu hỏi" : "Thêm câu hỏi", `<form id="q-form" class="stack">
      <label>Môn học<select name="subjectId">${subjects.map((item) => `<option value="${item.id}" ${row?.subjectId === item.id ? "selected" : ""}>${esc(item.code)}</option>`).join("")}</select></label>
      <label>Chương<input name="chapter" value="${esc(row?.chapter || "Chương 1")}" required></label>
      <label>Chủ đề<input name="topic" value="${esc(row?.topic || "")}" placeholder="Ví dụ: Vòng lặp"></label>
      <label>Độ khó<select name="difficulty"><option value="easy" ${row?.difficulty === "easy" ? "selected" : ""}>Dễ</option><option value="medium" ${row?.difficulty === "medium" ? "selected" : ""}>Trung bình</option><option value="hard" ${row?.difficulty === "hard" ? "selected" : ""}>Khó</option></select></label>
      <label>Điểm<input name="score" type="number" min="0.5" step="0.5" value="${row?.score || 1}" required></label>
      <label>Nội dung<textarea name="text" required>${esc(row?.text || "")}</textarea></label>
      <label>Giải thích<textarea name="explanation" placeholder="Hiện sau khi chấm bài">${esc(row?.explanation || "")}</textarea></label>
      ${answers.slice(0, 4).map((answer, index) => `<label>Đáp án ${"ABCD"[index]}<input name="answer${index}" value="${esc(answer.text || "")}" required></label>
        <input type="hidden" name="answerId${index}" value="${answer.id || ""}">`).join("")}
      <label>Đáp án đúng<select name="correct">${[0, 1, 2, 3].map((index) => `<option value="${index}" ${answers[index]?.correct ? "selected" : ""}>${"ABCD"[index]}</option>`).join("")}</select></label>
      <button class="btn primary" type="submit">Lưu câu hỏi</button>
    </form>`);
    body.querySelector("#q-form").onsubmit = async (event) => {
      event.preventDefault();
      const form = event.target;
      const correct = Number(form.correct.value);
      const payload = {
        subjectId: Number(form.subjectId.value),
        chapter: form.chapter.value,
        topic: form.topic.value,
        explanation: form.explanation.value,
        difficulty: form.difficulty.value,
        score: Number(form.score.value),
        text: form.text.value,
        isActive: row ? row.isActive : true,
        answers: [0, 1, 2, 3].map((index) => ({
          id: Number(form["answerId" + index].value || 0),
          text: form["answer" + index].value,
          correct: index === correct
        }))
      };
      try {
        if (row) await API.put("/api/questions/" + row.id, payload);
        else await API.post("/api/questions", payload);
        closeModal();
        toast("Đã lưu câu hỏi");
        await load();
      } catch (error) { toast(error.message, "bad"); }
    };
  }

  function openImport() {
    const body = openModal("Nhập từ Excel", `<p class="muted">Lưu file Excel thành CSV UTF-8 rồi chọn file. Cột đúng là A, B, C hoặc D.</p>
      <input id="csv-file" type="file" accept=".csv,text/csv">
      <button class="btn primary" id="do-import" type="button">Nhập</button>`);
    body.querySelector("#do-import").onclick = async () => {
      const file = body.querySelector("#csv-file").files[0];
      if (!file) return toast("Chọn file CSV", "bad");
      try {
        const csv = await file.text();
        const data = await API.post("/api/questions/import", { csv });
        closeModal();
        toast("Đã nhập " + data.imported + " câu");
        await load();
      } catch (error) { toast(error.message, "bad"); }
    };
  }

  load().catch((error) => { ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`; });
})();
