const form = document.getElementById("login-form");
bindSecretToggles(form);
loadBrand().then(applyBrand);

const nextPath = safeNextPath(new URLSearchParams(location.search).get("next") || "");
const fromAssignment = isAssignmentNext(nextPath);

async function prepareLoginPage() {
  if (fromAssignment) {
    const lead = form.querySelector(".muted");
    if (lead) lead.textContent = "Nhập tài khoản học sinh để vào bài làm. Chưa đăng nhập thì không mở được bài.";
    const title = form.querySelector("h2");
    if (title) title.textContent = "Đăng nhập để làm bài";
    try { await API.post("/api/auth/logout", {}); } catch (error) {}
    form.username.value = "";
    form.password.value = "";
    form.username.focus();
    return;
  }
  try {
    const me = await API.get("/api/auth/me");
    location.href = afterLoginPath(me, roleHome(me.role));
  } catch (error) {}
}

if (nextPath && !fromAssignment) {
  const lead = form.querySelector(".muted");
  if (lead) lead.textContent = "Đăng nhập xong sẽ vào trang bạn vừa mở.";
}

prepareLoginPage();

form.onsubmit = async (event) => {
  event.preventDefault();
  const error = document.getElementById("login-error");
  const button = form.querySelector("[type=submit]");
  error.textContent = "";
  button.disabled = true;
  const label = button.textContent;
  button.textContent = "Đang vào...";
  try {
    const me = await API.post("/api/auth/login", {
      username: form.username.value.trim(),
      password: form.password.value
    });
    if (fromAssignment && me.role !== "student") {
      error.textContent = "Phải dùng tài khoản học sinh mới vào được bài làm.";
      button.disabled = false;
      button.textContent = label;
      try { await API.post("/api/auth/logout", {}); } catch (err) {}
      return;
    }
    if (fromAssignment && nextPath) {
      sessionStorage.setItem("oes-assignment-ok", nextPath);
    }
    location.href = afterLoginPath(me, roleHome(me.role));
  } catch (err) {
    error.textContent = err.message;
    button.disabled = false;
    button.textContent = label;
  }
};
