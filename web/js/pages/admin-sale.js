(async function () {
  const me = await guard(["admin", "partner"]);
  if (!me) return;
  const ui = mount({
    title: "Bán cho trường",
    lead: "Chọn chức năng, thanh toán, rồi nhận sản phẩm gói đã bán.",
    me
  });
  const items = [
    ["exam", "Kỳ thi trực tuyến", "Học sinh vào phòng thi, trộn đề và tính giờ.", 8000000],
    ["question", "Ngân hàng câu hỏi", "Giáo viên soạn và nhập câu hỏi.", 4000000],
    ["result", "Kết quả và thống kê", "Giáo viên xem điểm của cả lớp.", 3000000],
    ["upgrade", "Xét duyệt giáo viên", "Học sinh gửi CCCD, nhà trường duyệt lên giáo viên.", 2000000]
  ];
  const names = Object.fromEntries(items.map(([key, title]) => [key, title]));

  function vnd(value) {
    return Number(value || 0).toLocaleString("vi-VN") + " đ";
  }

  function selectedTotal(form) {
    return items.reduce((sum, [key, , , price]) => sum + (form[key].checked ? price : 0), 0);
  }

  function productCard(order, fresh) {
    const bits = ["exam", "question", "result", "upgrade"].filter((key) => order[key]).map((key) => names[key]);
    const pay = order.method === "cash" ? "Tiền mặt" : "Chuyển khoản";
    return `<article class="card panel">
      <h2>${fresh ? "Sản phẩm vừa thanh toán" : esc(order.code)}</h2>
      <p class="note">Đã thanh toán · ${esc(pay)} · ${esc(formatTime(order.createdAt))}</p>
      <dl class="facts">
        <dt>Mã sản phẩm</dt><dd>${esc(order.code)}</dd>
        <dt>Trường</dt><dd>${esc(order.schoolName)}</dd>
        <dt>Đơn vị mua</dt><dd>${esc(order.buyerName)}</dd>
        <dt>Liên hệ</dt><dd>${esc(order.contact)}</dd>
        <dt>Gói gồm</dt><dd>${esc(bits.join(", ") || "—")}</dd>
        <dt>Số tiền</dt><dd><strong>${esc(vnd(order.amount))}</strong></dd>
      </dl>
    </article>`;
  }

  function paint(brand, orders, fresh) {
    ui.content.innerHTML = `
      ${fresh ? productCard(fresh, true) : ""}
      <section class="card panel">
        <h2>Chọn gói và thanh toán</h2>
        <p class="muted">Sản phẩm giao cho <strong>${esc(brand.schoolName)}</strong>. Hệ thống tự tính tiền theo chức năng đã chọn. Không lưu số thẻ.</p>
        <form id="sale-form" class="stack">
          ${items.map(([key, title, note, price]) => `<label class="package-option"><input type="checkbox" name="${key}" checked><span class="package-text"><strong>${title}</strong> · ${vnd(price)}<br><span class="muted">${note}</span></span></label>`).join("")}
          <p id="sale-total"><strong>Tổng thanh toán: ${vnd(items.reduce((sum, row) => sum + row[3], 0))}</strong></p>
          <label>Đơn vị mua<input name="buyerName" required placeholder="Tên trường hoặc trung tâm"></label>
          <label>Liên hệ<input name="contact" required placeholder="Email hoặc số điện thoại"></label>
          <label>Hình thức<select name="method"><option value="transfer">Chuyển khoản</option><option value="cash">Tiền mặt</option></select></label>
          <button class="btn primary" type="submit">Thanh toán và nhận sản phẩm</button>
        </form>
      </section>
      <section class="card panel">
        <h2>Sản phẩm gói đã bán</h2>
        ${orders.length ? orders.map((order) => productCard(order, false)).join("") : `<p class="muted">Chưa có sản phẩm. Thanh toán xong, gói hiện ở đây.</p>`}
      </section>`;
    const form = ui.content.querySelector("#sale-form");
    const total = ui.content.querySelector("#sale-total");
    const refreshTotal = () => { total.innerHTML = `<strong>Tổng thanh toán: ${esc(vnd(selectedTotal(form)))}</strong>`; };
    form.querySelectorAll("input[type=checkbox]").forEach((box) => { box.onchange = refreshTotal; });
    form.onsubmit = async (event) => {
      event.preventDefault();
      const button = form.querySelector("button");
      button.disabled = true;
      try {
        const sold = await API.post("/api/partner/orders", {
          buyerName: form.buyerName.value.trim(),
          contact: form.contact.value.trim(),
          method: form.method.value,
          exam: form.exam.checked,
          question: form.question.checked,
          result: form.result.checked,
          upgrade: form.upgrade.checked
        });
        toast("Đã thanh toán. Mã sản phẩm " + sold.code);
        const next = await API.get("/api/partner/orders");
        paint(brand, next, sold);
      } catch (error) {
        toast(error.message, "bad");
        button.disabled = false;
      }
    };
  }

  try {
    const [brand, orders] = await Promise.all([API.get("/api/brand"), API.get("/api/partner/orders")]);
    paint(brand, orders, null);
  } catch (error) {
    ui.content.innerHTML = `<p class="note bad">${esc(error.message)}</p>`;
  }
})();
