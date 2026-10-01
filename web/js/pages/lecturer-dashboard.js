(async function () {
  const me = await guard(["lecturer"]);
  if (!me) return;
  const ui = mount({ title: "Bảng của giảng viên", me });
  try {
    const data = await API.get("/api/dashboard/lecturer");
    const card = (row) => `<article class="card exam-card"><h3>${esc(row.title)}</h3><div class="meta"><span>${esc(row.subjectCode)}</span><span>${esc(formatTime(row.startTime))} – ${esc(formatTime(row.endTime))}</span><span>${row.submitted} bài đã nộp</span></div></article>`;
    ui.content.innerHTML = `
      <div class="stats">
        <article class="card stat"><span>Đang diễn ra</span><b>${data.ongoing.length}</b></article>
        <article class="card stat"><span>Sinh viên đang thi</span><b>${data.inProgress || 0}</b></article>
        <article class="card stat"><span>Sắp tới</span><b>${data.upcoming.length}</b></article>
        <article class="card stat"><span>Bài đã nộp</span><b>${data.submittedCount}</b></article>
        <article class="card stat"><span>Điểm trung bình</span><b>${formatScore(data.averagePercent)}%</b></article>
      </div>
      <div class="grid-2">
        <section class="card panel"><h2>Kỳ thi đang diễn ra</h2><div class="card-list">${data.ongoing.map(card).join("") || "<p class='muted'>Không có kỳ thi đang mở.</p>"}</div></section>
        <section class="card panel"><h2>Kỳ thi sắp tới</h2><div class="card-list">${data.upcoming.map(card).join("") || "<p class='muted'>Không có kỳ thi sắp tới.</p>"}</div>
          <h2>Môn phụ trách</h2>
          <ul>${data.subjects.map((row) => `<li>${esc(row.code)} · ${esc(row.name)} · ${row.questions} câu</li>`).join("")}</ul>
        </section>
      </div>`;
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }
})();
