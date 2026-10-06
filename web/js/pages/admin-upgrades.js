(async function () {
  const me = await guard(["admin", "partner"]);
  if (!me) return;
  const ui = mount({
    title: "Xét duyệt giáo viên",
    lead: "Tạo mã nâng cấp, hoặc duyệt đơn CCCD. Học sinh dùng mã hoặc gửi CCCD trong Tài khoản.",
    me
  });
  const create = document.createElement("button");
  create.className = "btn primary";
  create.type = "button";
  create.textContent = "Tạo mã nâng cấp";
  create.onclick = () => openCreate();
  ui.actions.appendChild(create);

  function rowHtml(row) {
    return `<tr>
      <td>${esc(row.username)}<div class="muted">${esc(row.fullName)}</div></td>
      <td>${esc(row.cccd)}</td>
      <td>${esc(row.lecturerCode)}<div class="muted">${esc(row.department)} · ${esc(row.faculty)}</div></td>
      <td><button class="btn small" data-approve="${row.id}" type="button">Duyệt</button>
        <button class="btn small" data-reject="${row.id}" type="button">Từ chối</button></td>
    </tr>`;
  }

  function codeRow(row) {
    const left = Math.max(0, row.maxUses - row.usedCount);
    const live = row.status === "active" && left > 0;
    return `<tr>
      <td><strong>${esc(row.code)}</strong><div class="muted">${esc(row.note || "Không ghi chú")}</div></td>
      <td>${row.usedCount}/${row.maxUses}<div class="muted">Còn ${left}</div></td>
      <td>${live ? `<span class="badge ok">Còn dùng</span>` : row.status === "disabled" ? `<span class="badge warn">Đã tắt</span>` : `<span class="badge bad">Hết lượt</span>`}</td>
      <td>${esc(formatTime(row.createdAt))}</td>
      <td>${live ? `<button class="btn small" data-disable="${row.id}" type="button">Tắt mã</button>` : ""}</td>
    </tr>`;
  }

  async function load() {
    const [rows, codes] = await Promise.all([
      API.get("/api/admin/teacher-upgrades"),
      API.get("/api/admin/teacher-codes")
    ]);
    ui.content.innerHTML = `
      <section class="card panel">
        <h2>Mã nâng cấp giáo viên</h2>
        <p class="muted">Đưa mã cho người cần lên giáo viên. Họ nhập mã trong Tài khoản, kèm mã giáo viên, bộ môn và khoa. Dùng mã xong tài khoản đổi ngay, không cần duyệt CCCD.</p>
        <div class="table-wrap"><table class="data">
          <thead><tr><th>Mã</th><th>Lượt dùng</th><th>Trạng thái</th><th>Tạo lúc</th><th></th></tr></thead>
          <tbody>${codes.map(codeRow).join("") || "<tr><td colspan='5'>Chưa có mã. Bấm Tạo mã nâng cấp.</td></tr>"}</tbody>
        </table></div>
      </section>
      <section class="card panel" id="upgrade-box">
        <h2>Đơn CCCD đang chờ</h2>
        <p class="muted">${rows.length ? rows.length + " đơn cần xét." : "Chưa có đơn chờ duyệt."} Số CCCD chỉ hiện ở đây.</p>
        <div class="table-wrap"><table class="data">
          <thead><tr><th>Tài khoản</th><th>CCCD</th><th>Mã giáo viên</th><th></th></tr></thead>
          <tbody>${rows.map(rowHtml).join("") || "<tr><td colspan='4'>Chưa có đơn chờ duyệt</td></tr>"}</tbody>
        </table></div>
      </section>`;
    ui.content.querySelectorAll("[data-approve]").forEach((button) => {
      button.onclick = () => decide(Number(button.dataset.approve), true);
    });
    ui.content.querySelectorAll("[data-reject]").forEach((button) => {
      button.onclick = () => decide(Number(button.dataset.reject), false);
    });
    ui.content.querySelectorAll("[data-disable]").forEach((button) => {
      button.onclick = async () => {
        if (!(await confirmBox("Tắt mã này? Không ai dùng được nữa."))) return;
        try {
          await API.post("/api/admin/teacher-codes/" + button.dataset.disable + "/disable", {});
          toast("Đã tắt mã");
          await load();
        } catch (error) {
          toast(error.message, "bad");
        }
      };
    });
  }

  function openCreate() {
    const body = openModal("Tạo mã nâng cấp", `<form id="code-form" class="stack">
      <label>Mã tự nhập (chữ và số)<input name="code" maxlength="32" placeholder="Ví dụ GV2026A hoặc deptrai01"></label>
      <p class="muted">Điền mã bằng chữ và số nếu muốn tự đặt. Để trống thì hệ thống tạo mã ngẫu nhiên.</p>
      <label>Số mã tạo (khi để trống ô mã)<input name="quantity" type="number" min="1" max="20" value="1"></label>
      <label>Số lần dùng mỗi mã<input name="maxUses" type="number" min="1" max="100" value="1" required></label>
      <label>Ghi chú<input name="note" placeholder="Ví dụ: Gói trường A"></label>
      <button class="btn primary" type="submit">Tạo mã</button>
    </form>`);
    body.querySelector("#code-form").onsubmit = async (event) => {
      event.preventDefault();
      const form = event.target;
      try {
        const made = await API.post("/api/admin/teacher-codes", {
          code: form.code.value.trim(),
          quantity: Number(form.quantity.value || 1),
          maxUses: Number(form.maxUses.value),
          note: form.note.value.trim()
        });
        closeModal();
        openModal("Mã vừa tạo", `<p class="muted">Chép mã này đưa cho người cần nâng cấp.</p>
          <div class="stack">${made.map((row) => `<p><strong>${esc(row.code)}</strong> · dùng ${row.maxUses} lần</p>`).join("")}</div>
          <button class="btn primary" id="copy-codes" type="button">Sao chép mã</button>`);
        document.getElementById("copy-codes").onclick = async () => {
          const text = made.map((row) => row.code).join("\n");
          try {
            if (navigator.clipboard) await navigator.clipboard.writeText(text);
            else {
              const area = document.createElement("textarea");
              area.value = text;
              document.body.appendChild(area);
              area.select();
              document.execCommand("copy");
              area.remove();
            }
            toast("Đã chép mã");
          } catch (error) {
            toast("Không chép được, hãy chọn mã thủ công", "bad");
          }
        };
        await load();
      } catch (error) {
        toast(error.message, "bad");
      }
    };
  }

  async function decide(id, approve) {
    try {
      if (approve) {
        if (!(await confirmBox("Duyệt đơn này và đổi tài khoản thành giáo viên?"))) return;
        await API.post("/api/admin/teacher-upgrades/" + id + "/approve", {});
        toast("Đã duyệt. Người dùng tải lại trang để vào quyền giáo viên");
      } else {
        const body = openModal("Từ chối đơn", `<form id="reject-form" class="stack">
          <label>Lý do<textarea name="note" required></textarea></label>
          <button class="btn primary" type="submit">Từ chối</button>
        </form>`);
        body.querySelector("#reject-form").onsubmit = async (event) => {
          event.preventDefault();
          try {
            await API.post("/api/admin/teacher-upgrades/" + id + "/reject", { note: event.target.note.value.trim() });
            closeModal();
            toast("Đã từ chối đơn");
            await load();
          } catch (error) {
            toast(error.message, "bad");
          }
        };
        return;
      }
      await load();
    } catch (error) {
      toast(error.message, "bad");
    }
  }

  load().catch((error) => { ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`; });
})();
