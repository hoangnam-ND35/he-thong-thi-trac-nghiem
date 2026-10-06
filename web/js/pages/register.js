const form = document.getElementById("register-form");
form.querySelectorAll(".secret").forEach((wrap) => wrap.insertAdjacentHTML("beforeend", eyeButton()));
bindSecretToggles(form);

let options = { faculties: [], classes: [] };

loadBrand().then(applyBrand);
API.get("/api/auth/me").then((me) => { location.href = roleHome(me.role); }).catch(() => {});

function paintLists(facultyFilter) {
  const facultyList = document.getElementById("faculty-list");
  const classList = document.getElementById("class-list");
  const faculties = options.faculties || [];
  const classes = (options.classes || []).filter((row) => !facultyFilter || row.faculty === facultyFilter);
  facultyList.innerHTML = faculties.map((name) => `<option value="${esc(name)}"></option>`).join("");
  classList.innerHTML = classes.map((row) =>
    `<option value="${esc(row.name)}" label="${esc(row.faculty)}"></option>`
  ).join("");
}

function matchClass(name) {
  const needle = String(name || "").trim().toLowerCase();
  if (!needle) return null;
  const exact = (options.classes || []).filter((row) => row.name.toLowerCase() === needle);
  if (exact.length === 1) return exact[0];
  if (exact.length > 1) {
    const withFaculty = exact.find((row) => row.faculty === form.faculty.value.trim());
    return withFaculty || exact[0];
  }
  const starts = (options.classes || []).filter((row) => row.name.toLowerCase().startsWith(needle));
  return starts.length === 1 ? starts[0] : null;
}

function autofillFromClass() {
  const hit = matchClass(form.className.value);
  if (!hit) return;
  if (form.className.value.trim() !== hit.name) form.className.value = hit.name;
  if (hit.faculty) form.faculty.value = hit.faculty;
  paintLists(hit.faculty);
}

function onFacultyChange() {
  const faculty = form.faculty.value.trim();
  paintLists(faculty);
  const current = form.className.value.trim();
  if (!current) return;
  const ok = (options.classes || []).some((row) => row.name === current && (!faculty || row.faculty === faculty));
  if (!ok) {
    const hit = matchClass(current);
    if (hit && (!faculty || hit.faculty === faculty)) form.className.value = hit.name;
  }
}

form.className.addEventListener("change", autofillFromClass);
form.className.addEventListener("blur", autofillFromClass);
form.className.addEventListener("input", () => {
  const hit = matchClass(form.className.value);
  if (hit && hit.name.toLowerCase() === form.className.value.trim().toLowerCase()) {
    form.faculty.value = hit.faculty;
    paintLists(hit.faculty);
  }
});
form.faculty.addEventListener("change", onFacultyChange);
form.faculty.addEventListener("input", onFacultyChange);

API.get("/api/auth/register-options").then((data) => {
  options = data;
  paintLists("");
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
  autofillFromClass();
  const faculty = form.faculty.value.trim();
  const className = form.className.value.trim();
  if ((faculty || className) && !(options.classes || []).some((row) => row.name === className && row.faculty === faculty)) {
    error.textContent = "Khoa hoặc lớp chưa khớp danh mục. Gõ/chọn đúng lớp để tự điền khoa.";
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
      faculty,
      className
    });
    location.href = roleHome(me.role);
  } catch (err) {
    error.textContent = err.message;
    button.disabled = false;
    button.textContent = label;
  }
};
