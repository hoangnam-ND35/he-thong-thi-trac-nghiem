(async function () {
  const me = await guard(["admin", "lecturer"]);
  if (!me) return;
  const ui = mount({ title: "Kết quả và thống kê", me });
  const exams = await API.get("/api/exams");
  const selected = new URLSearchParams(location.search).get("examId") || (exams[0] ? String(exams[0].id) : "");
  ui.content.innerHTML = `<div class="filters"><select id="exam">${exams.map((row) => `<option value="${row.id}" ${String(row.id) === selected ? "selected" : ""}>${esc(row.title)}</option>`).join("")}</select></div><div id="report"></div>`;
  document.getElementById("exam").onchange = () => show(document.getElementById("exam").value);
  if (selected) show(selected);

  async function show(examId) {
    const report = document.getElementById("report");
    try {
      const [analysis, rows] = await Promise.all([
        API.get("/api/exams/" + examId + "/analysis"),
        API.get("/api/exams/" + examId + "/results")
      ]);
      report.innerHTML = `
        <div class="stats">
          <article class="card stat"><span>Bài đã chấm</span><b>${analysis.gradedCount}</b></article>
          <article class="card stat"><span>Trung bình</span><b>${formatScore(analysis.average)}</b></article>
          <article class="card stat"><span>Cao nhất</span><b>${formatScore(analysis.highest)}</b></article>
          <article class="card stat"><span>Thấp nhất</span><b>${formatScore(analysis.lowest)}</b></article>
        </div>
        <section class="card panel table-wrap"><h2>Bài làm</h2><table class="data"><thead><tr><th>Học sinh</th><th>Lớp</th><th>Mã đề</th><th>Đúng / sai / bỏ</th><th>Điểm</th><th>Theo dõi</th><th></th></tr></thead>
          <tbody>${rows.map((row) => `<tr><td>${esc(row.fullName)}<div class="muted">${esc(row.studentCode)}</div></td><td>${esc(row.className)}</td><td>${esc(row.paperCode)}<div class="muted">${esc(attemptLabel(row.status))}</div></td><td>${row.correctAnswers ?? "—"} / ${row.wrongAnswers ?? "—"} / ${row.skipped ?? "—"}</td><td>${formatScore(row.score)}/${formatScore(row.total)}</td><td class="muted">Tab ${row.tab || 0} · Nối ${row.reconnect || 0} · Mất mạng ${row.offline || 0}</td><td>${row.submitTime ? `<a href="/exam/result.html?attemptId=${row.attemptId}">Xem bài</a>` : ""}</td></tr>`).join("") || "<tr><td colspan='7'>Chưa có bài</td></tr>"}</tbody></table></section>
        <section class="card panel"><h2>Tỷ lệ đúng từng câu</h2>
          ${(analysis.questions || []).map((row) => `<p>${esc(row.text)}</p><div class="bar" title="${formatScore(row.correctRate)}%"><span style="width:${Math.max(0, Math.min(100, row.correctRate))}%"></span></div><p class="muted">${row.correct}/${row.seen} lượt đúng</p>`).join("") || "<p class='muted'>Chưa đủ bài để phân tích.</p>"}
        </section>`;
    } catch (error) {
      report.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
    }
  }
})();
