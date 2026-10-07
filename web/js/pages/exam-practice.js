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

  const unlocked = sessionStorage.getItem("oes-assignment-ok") === here;
  sessionStorage.removeItem("oes-assignment-ok");
  if (!unlocked) {
    location.replace(loginUrl(here));
    return;
  }

  let me = null;
  try {
    me = await API.get("/api/auth/me");
  } catch (error) {
    location.replace(loginUrl(here));
    return;
  }

  if (me.role !== "student") {
    show("Phải đăng nhập tài khoản học sinh mới làm được bài.", `
      <a class="btn primary" href="${esc(loginUrl(here))}">Đăng nhập học sinh</a>`);
    return;
  }

  if (me.mustChangePassword) {
    show("Bạn cần đổi mật khẩu trước khi làm bài.", `<a class="btn primary" href="/student/profile.html">Đổi mật khẩu</a>`);
    return;
  }

  try {
    status.textContent = "Đang mở bài làm...";
    const data = await API.post("/api/practice/start", { questionIds: ids });
    const attemptId = data && data.attemptId;
    if (!attemptId) throw new Error("Không nhận được mã bài làm");
    location.replace("/exam/take.html?attemptId=" + attemptId);
  } catch (error) {
    show(error.message || "Không vào được bài làm.", `
      <a class="btn primary" href="${esc(loginUrl(here))}">Đăng nhập lại</a>
      <button class="btn" id="retry-practice" type="button">Thử lại</button>`);
    const retry = document.getElementById("retry-practice");
    if (retry) retry.onclick = () => {
      sessionStorage.setItem("oes-assignment-ok", here);
      location.reload();
    };
  }
})();
