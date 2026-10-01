(async function () {
  const me = await guard(["student"]);
  if (!me) return;
  const ui = mount({ title: "Kỳ thi của tôi", me });
  try {
    const [exams, dash] = await Promise.all([
      API.get("/api/student/exams"),
      API.get("/api/dashboard/student")
    ]);
    const doing = exams.filter((row) => row.canResume);
    const ready = exams.filter((row) => row.canStart && !row.lastAttemptId);
    const upcoming = exams.filter((row) => !row.canStart && !row.canResume && row.startTime * 1000 > Date.now());
    const history = exams.filter((row) => row.lastAttemptId && !row.canResume);
    const card = (row, action) => `<article class="card exam-card">
      <h3>${esc(row.title)}</h3>
      <div class="meta"><span>${esc(row.subjectCode)} · ${esc(row.subjectName)}</span><span>${row.totalQuestions} câu · ${row.durationMinutes} phút</span><span>Đã dùng ${row.attemptsUsed}/${row.maxAttempts} lần</span></div>
      <p class="muted">${esc(formatTime(row.startTime))} – ${esc(formatTime(row.endTime))}</p>
      <div class="meta"><span>Trộn câu ${row.shuffleQuestions ? "có" : "không"}</span><span>Trộn đáp án ${row.shuffleAnswers ? "có" : "không"}</span><span>Tự nộp ${row.autoSubmit ? "có" : "không"}</span></div>
      ${row.lastAttemptId && !row.canResume ? `<p>Điểm gần nhất: <strong>${formatScore(row.lastScore)}/${formatScore(row.lastTotal)}</strong> · ${esc(attemptLabel(row.lastStatus))}</p>` : ""}
      <div class="row-actions">${action}</div>
    </article>`;
    ui.content.innerHTML = `
      <section class="note">Bốn cơ chế của phòng thi: mỗi người một mã đề, đồng hồ lấy từ máy chủ, mất mạng vẫn lưu bài trên máy, hết giờ thì tự nộp kể cả khi chưa có mạng.</section>
      ${(dash.notifications || []).length ? `<section class="card panel"><h2>Thông báo</h2>${dash.notifications.map((row) => `<p>${esc(row.message)} <span class="muted">${esc(formatTime(row.createdAt))}</span></p>`).join("")}</section>` : ""}
      <section class="card panel"><h2>Đang làm</h2><div class="card-list">${doing.map((row) => card(row, `<button class="btn primary" data-resume="${row.openAttemptId}">${row.needsFinalize ? "Đồng bộ bài" : "Làm tiếp"}</button>`)).join("") || "<p class='muted'>Không có bài đang mở.</p>"}</div></section>
      <section class="card panel"><h2>Có thể bắt đầu</h2><div class="card-list">${ready.map((row) => card(row, `<button class="btn primary" data-start="${row.id}">Bắt đầu</button>`)).join("") || "<p class='muted'>Chưa có kỳ thi sẵn sàng.</p>"}</div></section>
      <section class="card panel"><h2>Sắp diễn ra</h2><div class="card-list">${upcoming.map((row) => card(row, "")).join("") || "<p class='muted'>Không có lịch sắp tới.</p>"}</div></section>
      <section class="card panel"><h2>Lịch sử</h2><div class="card-list">${history.map((row) => card(row, `<a class="btn" href="/exam/result.html?attemptId=${row.lastAttemptId}">Xem điểm</a>${row.canStart ? `<button class="btn primary" data-start="${row.id}">Thi lại</button>` : ""}`)).join("") || "<p class='muted'>Chưa có bài đã nộp.</p>"}</div></section>`;
    ui.content.querySelectorAll("[data-start]").forEach((button) => button.onclick = () => startExam(button.dataset.start));
    ui.content.querySelectorAll("[data-resume]").forEach((button) => button.onclick = () => {
      location.href = "/exam/take.html?attemptId=" + button.dataset.resume;
    });
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }

  async function startExam(id) {
    if (!(await confirmBox("Bắt đầu bài thi? Thời gian tính từ máy chủ ngay khi vào."))) return;
    try {
      const data = await API.post("/api/exams/" + id + "/start", {});
      location.href = "/exam/take.html?attemptId=" + data.attemptId;
    } catch (error) {
      toast(error.message, "bad");
    }
  }
})();
