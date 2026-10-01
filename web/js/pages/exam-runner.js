(async function () {
  const me = await guard(["student"]);
  if (!me) return;
  if (me.mustChangePassword) {
    location.href = "/student/profile.html";
    return;
  }
  const attemptId = new URLSearchParams(location.search).get("attemptId");
  if (!attemptId) {
    location.href = "/student/dashboard.html";
    return;
  }

  const storageKey = "oes-attempt-" + attemptId;
  const readLocal = () => {
    try { return JSON.parse(localStorage.getItem(storageKey)) || { dirty: {}, pending: false }; }
    catch (error) { return { dirty: {}, pending: false }; }
  };
  const writeLocal = (data) => localStorage.setItem(storageKey, JSON.stringify(data));

  let paper;
  try {
    paper = await API.get("/api/attempts/" + attemptId);
  } catch (error) {
    document.body.innerHTML = `<p class="note bad" style="margin:24px">${esc(error.message)}</p>`;
    return;
  }
  if (paper.finalized) {
    location.href = "/exam/result.html?attemptId=" + attemptId;
    return;
  }

  const local = readLocal();
  const answers = {};
  (paper.savedAnswers || []).forEach((row) => { answers[row.questionId] = row.answerId; });
  Object.keys(local.dirty || {}).forEach((key) => { answers[key] = local.dirty[key]; });

  let index = 0;
  let remainingBase = Number(paper.remainingSec);
  let baseAt = performance.now();
  let serverTimeBase = Number(paper.serverTime);
  let serverAt = performance.now();
  let policy = paper.policy || { shortSec: 30, longSec: 300, graceSec: 300 };
  let offlineSince = null;
  let clientStatus = "IN_PROGRESS";
  let serverStatus = paper.status;
  let finalized = false;
  let locked = false;
  let finishing = false;
  let zeroHandled = false;
  const questions = paper.questions || [];

  document.body.className = "exam-body";
  document.body.innerHTML = `<div class="exam-shell" id="exam">
    <header class="exam-top">
      <div>
        <p class="eyebrow">Mã đề ${esc(paper.paperCode)} · ${esc(paper.subjectName)}</p>
        <h1>${esc(paper.examTitle)}</h1>
        <p id="server-clock" class="muted"></p>
        <p id="status-line"></p>
      </div>
      <div class="timer" id="timer"><small>THỜI GIAN CÒN</small><span id="clock">--:--</span></div>
    </header>
    <div id="banner" class="note" style="margin-top:12px"></div>
    <div class="exam-grid">
      <section class="card sheet" id="sheet"></section>
      <aside class="card palette">
        <div class="row-actions" style="justify-content:space-between;margin-bottom:10px">
          <strong id="progress"></strong>
          <label class="switch"><input id="offline-sim" type="checkbox" autocomplete="off"> Mô phỏng mất mạng</label>
        </div>
        <div class="q-nav" id="palette"></div>
        <p class="muted">Hệ thống lưu mã câu hỏi và mã đáp án, không lưu chữ A/B/C/D.</p>
        <div class="row-actions">
          <button class="btn" id="prev" type="button">Câu trước</button>
          <button class="btn" id="next" type="button">Câu sau</button>
          <button class="btn primary" id="submit" type="button">Nộp bài</button>
        </div>
      </aside>
    </div>
  </div>`;

  const clockNode = document.getElementById("clock");
  const timerNode = document.getElementById("timer");
  const banner = document.getElementById("banner");
  const sheet = document.getElementById("sheet");
  const palette = document.getElementById("palette");

  function shownRemaining() {
    return Math.max(0, remainingBase - (performance.now() - baseAt) / 1000);
  }

  let autosaveMs = Math.min(30000, Math.max(5000, Number(paper.autosaveSec || 8) * 1000));
  const eventAt = {};
  function reportEvent(kind) {
    const now = Date.now();
    if (finalized || API.simulateOffline) return;
    if (eventAt[kind] && now - eventAt[kind] < 4000) return;
    eventAt[kind] = now;
    API.post("/api/attempts/" + attemptId + "/events", { kind }).catch(() => {});
  }

  function applyClock(data) {
    if (data.autosaveSec) autosaveMs = Math.min(30000, Math.max(5000, Number(data.autosaveSec) * 1000));
    remainingBase = Number(data.remainingSec);
    baseAt = performance.now();
    serverTimeBase = Number(data.serverTime);
    serverAt = performance.now();
    if (data.policy) policy = data.policy;
    serverStatus = data.status;
    if (data.finalized) {
      finalized = true;
      location.href = "/exam/result.html?attemptId=" + attemptId;
    }
  }

  function markOffline() {
    if (!offlineSince) {
      offlineSince = performance.now();
      reportEvent("OFFLINE");
    }
    clientStatus = local.pending ? "SYNC_PENDING" : "OFFLINE";
  }

  function updateBanner() {
    const elapsed = offlineSince ? (performance.now() - offlineSince) / 1000 : 0;
    if (local.pending) {
      banner.className = "note bad";
      banner.textContent = "Pending Submit — bài đang chờ nộp. Câu trả lời đã lưu trên máy, sẽ gửi khi có mạng.";
      return;
    }
    if (!offlineSince) {
      banner.className = "note ok";
      banner.textContent = "Đang nối máy chủ. Mỗi câu trả lời được lưu trên máy rồi đồng bộ lên server.";
      return;
    }
    if (elapsed < policy.shortSec) {
      banner.className = "note";
      banner.textContent = "Mất kết nối ngắn — đang thử lại. Câu trả lời vẫn được giữ trên máy.";
    } else if (elapsed < policy.longSec) {
      banner.className = "note warn";
      banner.textContent = "Mất kết nối — câu trả lời đang được lưu cục bộ. Thời gian thi vẫn tiếp tục, không được cộng thêm.";
    } else {
      banner.className = "note bad";
      banner.textContent = "Mất kết nối quá " + policy.longSec + " giây. Hết giờ, máy chủ chấm các câu đã nhận sau thời gian ân hạn " + policy.graceSec + " giây.";
    }
  }

  function renderSheet() {
    const question = questions[index];
    if (!question) {
      sheet.innerHTML = "<p>Không có câu hỏi.</p>";
      return;
    }
    const answered = Object.keys(answers).length;
    document.getElementById("progress").textContent = answered + "/" + questions.length;
    document.getElementById("status-line").textContent = "Trạng thái máy: " + attemptLabel(clientStatus) + " · Trạng thái server: " + attemptLabel(serverStatus);
    sheet.innerHTML = `<p class="muted">Câu ${index + 1}/${questions.length} · ${formatScore(question.score)} điểm</p>
      <h2>${esc(question.text)}</h2>
      <div class="stack">${question.answers.map((answer, answerIndex) => {
        const selected = Number(answers[question.questionId]) === Number(answer.answerId);
        return `<button class="choice ${selected ? "selected" : ""}" type="button" data-answer-id="${answer.answerId}" ${locked ? "disabled" : ""}>
          <span class="letter">${"ABCD"[answerIndex] || answerIndex + 1}</span><span>${esc(answer.text)}</span></button>`;
      }).join("")}</div>`;
    palette.innerHTML = questions.map((item, itemIndex) => {
      const done = answers[item.questionId] ? "done" : "";
      const current = itemIndex === index ? "current" : "";
      return `<button class="${done} ${current}" type="button" data-goto="${itemIndex}">${itemIndex + 1}</button>`;
    }).join("");
    updateBanner();
  }

  function choose(answerId) {
    if (locked || finalized) return;
    const question = questions[index];
    answers[question.questionId] = Number(answerId);
    local.dirty[question.questionId] = Number(answerId);
    writeLocal(local);
    renderSheet();
    clearTimeout(choose.timer);
    choose.timer = setTimeout(syncAnswers, 400);
  }

  function answerPayload() {
    return Object.keys(answers).map((questionId) => ({
      questionId: Number(questionId),
      answerId: Number(answers[questionId])
    }));
  }

  async function syncAnswers() {
    if (finalized || API.simulateOffline || !Object.keys(local.dirty).length) return;
    try {
      const data = await API.post("/api/attempts/" + attemptId + "/sync", { answers: answerPayload() });
      if (API.simulateOffline) return;
      local.dirty = {};
      writeLocal(local);
      offlineSince = null;
      clientStatus = "IN_PROGRESS";
      applyClock(data);
      updateBanner();
    } catch (error) {
      if (error.offline) markOffline();
      else toast(error.message, "bad");
      updateBanner();
    }
  }

  async function pollTime() {
    if (finalized || API.simulateOffline) return;
    try {
      const data = await API.get("/api/attempts/" + attemptId + "/time");
      if (API.simulateOffline) return;
      offlineSince = null;
      if (!local.pending) clientStatus = "IN_PROGRESS";
      applyClock(data);
      updateBanner();
    } catch (error) {
      if (error.offline) markOffline();
      updateBanner();
    }
  }

  async function finish(auto) {
    if (finalized || finishing) return;
    if (!auto) {
      const ok = await confirmBox(API.simulateOffline
        ? "Đang mất mạng. Bài sẽ được đánh dấu chờ nộp và gửi khi có kết nối."
        : "Nộp bài và kết thúc lượt thi?");
      if (!ok) return;
    }
    finishing = true;
    locked = true;
    local.pending = true;
    Object.keys(answers).forEach((key) => { local.dirty[key] = answers[key]; });
    writeLocal(local);
    clientStatus = "SYNC_PENDING";
    renderSheet();
    try {
      await API.post("/api/attempts/" + attemptId + "/submit", { answers: answerPayload() });
      finalized = true;
      local.pending = false;
      local.dirty = {};
      writeLocal(local);
      location.href = "/exam/result.html?attemptId=" + attemptId;
    } catch (error) {
      finishing = false;
      if (error.offline) markOffline();
      else toast(error.message, "bad");
      updateBanner();
    }
  }

  document.getElementById("exam").addEventListener("click", (event) => {
    const choice = event.target.closest("[data-answer-id]");
    if (choice) choose(choice.dataset.answerId);
    const jump = event.target.closest("[data-goto]");
    if (jump) {
      index = Number(jump.dataset.goto);
      renderSheet();
    }
  });
  document.getElementById("prev").onclick = () => { index = Math.max(0, index - 1); renderSheet(); };
  document.getElementById("next").onclick = () => { index = Math.min(questions.length - 1, index + 1); renderSheet(); };
  document.getElementById("submit").onclick = () => finish(false);
  const simulator = document.getElementById("offline-sim");
  const applySimulator = () => {
    API.setOfflineSimulator(simulator.checked);
    if (simulator.checked) markOffline();
    else {
      offlineSince = null;
      clientStatus = local.pending ? "SYNC_PENDING" : "IN_PROGRESS";
      if (local.pending) finish(true);
      else syncAnswers().then(pollTime);
    }
    updateBanner();
    document.getElementById("status-line").textContent = "Trạng thái máy: " + attemptLabel(clientStatus) + " · Trạng thái server: " + attemptLabel(serverStatus);
  };
  simulator.checked = false;
  let userToggled = false;
  const onSimulator = () => {
    userToggled = true;
    applySimulator();
  };
  simulator.addEventListener("change", onSimulator);
  simulator.addEventListener("click", onSimulator);
  const clearRestored = () => {
    if (!userToggled && simulator.checked) simulator.checked = false;
  };
  const restoreWatch = setInterval(clearRestored, 100);
  setTimeout(() => clearInterval(restoreWatch), 1200);
  document.addEventListener("keydown", (event) => {
    if (locked) return;
    if (event.key >= "1" && event.key <= "4") {
      const question = questions[index];
      const answer = question?.answers[Number(event.key) - 1];
      if (answer) choose(answer.answerId);
    }
    if (event.key === "ArrowRight") document.getElementById("next").click();
    if (event.key === "ArrowLeft") document.getElementById("prev").click();
  });
  window.addEventListener("beforeunload", (event) => {
    if (!finalized) {
      event.preventDefault();
      event.returnValue = "";
    }
  });

  setInterval(() => {
    const left = shownRemaining();
    clockNode.textContent = formatClock(left);
    timerNode.className = "timer" + (left <= 60 ? " bad" : left <= 300 ? " warn" : "");
    const serverNow = serverTimeBase + (performance.now() - serverAt) / 1000;
    document.getElementById("server-clock").textContent = (offlineSince ? "Giờ máy chủ (nội suy): " : "Giờ máy chủ: ") +
      new Date(serverNow * 1000).toLocaleTimeString("vi-VN");
    document.getElementById("status-line").textContent = "Trạng thái máy: " + attemptLabel(clientStatus) + " · Trạng thái server: " + attemptLabel(serverStatus);
    updateBanner();
    if (left <= 0 && !zeroHandled) {
      zeroHandled = true;
      finish(true);
    }
  }, 250);
  setInterval(pollTime, 5000);
  setInterval(() => {
    if (finalized || API.simulateOffline || !Object.keys(answers).length) return;
    Object.keys(answers).forEach((key) => { local.dirty[key] = answers[key]; });
    syncAnswers();
  }, autosaveMs);
  document.addEventListener("visibilitychange", () => { if (document.hidden) reportEvent("TAB"); });
  window.addEventListener("blur", () => reportEvent("BLUR"));
  if (performance.getEntriesByType("navigation")[0]?.type === "reload") reportEvent("REFRESH");
  if (Object.keys(local.dirty).length) reportEvent("RECONNECT");
  setInterval(() => {
    if (local.pending && !finalized && !finishing && !API.simulateOffline) finish(true);
  }, 2000);

  renderSheet();
  if (local.pending) finish(true);
  else if (Object.keys(local.dirty).length) syncAnswers();
})();
