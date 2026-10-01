(async function () {
  const me = await guard(["admin", "lecturer"]);
  if (!me) return;
  const ui = mount({ title: "Kỳ thi", me });
  let subjects = [];
  let classes = [];
  let exams = [];
  let editing = null;

  async function load() {
    [subjects, classes, exams] = await Promise.all([
      API.get("/api/subjects"),
      API.get("/api/classes"),
      API.get("/api/exams")
    ]);
    ui.content.innerHTML = `<div class="grid-2">
      <section class="card-list" id="exam-list">${exams.map((row) => `<article class="card exam-card">
        <h3>${esc(row.title)}</h3>
        <div class="meta"><span>${esc(row.subjectCode)}</span><span class="badge ${row.status === "published" ? "ok" : ""}">${esc(examLabel(row.status))}</span><span>${row.totalQuestions} câu · ${row.durationMinutes} phút</span><span>${esc(row.className)}</span></div>
        <p class="muted">${esc(formatTime(row.startTime))} – ${esc(formatTime(row.endTime))}</p>
        <div class="meta"><span>Trộn câu: ${row.shuffleQuestions ? "Có" : "Không"}</span><span>Trộn đáp án: ${row.shuffleAnswers ? "Có" : "Không"}</span><span>Tự nộp: ${row.autoSubmit ? "Có" : "Không"}</span><span>${row.attemptCount} lượt làm</span></div>
        <div class="row-actions">
          <button class="btn small" data-edit="${row.id}">Sửa</button>
          ${row.status !== "published" ? `<button class="btn small primary" data-publish="${row.id}">Mở kỳ thi</button>` : `<button class="btn small" data-close="${row.id}">Đóng</button>`}
          ${row.status === "draft" ? `<button class="btn small danger" data-delete="${row.id}">Xóa</button>` : ""}
          <a class="btn small" href="/lecturer/results.html?examId=${row.id}">Kết quả</a>
        </div>
      </article>`).join("") || "<p class='muted'>Chưa có kỳ thi.</p>"}</section>
      <section class="card panel">
        <h2 id="form-title">Tạo kỳ thi</h2>
        <form id="exam-form" class="stack">
          <label>Tên kỳ thi<input name="title" required></label>
          <label>Môn học<select name="subjectId">${subjects.map((row) => `<option value="${row.id}">${esc(row.code)} · ${esc(row.name)}</option>`).join("")}</select></label>
          <p id="bank" class="note">Chọn môn để xem số câu trong ngân hàng.</p>
          <label>Mô tả<textarea name="description"></textarea></label>
          <label>Mở đề<input name="startTime" type="datetime-local" required></label>
          <label>Đóng đề<input name="endTime" type="datetime-local" required></label>
          <label>Thời lượng (phút)<input name="durationMinutes" type="number" min="1" value="45" required></label>
          <div class="filters">
            <label>Dễ<input name="easyCount" type="number" min="0" value="8"></label>
            <label>Trung bình<input name="mediumCount" type="number" min="0" value="8"></label>
            <label>Khó<input name="hardCount" type="number" min="0" value="4"></label>
          </div>
          <div id="matrix-box"></div>
          <label>Số lần thi<input name="maxAttempts" type="number" min="1" max="10" value="1"></label>
          <label>Lớp<select name="classId"><option value="0">Tất cả các lớp</option>${classes.map((row) => `<option value="${row.id}">${esc(row.name)}</option>`).join("")}</select></label>
          <div class="check-grid">
            <label><input name="shuffleQuestions" type="checkbox" checked> Trộn câu hỏi</label>
            <label><input name="shuffleAnswers" type="checkbox" checked> Trộn đáp án</label>
            <label><input name="autoSubmit" type="checkbox" checked> Tự động nộp</label>
          </div>
          <details>
            <summary>Chính sách mất kết nối</summary>
            <div class="stack" style="margin-top:8px">
              <label>Cảnh báo ngắn (giây)<input name="shortDisconnectSec" type="number" value="30"></label>
              <label>Cảnh báo dài (giây)<input name="longDisconnectSec" type="number" value="300"></label>
              <label>Ân hạn đồng bộ sau hết giờ (giây)<input name="syncGraceSec" type="number" value="300"></label>
              <p class="muted">Thời gian thi không được cộng thêm khi mất mạng. Hết giờ, bài lưu cục bộ còn được nộp trong thời gian ân hạn.</p>
            </div>
          </details>
          <div class="row-actions">
            <button class="btn" name="mode" value="draft" type="submit">Lưu nháp</button>
            <button class="btn primary" name="mode" value="publish" type="submit">Lưu và mở</button>
            <button class="btn ghost" id="reset-form" type="button">Tạo mới</button>
          </div>
        </form>
      </section>
    </div>`;
    const form = document.getElementById("exam-form");
    const now = new Date();
    const later = new Date(now.getTime() + 14 * 86400000);
    form.startTime.value = toInputTime(Math.floor(now.getTime() / 1000));
    form.endTime.value = toInputTime(Math.floor(later.getTime() / 1000));
    form.subjectId.onchange = () => refreshBank();
    refreshBank();
    form.onsubmit = save;
    document.getElementById("reset-form").onclick = () => { editing = null; load(); };
    ui.content.querySelectorAll("[data-edit]").forEach((button) => button.onclick = () => fill(exams.find((row) => row.id === Number(button.dataset.edit))));
    ui.content.querySelectorAll("[data-publish]").forEach((button) => button.onclick = () => act("publish", button.dataset.publish));
    ui.content.querySelectorAll("[data-close]").forEach((button) => button.onclick = () => act("close", button.dataset.close));
    ui.content.querySelectorAll("[data-delete]").forEach((button) => button.onclick = () => act("delete", button.dataset.delete));
    const current = exams.find((row) => row.id === editing);
    if (current) fill(current);
  }

  function readMatrix() {
    return [...document.querySelectorAll("#matrix-box [data-chapter]")].reduce((rows, input) => {
      let row = rows.find((item) => item.chapter === input.dataset.chapter);
      if (!row) {
        row = { chapter: input.dataset.chapter, easyCount: 0, mediumCount: 0, hardCount: 0 };
        rows.push(row);
      }
      row[input.dataset.level] = Number(input.value || 0);
      return rows;
    }, []);
  }

  function paintMatrix(chapters, chosen) {
    const box = document.getElementById("matrix-box");
    if (!box) return;
    const saved = chosen || readMatrix();
    box.innerHTML = `<h3 style="margin:0">Ma trận theo chương</h3>
      <div class="table-wrap"><table class="data"><thead><tr><th>Chương</th><th>Có sẵn</th><th>Dễ</th><th>Trung bình</th><th>Khó</th></tr></thead>
      <tbody>${(chapters || []).map((row) => {
        const old = saved.find((item) => item.chapter === row.chapter) || {};
        return `<tr><td>${esc(row.chapter)}</td><td class="muted">${row.easy}/${row.medium}/${row.hard}</td>
          <td><input data-chapter="${esc(row.chapter)}" data-level="easyCount" type="number" min="0" value="${old.easyCount || 0}"></td>
          <td><input data-chapter="${esc(row.chapter)}" data-level="mediumCount" type="number" min="0" value="${old.mediumCount || 0}"></td>
          <td><input data-chapter="${esc(row.chapter)}" data-level="hardCount" type="number" min="0" value="${old.hardCount || 0}"></td></tr>`;
      }).join("") || "<tr><td colspan='5'>Môn này chưa có chương trong ngân hàng.</td></tr>"}</tbody></table></div>
      <p class="muted">Nhập số câu theo chương để hệ thống tự rút đề. Để mọi ô bằng 0 thì dùng ba ô tổng phía trên.</p>`;
  }

  async function refreshBank(chosen) {
    const form = document.getElementById("exam-form");
    if (!form) return;
    try {
      const bank = await API.get("/api/questions/bank?subjectId=" + form.subjectId.value);
      document.getElementById("bank").textContent = `Ngân hàng đang có ${bank.easy} dễ, ${bank.medium} trung bình, ${bank.hard} khó.`;
      paintMatrix(bank.chapters || [], chosen);
    } catch (error) {
      document.getElementById("bank").textContent = error.message;
    }
  }

  function fill(row) {
    if (!row) return;
    editing = row.id;
    const form = document.getElementById("exam-form");
    document.getElementById("form-title").textContent = "Sửa kỳ thi";
    form.title.value = row.title;
    form.subjectId.value = row.subjectId;
    form.description.value = row.description || "";
    form.startTime.value = toInputTime(row.startTime);
    form.endTime.value = toInputTime(row.endTime);
    form.durationMinutes.value = row.durationMinutes;
    form.easyCount.value = row.easyCount;
    form.mediumCount.value = row.mediumCount;
    form.hardCount.value = row.hardCount;
    form.maxAttempts.value = row.maxAttempts;
    form.classId.value = row.classId;
    form.shuffleQuestions.checked = row.shuffleQuestions;
    form.shuffleAnswers.checked = row.shuffleAnswers;
    form.autoSubmit.checked = row.autoSubmit;
    form.shortDisconnectSec.value = row.shortDisconnectSec;
    form.longDisconnectSec.value = row.longDisconnectSec;
    form.syncGraceSec.value = row.syncGraceSec;
    const locked = row.attemptCount > 0;
    ["subjectId", "durationMinutes", "easyCount", "mediumCount", "hardCount", "shuffleQuestions", "shuffleAnswers"].forEach((name) => {
      form[name].disabled = locked;
    });
    refreshBank(row.matrix || []).then(() => {
      document.querySelectorAll("#matrix-box input").forEach((input) => { input.disabled = locked; });
    });
  }

  async function save(event) {
    event.preventDefault();
    const form = event.target;
    const publish = event.submitter?.value === "publish";
    const payload = {
      title: form.title.value.trim(),
      subjectId: Number(form.subjectId.value),
      description: form.description.value,
      startTime: toEpoch(form.startTime.value),
      endTime: toEpoch(form.endTime.value),
      durationMinutes: Number(form.durationMinutes.value),
      easyCount: Number(form.easyCount.value),
      mediumCount: Number(form.mediumCount.value),
      hardCount: Number(form.hardCount.value),
      maxAttempts: Number(form.maxAttempts.value),
      classId: Number(form.classId.value),
      shuffleQuestions: form.shuffleQuestions.checked,
      shuffleAnswers: form.shuffleAnswers.checked,
      autoSubmit: form.autoSubmit.checked,
      shortDisconnectSec: Number(form.shortDisconnectSec.value),
      longDisconnectSec: Number(form.longDisconnectSec.value),
      syncGraceSec: Number(form.syncGraceSec.value),
      matrix: readMatrix(),
      publish
    };
    const matrixTotal = payload.matrix.reduce((sum, row) => sum + row.easyCount + row.mediumCount + row.hardCount, 0);
    if (matrixTotal > 0) {
      payload.easyCount = payload.matrix.reduce((sum, row) => sum + row.easyCount, 0);
      payload.mediumCount = payload.matrix.reduce((sum, row) => sum + row.mediumCount, 0);
      payload.hardCount = payload.matrix.reduce((sum, row) => sum + row.hardCount, 0);
    }
    try {
      if (editing) {
        await API.put("/api/exams/" + editing, payload);
        if (publish) await API.post("/api/exams/" + editing + "/publish", {});
      } else {
        await API.post("/api/exams", payload);
      }
      toast(publish ? "Đã mở kỳ thi" : "Đã lưu kỳ thi");
      editing = null;
      await load();
    } catch (error) {
      toast(error.message, "bad");
    }
  }

  async function act(kind, id) {
    const text = { publish: "Mở kỳ thi này?", close: "Đóng kỳ thi này?", delete: "Xóa kỳ thi nháp?" }[kind];
    if (!(await confirmBox(text))) return;
    try {
      await API.post(`/api/exams/${id}/${kind}`, {});
      toast("Đã cập nhật kỳ thi");
      await load();
    } catch (error) { toast(error.message, "bad"); }
  }

  load().catch((error) => { ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`; });
})();
