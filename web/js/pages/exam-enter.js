(async function () {
  const box = document.getElementById("enter-box");
  const status = document.getElementById("enter-status");
  const examId = Number(new URLSearchParams(location.search).get("examId") || 0);
  const here = "/exam/enter.html?examId=" + examId;

  function show(message, actionsHtml) {
    status.textContent = message;
    const old = box.querySelector(".row-actions");
    if (old) old.remove();
    if (actionsHtml) {
      const wrap = document.createElement("div");
      wrap.className = "row-actions";
      wrap.innerHTML = actionsHtml;
      box.appendChild(wrap);
    }
  }

  if (!examId) {
    show("Link thiếu mã kỳ thi.", `<a class="btn" href="/login.html">Về đăng nhập</a>`);
    return;
  }

  let me = null;
  try {
    me = await API.get("/api/auth/me");
  } catch (error) {
    location.href = loginUrl(here);
    return;
  }

  if (me.role !== "student") {
    show("Link này dành cho học sinh. Hãy đăng xuất rồi đăng nhập tài khoản học sinh.", `
      <button class="btn primary" id="switch-account" type="button">Đăng xuất để đổi tài khoản</button>
      <a class="btn" href="${esc(roleHome(me.role))}">Về trang của tôi</a>`);
    document.getElementById("switch-account").onclick = async () => {
      try { await API.post("/api/auth/logout", {}); } catch (error) {}
      location.href = loginUrl(here);
    };
    return;
  }

  if (me.mustChangePassword) {
    show("Bạn cần đổi mật khẩu trước khi làm bài.", `<a class="btn primary" href="/student/profile.html">Đổi mật khẩu</a>`);
    return;
  }

  try {
    status.textContent = "Đang vào phòng thi...";
    const data = await API.post("/api/exams/" + examId + "/start", {});
    const attemptId = data && data.attemptId;
    if (!attemptId) throw new Error("Không nhận được mã bài làm");
    location.replace("/exam/take.html?attemptId=" + attemptId);
  } catch (error) {
    show(error.message || "Không vào được bài thi.", `
      <a class="btn primary" href="/student/dashboard.html?examId=${examId}">Xem danh sách kỳ thi</a>
      <button class="btn" id="retry-enter" type="button">Thử lại</button>`);
    document.getElementById("retry-enter").onclick = () => location.reload();
  }
})();
