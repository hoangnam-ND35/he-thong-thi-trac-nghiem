(async function () {
  const box = document.getElementById("practice-box");
  const status = document.getElementById("practice-status");
  const params = new URLSearchParams(location.search);
  const ids = String(params.get("ids") || "")
    .split(",")
    .map((value) => Number(value.trim()))
    .filter((value) => value > 0);
  const here = "/exam/practice.html?ids=" + ids.join(",");

  function show(message, actionsHtml) {
    status.textContent = message;
    const old = box.querySelector(".row-actions");
    if (old) old.remove();
    if (!actionsHtml) return;
    const wrap = document.createElement("div");
    wrap.className = "row-actions";
    wrap.innerHTML = actionsHtml;
    box.appendChild(wrap);
  }

  if (!ids.length) {
    show("Link thiếu danh sách câu hỏi.", `<a class="btn" href="/login.html">Về đăng nhập</a>`);
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
    show("Link làm bài dành cho học sinh. Đăng xuất rồi đăng nhập tài khoản học sinh.", `
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
    status.textContent = "Đang tạo đề với " + ids.length + " câu đã chọn...";
    const data = await API.post("/api/practice/start", { questionIds: ids });
    const attemptId = data && data.attemptId;
    if (!attemptId) throw new Error("Không nhận được mã bài làm");
    location.replace("/exam/take.html?attemptId=" + attemptId);
  } catch (error) {
    show(error.message || "Không vào được bài làm.", `
      <a class="btn primary" href="/student/dashboard.html">Về kỳ thi của tôi</a>
      <button class="btn" id="retry-practice" type="button">Thử lại</button>`);
    const retry = document.getElementById("retry-practice");
    if (retry) retry.onclick = () => location.reload();
  }
})();
