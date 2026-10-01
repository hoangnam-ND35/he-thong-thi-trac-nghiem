(async function () {
  const me = await guard(["admin"]);
  if (!me) return;
  const ui = mount({
    title: "Nhật ký hệ thống",
    lead: "Việc đăng nhập, sao lưu, hồ sơ và kỳ thi được ghi lại tại đây.",
    me
  });
  let rows = [];

  function paint() {
    const q = (document.getElementById("audit-q")?.value || "").trim().toLowerCase();
    const action = document.getElementById("audit-action")?.value || "";
    const shown = rows.filter((row) => {
      const blob = `${row.fullName} ${row.username} ${row.detail} ${actionLabel(row.action)} ${row.ip}`.toLowerCase();
      return (!q || blob.includes(q)) && (!action || row.action === action);
    });
    const actions = [...new Set(rows.map((row) => row.action).filter(Boolean))];
    const shell = document.getElementById("audit-table");
    if (!shell) {
      ui.content.innerHTML = `
        <div class="toolbar">
          <div class="filters" style="margin:0">
            <input id="audit-q" placeholder="Tìm người dùng, việc làm, IP" aria-label="Tìm nhật ký">
            <select id="audit-action" aria-label="Loại việc"><option value="">Mọi việc</option>${actions.map((item) => `<option value="${esc(item)}">${esc(actionLabel(item))}</option>`).join("")}</select>
          </div>
          <p class="count-line" id="audit-count"></p>
        </div>
        <div class="card panel table-wrap" id="audit-table"></div>`;
      document.getElementById("audit-q").oninput = paint;
      document.getElementById("audit-action").onchange = paint;
    } else if (!document.getElementById("audit-action").options.length) {
      document.getElementById("audit-action").innerHTML = `<option value="">Mọi việc</option>${actions.map((item) => `<option value="${esc(item)}">${esc(actionLabel(item))}</option>`).join("")}`;
    }
    document.getElementById("audit-count").textContent = shown.length + " / " + rows.length + " dòng";
    document.getElementById("audit-table").innerHTML = `<table class="data">
      <thead><tr><th>Thời điểm</th><th>Người dùng</th><th>Việc đã làm</th><th>Chi tiết</th><th>IP</th></tr></thead>
      <tbody>${shown.map((row) => `<tr><td>${esc(formatTime(row.createdAt))}</td><td>${esc(row.fullName || "Hệ thống")}<div class="muted">${esc(row.username || "")}</div></td><td>${esc(actionLabel(row.action))}</td><td>${esc(row.detail)}</td><td>${esc(row.ip)}</td></tr>`).join("") || "<tr><td colspan='5'>Không có dòng phù hợp</td></tr>"}</tbody>
    </table>`;
  }

  try {
    rows = await API.get("/api/audit");
    paint();
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }
})();
