(async function () {
  const me = await guard(["admin"]);
  if (!me) return;
  const ui = mount({
    title: "Tổng quan",
    lead: "Theo dõi hồ sơ, học vụ và tình trạng máy chủ tại một chỗ.",
    active: "/admin/dashboard.html",
    me
  });
  const backup = document.createElement("button");
  backup.className = "btn";
  backup.type = "button";
  backup.textContent = "Sao lưu ngay";
  ui.actions.appendChild(backup);

  function uptimeText(seconds) {
    const total = Math.max(0, Number(seconds) || 0);
    const hour = Math.floor(total / 3600);
    const minute = Math.floor((total % 3600) / 60);
    if (hour > 0) return hour + " giờ " + minute + " phút";
    return minute + " phút";
  }

  function opsHtml(ops) {
    const serving = ops.status === "up";
    return `<section class="ops">
      <article class="card">
        <span>Bảo mật</span>
        <b>Đang áp dụng</b>
        <p>Mật khẩu được băm, phiên HttpOnly, giới hạn khi đăng nhập hoặc đăng ký quá nhiều lần.</p>
      </article>
      <article class="card">
        <span>Hiệu suất</span>
        <b>${formatScore(ops.avgHandleMs)} ms</b>
        <p>${ops.requests} yêu cầu đã xử lý. Trang tĩnh được giữ trong bộ nhớ đến khi file đổi.</p>
      </article>
      <article class="card">
        <span>Khả dụng</span>
        <b>Sẵn sàng dùng</b>
        <p>Biểu mẫu có nhãn, lối tắt tới nội dung, nút hiện mật khẩu và trạng thái khi đang gửi.</p>
      </article>
      <article class="card">
        <span>Sẵn có</span>
        <b>${serving ? "Đang phục vụ" : "Đang gián đoạn"}</b>
        <p>Đã chạy ${uptimeText(ops.uptimeSec)}. Sao lưu ${ops.lastBackupAt ? formatTime(ops.lastBackupAt) : "chưa có"}, giữ ${ops.backupsKept} bản.</p>
      </article>
    </section>`;
  }

  backup.onclick = async () => {
    if (!(await confirmBox("Sao lưu cơ sở dữ liệu ngay bây giờ?"))) return;
    backup.disabled = true;
    backup.textContent = "Đang sao lưu...";
    try {
      const ops = await API.post("/api/admin/backup");
      const box = document.getElementById("ops");
      if (box) box.innerHTML = opsHtml(ops);
      toast("Đã sao lưu dữ liệu");
    } catch (error) {
      toast(error.message, "bad");
    }
    backup.disabled = false;
    backup.textContent = "Sao lưu ngay";
  };

  try {
    const [data, ops] = await Promise.all([API.get("/api/dashboard/admin"), API.get("/api/health")]);
    ui.content.innerHTML = `
      <div class="stats">
        <a class="card stat" href="/admin/profiles.html"><span>Người dùng</span><b>${data.users}</b><small>${data.students} học sinh · ${data.lecturers} giáo viên</small></a>
        <a class="card stat" href="/lecturer/exams.html"><span>Kỳ thi</span><b>${data.exams}</b><small>${data.attempts || 0} lượt làm bài</small></a>
        <a class="card stat" href="/admin/subjects.html"><span>Môn học</span><b>${data.subjects}</b><small>Khoa, bộ môn và lớp</small></a>
        <a class="card stat" href="/lecturer/questions.html"><span>Câu hỏi</span><b>${data.questions}</b><small>Ngân hàng đang dùng</small></a>
      </div>
      <div id="ops">${opsHtml(ops)}</div>
      <div class="grid-2">
        <section class="card panel">
          <h2>Kết quả đã chấm</h2>
          <div class="stats" style="grid-template-columns:repeat(3,1fr)">
            <article><span class="muted">Bài đã chấm</span><b style="font-family:var(--serif);font-size:28px">${data.graded}</b></article>
            <article><span class="muted">Điểm trung bình</span><b style="font-family:var(--serif);font-size:28px">${formatScore(data.averagePercent)}%</b></article>
            <article><span class="muted">Cao / thấp</span><b style="font-family:var(--serif);font-size:28px">${formatScore(data.highestPercent)} / ${formatScore(data.lowestPercent)}</b></article>
          </div>
        </section>
        <section class="card panel">
          <h2>Hoạt động gần đây</h2>
          <div class="table-wrap"><table class="data">
            <thead><tr><th>Người dùng</th><th>Việc đã làm</th><th>Thời điểm</th></tr></thead>
            <tbody>${(data.recent || []).map((row) => `<tr><td>${esc(row.fullName || "Hệ thống")}</td><td>${esc(actionLabel(row.action))}</td><td>${esc(formatTime(row.createdAt))}</td></tr>`).join("") || "<tr><td colspan='3'>Chưa có nhật ký</td></tr>"}</tbody>
          </table></div>
          <p class="count-line" style="margin-top:10px"><a href="/admin/audit.html">Xem toàn bộ nhật ký</a></p>
        </section>
      </div>`;
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }
})();
