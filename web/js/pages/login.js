const form = document.getElementById("login-form");
bindSecretToggles(form);
loadBrand().then(applyBrand);

const nextPath = safeNextPath(new URLSearchParams(location.search).get("next") || "");
API.get("/api/auth/me").then((me) => {
  location.href = afterLoginPath(me, roleHome(me.role));
}).catch(() => {});

if (nextPath) {
  const lead = form.querySelector(".muted");
  if (lead) lead.textContent = "Đăng nhập xong sẽ vào thẳng bài làm hoặc trang bạn vừa mở.";
}

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
    location.href = afterLoginPath(me, roleHome(me.role));
  } catch (err) {
    error.textContent = err.message;
    button.disabled = false;
    button.textContent = label;
  }
};
