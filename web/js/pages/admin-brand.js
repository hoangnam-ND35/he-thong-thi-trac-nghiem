(async function () {
  const me = await guard(["admin"]);
  if (!me) return;
  const ui = mount({
    title: "Nhận diện trường",
    lead: "Đổi tên, logo và màu để giao mẫu này cho một đại học, cao đẳng, trường phổ thông hoặc trung tâm.",
    me
  });
  const levels = [
    ["university", "Đại học"],
    ["college", "Cao đẳng"],
    ["school", "Phổ thông"],
    ["center", "Trung tâm"]
  ];
  const themes = [
    ["navy", "#14283a"],
    ["forest", "#143028"],
    ["wine", "#3a1824"],
    ["teal", "#12343a"]
  ];
  try {
    const brand = await API.get("/api/brand");
    ui.content.innerHTML = `
      <div class="grid-2">
        <section class="card panel">
          <h2>Thông tin giao cho trường</h2>
          <form id="brand-form" class="stack">
            <label>Tên trường<input name="schoolName" required maxlength="140" value="${esc(brand.schoolName)}"></label>
            <div class="filters">
              <label>Tên viết tắt<input name="schoolShort" required maxlength="16" value="${esc(brand.schoolShort)}"></label>
              <label>Cấp<select name="level">${levels.map(([value, label]) => `<option value="${value}" ${brand.level === value ? "selected" : ""}>${label}</option>`).join("")}</select></label>
            </div>
            <label>Khẩu hiệu<textarea name="motto" maxlength="180">${esc(brand.motto || "")}</textarea></label>
            <label>Địa chỉ<input name="address" maxlength="180" value="${esc(brand.address || "")}"></label>
            <div class="filters">
              <label>Điện thoại<input name="phone" maxlength="32" value="${esc(brand.phone || "")}"></label>
              <label>Email<input name="email" type="email" value="${esc(brand.email || "")}"></label>
            </div>
            <label>Website<input name="website" placeholder="https://" value="${esc(brand.website || "")}"></label>
            <div>
              <span>Màu mẫu</span>
              <div class="theme-pick" id="themes">${themes.map(([value, color]) => `<button type="button" data-theme="${value}" style="background:${color}" aria-label="${value}" class="${brand.theme === value ? "on" : ""}"></button>`).join("")}</div>
            </div>
            <div class="row-actions">
              <button class="btn" id="pick-logo" type="button">Chọn logo</button>
              <button class="btn ghost" id="clear-logo" type="button">Bỏ logo</button>
              <input id="logo-file" type="file" accept="image/png,image/jpeg,image/webp" hidden>
            </div>
            <button class="btn primary" type="submit">Lưu mẫu trường</button>
          </form>
        </section>
        <section class="card panel">
          <h2>Xem trước cổng đăng nhập</h2>
          <div class="preview-hero" id="preview">
            <div class="seal" id="preview-seal">THI</div>
            <p class="brand-kicker" id="preview-level"></p>
            <h2 id="preview-name" style="font-family:var(--serif);font-weight:600"></h2>
            <p id="preview-motto"></p>
            <p id="preview-foot" class="muted"></p>
          </div>
          <p class="muted">Cấp trường đổi cách gọi: đại học và cao đẳng dùng sinh viên, phổ thông dùng học sinh, trung tâm dùng học viên.</p>
        </section>
      </div>`;
    const form = document.getElementById("brand-form");
    let logo = brand.logo || "";
    let theme = brand.theme || "navy";
    const preview = () => {
      const level = levels.find((row) => row[0] === form.level.value) || levels[0];
      const sample = {
        schoolName: form.schoolName.value.trim() || "Phòng thi trực tuyến",
        schoolShort: form.schoolShort.value.trim() || "THI",
        levelLabel: level[1],
        motto: form.motto.value.trim(),
        address: form.address.value.trim(),
        phone: form.phone.value.trim(),
        email: form.email.value.trim(),
        theme,
        logo
      };
      document.body.dataset.theme = theme;
      paintSeal(document.getElementById("preview-seal"), sample);
      paintSeal(document.querySelector(".brand .seal"), sample);
      document.getElementById("preview-level").textContent = sample.levelLabel;
      document.getElementById("preview-name").textContent = sample.schoolName;
      document.getElementById("preview-motto").textContent = sample.motto;
      document.getElementById("preview-foot").textContent = [sample.address, sample.phone, sample.email].filter(Boolean).join(" · ");
      const sideName = document.querySelector(".brand strong");
      const sideNote = document.querySelector(".brand span");
      if (sideName) sideName.textContent = sample.schoolName;
      if (sideNote) sideNote.textContent = sample.levelLabel;
    };
    form.oninput = preview;
    document.getElementById("themes").onclick = (event) => {
      const button = event.target.closest("[data-theme]");
      if (!button) return;
      theme = button.dataset.theme;
      document.querySelectorAll("#themes button").forEach((item) => item.classList.toggle("on", item === button));
      preview();
    };
    document.getElementById("pick-logo").onclick = () => document.getElementById("logo-file").click();
    document.getElementById("clear-logo").onclick = () => { logo = ""; preview(); };
    document.getElementById("logo-file").onchange = () => {
      const file = document.getElementById("logo-file").files[0];
      document.getElementById("logo-file").value = "";
      if (!file) return;
      const reader = new FileReader();
      reader.onload = () => {
        const image = new Image();
        image.onload = () => {
          const size = 256;
          const canvas = document.createElement("canvas");
          canvas.width = size;
          canvas.height = size;
          const context = canvas.getContext("2d");
          const scale = Math.max(size / image.width, size / image.height);
          const width = image.width * scale;
          const height = image.height * scale;
          context.drawImage(image, (size - width) / 2, (size - height) / 2, width, height);
          logo = canvas.toDataURL("image/jpeg", 0.8);
          preview();
        };
        image.onerror = () => toast("Không đọc được ảnh", "bad");
        image.src = reader.result;
      };
      reader.readAsDataURL(file);
    };
    form.onsubmit = async (event) => {
      event.preventDefault();
      const button = form.querySelector("[type=submit]");
      button.disabled = true;
      try {
        const saved = await API.put("/api/admin/brand", {
          schoolName: form.schoolName.value.trim(),
          schoolShort: form.schoolShort.value.trim(),
          level: form.level.value,
          motto: form.motto.value.trim(),
          address: form.address.value.trim(),
          phone: form.phone.value.trim(),
          email: form.email.value.trim(),
          website: form.website.value.trim(),
          theme,
          logo
        });
        applyBrand(saved);
        toast("Đã lưu mẫu cho " + saved.schoolName);
      } catch (error) {
        toast(error.message, "bad");
      }
      button.disabled = false;
    };
    preview();
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }
})();
