(async function () {
  const me = await guard(["student", "lecturer", "admin"]);
  if (!me) return;
  const attemptId = new URLSearchParams(location.search).get("attemptId");
  const ui = mount({ title: "Kết quả bài thi", me });
  if (!attemptId) {
    ui.content.innerHTML = `<p class="note bad">Thiếu mã lượt thi.</p>`;
    return;
  }
  try {
    const data = await API.get("/api/attempts/" + attemptId + "/result");
    const back = me.role === "student" ? "/student/dashboard.html" : "/lecturer/results.html?examId=" + data.examId;
    const percent = data.total ? (100 * data.score / data.total) : 0;
    ui.actions.innerHTML = `<a class="btn" href="${back}">Quay lại</a>`;
    ui.content.innerHTML = `
      <section class="card panel">
        <p class="eyebrow">Mã đề ${esc(data.paperCode)}</p>
        <h2 style="margin:6px 0">${esc(data.examTitle)}</h2>
        <div class="meta">
          <span>${esc(data.studentName || me.fullName)} ${data.studentCode ? "· " + esc(data.studentCode) : ""}</span>
          <span>${esc(attemptLabel(data.status))}</span>
          <span>${data.isAutoSubmitted ? "Nộp tự động" : "Nộp tay"}</span>
          <span>${esc(formatTime(data.submitTime))}</span>
        </div>
        <p style="font-family:Georgia,serif;font-size:40px;margin:8px 0">${formatScore(data.score)} / ${formatScore(data.total)}</p>
        <p class="muted">${formatScore(percent)}% · Đúng ${data.correctAnswers} · Sai ${data.wrongAnswers} · Bỏ ${data.skipped}${data.submitTime && data.startTime ? " · Làm trong " + Math.max(0, Math.round((data.submitTime - data.startTime) / 60)) + " phút" : ""}</p>
        ${me.role !== "student" && data.monitor ? `<p class="note">Ghi nhận để xem xét, không tự kết luận gian lận: chuyển tab ${data.monitor.tab}, rời cửa sổ ${data.monitor.blur}, tải lại ${data.monitor.refresh}, nối lại ${data.monitor.reconnect}, mất mạng ${data.monitor.offline}.</p>` : ""}
      </section>
      ${(data.questions || []).map((question) => `<article class="card panel">
        <p class="muted">Câu ${question.order} · ${formatScore(question.score)} điểm · ${question.gotPoint ? "Đúng" : "Chưa đúng"}</p>
        <h2>${esc(question.text)}</h2>
        ${question.explanation ? `<p class="note">Giải thích: ${esc(question.explanation)}</p>` : ""}
        <div class="stack">${(question.answers || []).map((answer, index) => {
          const cls = answer.correct ? "correct" : answer.chosen ? "wrong" : "";
          return `<div class="choice ${cls}"><span class="letter">${"ABCD"[index] || index + 1}</span><span>${esc(answer.text)}${answer.chosen ? " · đã chọn" : ""}${answer.correct ? " · đáp án đúng" : ""}</span></div>`;
        }).join("")}</div>
      </article>`).join("")}`;
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }
})();
