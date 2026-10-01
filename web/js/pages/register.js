const form = document.getElementById("register-form");
form.querySelectorAll(".secret").forEach((wrap) => wrap.insertAdjacentHTML("beforeend", eyeButton()));
bindSecretToggles(form);

let options = { faculties: [], classes: [] };

loadBrand().then(applyBrand);
API.get("/api/auth/me").then((me) => { location.href = roleHome(me.role); }).catch(() => {});

function fill(select, values, placeholder) {
  select.innerHTML = `<option value="">${esc(placeholder)}</option>` +
    values.map((value) => `<option value="${esc(value)}">${esc(value)}</option>`).join("");
}

form.faculty.onchange = () => {
  const rows = options.classes.filter((row) => row.faculty === form.faculty.value).map((row) => row.name);
  fill(form.className, rows, rows.length ? "Chọn lớp" : "Khoa này chưa có lớp");
};

API.get("/api/auth/register-options").then((data) => {
  options = data;
  const names = data.faculties || [];
  fill(form.faculty, names, names.length ? "Chọn khoa" : "Chưa có khoa");
  fill(form.className, [], "Chọn khoa trước");
}).catch((error) => {
  document.getElementById("register-error").textContent = error.message;
});

form.onsubmit = async (event) => {
  event.preventDefault();
  const error = document.getElementById("register-error");
  const button = form.querySelector("[type=submit]");
  error.textContent = "";
  if (form.password.value !== form.confirm.value) {
    error.textContent = "Mật khẩu nhập lại chưa khớp";
    return;
  }
  button.disabled = true;
  const label = button.textContent;
  button.textContent = "Đang tạo...";
  try {
    const me = await API.post("/api/auth/register", {
      role: "student",
      username: form.username.value.trim(),
      password: form.password.value,
      fullName: form.fullName.value.trim(),
      email: form.email.value.trim(),
      phone: form.phone.value.trim(),
      studentCode: form.studentCode.value.trim(),
      dob: form.dob.value,
      gender: form.gender.value,
      faculty: form.faculty.value,
      className: form.className.value
    });
    location.href = roleHome(me.role);
  } catch (err) {
    error.textContent = err.message;
    button.disabled = false;
    button.textContent = label;
  }
};
