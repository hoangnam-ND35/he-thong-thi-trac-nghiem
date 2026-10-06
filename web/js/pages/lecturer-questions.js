(async function () {
  const params0 = new URLSearchParams(location.search);
  const wantedIdsEarly = String(params0.get("ids") || "")
    .split(",")
    .map((value) => Number(value.trim()))
    .filter((value) => value > 0);
  if (wantedIdsEarly.length) {
    location.replace(loginUrl("/exam/practice.html?ids=" + wantedIdsEarly.join(",")));
    return;
  }

  const me = await guard(["admin", "partner", "lecturer"]);
  if (!me) return;
  const ui = mount({
    title: "Ngân hàng câu hỏi",
    me,
    lead: "Chọn câu rồi bấm Giao bài để lấy link. Học sinh mở link sẽ vào thẳng màn hình làm bài trên web."
  });
  let subjects = [];
  let questions = [];
  const selectedIds = new Set();
  let lastDraft = {
    subjectId: "",
    chapter: "Chương 1",
    topic: "",
    difficulty: "easy",
    score: 1
  };

  const wantedQuestion = Number(params0.get("questionId") || 0);

  ui.actions.innerHTML = `
    <a class="btn" id="template" href="/api/questions/template">Tải mẫu Excel</a>
    <a class="btn" id="export" href="/api/questions/export">Xuất Excel</a>
    <button class="btn primary" id="share-selected" type="button">Giao bài đã chọn</button>
    <button class="btn" id="share-all" type="button">Giao bài tất cả</button>`;
  const importBtn = document.createElement("button");
  importBtn.className = "btn";
  importBtn.textContent = "Nhập Excel";
  importBtn.onclick = openImport;
  const addBtn = document.createElement("button");
  addBtn.className = "btn primary";
  addBtn.textContent = "Thêm câu hỏi";
  addBtn.onclick = () => {
    if (!subjects.length) {
      toast("Chưa có môn được giao. Admin gán môn trong mục Môn học trước.", "bad");
      return;
    }
    openForm(null);
  };
  ui.actions.append(importBtn, addBtn);
  document.getElementById("share-selected").onclick = () => shareQuestions("selected");
  document.getElementById("share-all").onclick = () => shareQuestions("all");

  function currentFilters() {
    return {
      subjectId: document.getElementById("subjectId")?.value || "",
      chapter: document.getElementById("chapter")?.value || "",
      difficulty: document.getElementById("difficulty")?.value || "",
      q: document.getElementById("q")?.value.trim() || "",
      onlyActive: document.getElementById("onlyActive")?.checked !== false
    };
  }

  function practiceUrl(ids) {
    return assignmentLoginUrl("/exam/practice.html?ids=" + ids.join(","));
  }

  function chaptersOf(list) {
    const set = new Set();
    list.forEach((row) => {
      if (row.chapter) set.add(row.chapter);
    });
    return Array.from(set).sort((a, b) => a.localeCompare(b, "vi"));
  }

  function visibleQuestions(filters) {
    return questions.filter((row) => {
      if (filters.chapter && row.chapter !== filters.chapter) return false;
      if (filters.onlyActive && !row.isActive) return false;
      return true;
    });
  }

  function selectedOnPage(shown) {
    return shown.filter((row) => selectedIds.has(row.id));
  }

  function updateSelectionBar(shown) {
    const countEl = document.getElementById("selected-count");
    const selectAll = document.getElementById("select-all");
    const picked = selectedOnPage(shown);
    if (countEl) countEl.textContent = String(selectedIds.size);
    if (selectAll) {
      selectAll.checked = shown.length > 0 && picked.length === shown.length;
      selectAll.indeterminate = picked.length > 0 && picked.length < shown.length;
    }
  }

  function clip(text, max) {
    const value = String(text || "");
    if (value.length <= max) return esc(value);
    return esc(value.slice(0, max)) + "…";
  }

  async function copyText(text, okMessage) {
    let copied = false;
    try {
      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
        copied = true;
      }
    } catch (error) {
      copied = false;
    }
    if (!copied) {
      const area = document.createElement("textarea");
      area.value = text;
      document.body.appendChild(area);
      area.select();
      try { copied = document.execCommand("copy"); } catch (error) { copied = false; }
      area.remove();
    }
    toast(copied ? okMessage : "Hãy chọn ô và chép thủ công", copied ? "" : "bad");
  }

  async function load() {
    subjects = await API.get("/api/subjects");
    const filters = currentFilters();
    const seed = {
      subjectId: filters.subjectId || params0.get("subjectId") || "",
      chapter: filters.chapter || params0.get("chapter") || "",
      difficulty: filters.difficulty || params0.get("difficulty") || "",
      q: filters.q || params0.get("q") || "",
      onlyActive: document.getElementById("onlyActive")
        ? document.getElementById("onlyActive").checked
        : params0.get("onlyActive") !== "0"
    };
    const apiParams = new URLSearchParams();
    if (seed.subjectId) apiParams.set("subjectId", seed.subjectId);
    if (seed.difficulty) apiParams.set("difficulty", seed.difficulty);
    if (seed.q) apiParams.set("q", seed.q);
    questions = await API.get("/api/questions" + (apiParams.toString() ? "?" + apiParams : ""));
    document.getElementById("export").href = "/api/questions/export" + (seed.subjectId ? "?subjectId=" + seed.subjectId : "");

    const chapters = chaptersOf(questions);
    if (seed.chapter && !chapters.includes(seed.chapter)) seed.chapter = "";
    const chapterOptions = chapters.map((name) =>
      `<option value="${esc(name)}" ${seed.chapter === name ? "selected" : ""}>${esc(name)}</option>`
    ).join("");
    const shown = visibleQuestions(seed);
    const easy = shown.filter((row) => row.difficulty === "easy").length;
    const medium = shown.filter((row) => row.difficulty === "medium").length;
    const hard = shown.filter((row) => row.difficulty === "hard").length;
    if (seed.subjectId) lastDraft.subjectId = seed.subjectId;

    ui.content.innerHTML = `
      <section class="note ok">Link giao bài lấy bằng nút <strong>Giao bài</strong> bên dưới. Không copy URL trên thanh địa chỉ trình duyệt.</section>
      <div class="stats qb-stats">
        <div class="card stat"><span>Đang xem</span><b>${shown.length}</b><small>${questions.length} câu theo bộ lọc</small></div>
        <div class="card stat"><span>Dễ</span><b>${easy}</b></div>
        <div class="card stat"><span>Trung bình</span><b>${medium}</b></div>
        <div class="card stat"><span>Khó</span><b>${hard}</b></div>
      </div>
      <div class="filters qb-filters">
        <select id="subjectId"><option value="">Mọi môn</option>${subjects.map((row) =>
          `<option value="${row.id}" ${String(row.id) === String(seed.subjectId) ? "selected" : ""}>${esc(row.code)} · ${esc(row.name)}</option>`
        ).join("")}</select>
        <select id="chapter"><option value="">Mọi chương</option>${chapterOptions}</select>
        <select id="difficulty">
          <option value="">Mọi độ khó</option>
          <option value="easy" ${seed.difficulty === "easy" ? "selected" : ""}>Dễ</option>
          <option value="medium" ${seed.difficulty === "medium" ? "selected" : ""}>Trung bình</option>
          <option value="hard" ${seed.difficulty === "hard" ? "selected" : ""}>Khó</option>
        </select>
        <input id="q" placeholder="Tìm nội dung câu hỏi" value="${esc(seed.q)}">
        <label class="switch"><input id="onlyActive" type="checkbox" ${seed.onlyActive ? "checked" : ""}> Chỉ câu đang hiện</label>
        <button class="btn" id="filter" type="button">Lọc</button>
      </div>
      <div class="qb-select-bar">
        <label class="switch"><input id="select-all" type="checkbox"> Chọn tất cả đang xem</label>
        <span class="muted">Đã chọn <strong id="selected-count">${selectedIds.size}</strong> câu</span>
        <div class="row-actions">
          <button class="btn primary" id="share-picked" type="button">Giao bài đã chọn</button>
          <button class="btn" id="share-filtered" type="button">Giao bài tất cả</button>
          <button class="btn" id="clear-picked" type="button">Bỏ chọn</button>
        </div>
      </div>
      <div class="card panel table-wrap"><table class="data">
        <thead><tr>
          <th class="col-check"></th>
          <th>Câu hỏi</th><th>Môn</th><th>Chương</th><th>Độ khó</th><th>Điểm</th><th></th>
        </tr></thead>
        <tbody>${shown.map((row) => `<tr class="${row.isActive ? "" : "row-dim"}${selectedIds.has(row.id) ? " row-picked" : ""}">
          <td class="col-check"><input class="q-check" type="checkbox" data-id="${row.id}" ${selectedIds.has(row.id) ? "checked" : ""} aria-label="Chọn câu hỏi"></td>
          <td>
            <div class="q-text">${clip(row.text, 140)}</div>
            <div class="muted">${esc(row.topic || "")}${row.isActive ? "" : " · đang ẩn"}</div>
          </td>
          <td>${esc(row.subjectCode)}</td>
          <td>${esc(row.chapter)}</td>
          <td><span class="badge ${row.difficulty === "hard" ? "warn" : row.difficulty === "easy" ? "ok" : ""}">${esc(difficultyLabel(row.difficulty))}</span></td>
          <td>${formatScore(row.score)}</td>
          <td class="row-actions">
            <button class="btn small primary" data-link="${row.id}" type="button">Giao bài</button>
            <button class="btn small" data-edit="${row.id}" type="button">Sửa</button>
            <button class="btn small" data-hide="${row.id}" type="button">${row.isActive ? "Ẩn" : "Hiện"}</button>
          </td>
        </tr>`).join("") || "<tr><td colspan='7'>Chưa có câu hỏi khớp bộ lọc</td></tr>"}
        </tbody>
      </table></div>`;

    document.getElementById("filter").onclick = () => load().catch((error) => toast(error.message, "bad"));
    document.getElementById("share-filtered").onclick = () => shareQuestions("all");
    document.getElementById("share-picked").onclick = () => shareQuestions("selected");
    document.getElementById("clear-picked").onclick = () => {
      selectedIds.clear();
      ui.content.querySelectorAll(".q-check").forEach((box) => { box.checked = false; });
      ui.content.querySelectorAll("tr.row-picked").forEach((row) => row.classList.remove("row-picked"));
      updateSelectionBar(shown);
    };
    document.getElementById("select-all").onchange = (event) => {
      shown.forEach((row) => {
        if (event.target.checked) selectedIds.add(row.id);
        else selectedIds.delete(row.id);
      });
      ui.content.querySelectorAll(".q-check").forEach((box) => {
        box.checked = event.target.checked;
        box.closest("tr")?.classList.toggle("row-picked", event.target.checked);
      });
      updateSelectionBar(shown);
    };
    ui.content.querySelectorAll(".q-check").forEach((box) => {
      box.onchange = () => {
        const id = Number(box.dataset.id);
        if (box.checked) selectedIds.add(id);
        else selectedIds.delete(id);
        box.closest("tr")?.classList.toggle("row-picked", box.checked);
        updateSelectionBar(shown);
      };
    });
    document.getElementById("q").onkeydown = (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        load().catch((error) => toast(error.message, "bad"));
      }
    };
    ["subjectId", "chapter", "difficulty", "onlyActive"].forEach((id) => {
      document.getElementById(id).onchange = () => load().catch((error) => toast(error.message, "bad"));
    });
    ui.content.querySelectorAll("[data-link]").forEach((button) => {
      button.onclick = () => shareQuestion(Number(button.dataset.link));
    });
    ui.content.querySelectorAll("[data-edit]").forEach((button) => {
      button.onclick = () => openForm(questions.find((row) => row.id === Number(button.dataset.edit)));
    });
    ui.content.querySelectorAll("[data-hide]").forEach((button) => {
      button.onclick = async () => {
        try {
          await API.post("/api/questions/" + button.dataset.hide + "/disable", {});
          await load();
        } catch (error) {
          toast(error.message, "bad");
        }
      };
    });
    updateSelectionBar(shown);
  }

  function openForm(row, options = {}) {
    const draft = options.draft || lastDraft;
    const source = row?.answers?.length ? row.answers.slice() : [];
    while (source.length < 4) source.push({ text: "" });
    const answers = source.slice(0, 4);
    const selectedSubject = row?.subjectId || Number(draft.subjectId) || Number(currentFilters().subjectId) || subjects[0]?.id || "";
    const body = openModal(row ? "Sửa câu hỏi" : "Thêm câu hỏi", `<form id="q-form" class="stack qb-form">
      ${subjects.length ? "" : `<p class="note bad">Tài khoản này chưa được giao môn. Admin hoặc đối tác mở Môn học và gán môn cho giáo viên.</p>`}
      <div class="form-grid">
        <label>Môn học<select name="subjectId" required>${subjects.length
          ? subjects.map((item) => `<option value="${item.id}" ${Number(selectedSubject) === item.id ? "selected" : ""}>${esc(item.code)} · ${esc(item.name)}</option>`).join("")
          : `<option value="">Chưa có môn được giao</option>`}</select></label>
        <label>Chương<input name="chapter" value="${esc(row?.chapter || draft.chapter || "Chương 1")}" required list="chapter-suggest"></label>
        <datalist id="chapter-suggest">${chaptersOf(questions).map((name) => `<option value="${esc(name)}"></option>`).join("")}</datalist>
        <label>Chủ đề<input name="topic" value="${esc(row?.topic || draft.topic || "")}" placeholder="Ví dụ: Vòng lặp" list="topic-suggest"></label>
        <datalist id="topic-suggest">${Array.from(new Set(questions.map((item) => item.topic).filter(Boolean))).slice(0, 40).map((name) => `<option value="${esc(name)}"></option>`).join("")}</datalist>
        <label>Độ khó<select name="difficulty">
          <option value="easy" ${(row?.difficulty || draft.difficulty) === "easy" ? "selected" : ""}>Dễ</option>
          <option value="medium" ${(row?.difficulty || draft.difficulty) === "medium" ? "selected" : ""}>Trung bình</option>
          <option value="hard" ${(row?.difficulty || draft.difficulty) === "hard" ? "selected" : ""}>Khó</option>
        </select></label>
        <label>Điểm<input name="score" type="number" min="0.5" step="0.5" value="${row?.score || draft.score || 1}" required></label>
        <label class="wide">Nội dung câu hỏi<textarea name="text" rows="3" required placeholder="Nhập đề bài rõ ràng, ngắn gọn">${esc(row?.text || "")}</textarea></label>
        <label class="wide">Giải thích (hiện sau khi chấm)<textarea name="explanation" rows="2" placeholder="Không bắt buộc">${esc(row?.explanation || "")}</textarea></label>
      </div>
      <div class="answer-block">
        <div class="answer-head"><strong>Đáp án</strong><span class="muted">Chọn nút tròn bên trái để đánh dấu đáp án đúng</span></div>
        ${answers.map((answer, index) => {
          const letter = "ABCD"[index];
          const checked = answer.correct || (!row && index === 0 && !answers.some((item) => item.correct));
          return `<label class="answer-row">
            <input type="radio" name="correct" value="${index}" ${checked ? "checked" : ""} aria-label="Đáp án đúng ${letter}">
            <span class="answer-letter">${letter}</span>
            <input name="answer${index}" value="${esc(answer.text || "")}" required placeholder="Nội dung đáp án ${letter}">
            <input type="hidden" name="answerId${index}" value="${answer.id || ""}">
          </label>`;
        }).join("")}
      </div>
      <div class="row-actions qb-form-actions">
        <button class="btn primary" name="saveMode" value="close" type="submit">Lưu</button>
        ${row ? "" : `<button class="btn" name="saveMode" value="next" type="submit">Lưu & thêm tiếp</button>`}
        <button class="btn" type="button" data-cancel>Huỷ</button>
      </div>
    </form>`);
    const modal = document.querySelector(".modal");
    if (modal) modal.classList.add("modal-wide");
    const form = body.querySelector("#q-form");
    body.querySelector("[data-cancel]").onclick = () => closeModal();
    form.text.focus();

    let saveMode = "close";
    form.querySelectorAll("[name=saveMode]").forEach((button) => {
      button.addEventListener("click", () => { saveMode = button.value; });
    });

    form.onsubmit = async (event) => {
      event.preventDefault();
      const correct = Number(form.correct.value);
      const payload = {
        subjectId: Number(form.subjectId.value),
        chapter: form.chapter.value.trim(),
        topic: form.topic.value.trim(),
        explanation: form.explanation.value.trim(),
        difficulty: form.difficulty.value,
        score: Number(form.score.value),
        text: form.text.value.trim(),
        isActive: row ? row.isActive : true,
        answers: [0, 1, 2, 3].map((index) => ({
          id: Number(form["answerId" + index].value || 0),
          text: form["answer" + index].value.trim(),
          correct: index === correct
        }))
      };
      if (!payload.answers.every((item) => item.text)) {
        toast("Điền đủ 4 đáp án", "bad");
        return;
      }
      try {
        const saved = row ? await API.put("/api/questions/" + row.id, payload) : await API.post("/api/questions", payload);
        const id = saved && saved.id ? saved.id : row.id;
        lastDraft = {
          subjectId: String(payload.subjectId),
          chapter: payload.chapter,
          topic: payload.topic,
          difficulty: payload.difficulty,
          score: payload.score
        };
        toast("Đã lưu câu hỏi");
        if (saveMode === "next" && !row) {
          closeModal();
          await load();
          openForm(null, { draft: lastDraft });
          return;
        }
        closeModal();
        await load();
        shareQuestion(id);
      } catch (error) {
        toast(error.message, "bad");
      }
    };
  }

  function shareQuestion(id) {
    sharePracticeLink([id], "Giao bài 1 câu");
  }

  function shareQuestions(mode) {
    const filters = currentFilters();
    const shown = visibleQuestions(filters);
    const picked = mode === "selected"
      ? questions.filter((row) => selectedIds.has(row.id))
      : shown;
    if (mode === "selected" && !picked.length) {
      toast("Hãy tích chọn ít nhất một câu hỏi", "bad");
      return;
    }
    if (!picked.length) {
      toast("Không có câu hỏi để giao bài", "bad");
      return;
    }
    const ids = picked.map((row) => row.id);
    sharePracticeLink(ids, mode === "selected"
      ? "Giao bài " + ids.length + " câu đã chọn"
      : "Giao bài " + ids.length + " câu đang xem");
  }

  function sharePracticeLink(ids, title) {
    const url = practiceUrl(ids);
    const body = openModal(title, `
      <p class="muted">Gửi link này cho học sinh. Mở ra sẽ thấy trang đăng nhập trước, đăng nhập xong vào thẳng bài làm.</p>
      <label class="share-link-box">Link giao bài
        <input id="share-bank" readonly value="${esc(url)}">
      </label>
      <div class="row-actions">
        <button class="btn primary" id="copy-bank" type="button">Sao chép link</button>
        <a class="btn" id="open-preview" href="${esc(url)}" target="_blank" rel="noopener">Thử mở link</a>
      </div>
      <p class="note warn">Không gửi URL trên thanh địa chỉ trang giáo viên. Chỉ gửi link ở trên.</p>
    `);
    const bankInput = body.querySelector("#share-bank");
    bankInput.focus();
    bankInput.select();
    body.querySelector("#copy-bank").onclick = () => copyText(url, "Đã chép link giao bài");
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
      } catch (error) {
        toast(error.message, "bad");
      }
    };
  }

  load().then(() => {
    if (!wantedQuestion) return;
    const found = questions.find((row) => row.id === wantedQuestion);
    if (found) openForm(found);
    else toast("Không thấy câu hỏi", "bad");
  }).catch((error) => {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  });
})();
