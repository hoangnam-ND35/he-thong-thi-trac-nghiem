# Đưa web lên internet (giống mở được trên Chrome + điện thoại)

Trang [Chợ Nhà Smart Food](https://hoangnam-nd35.github.io/chonha-smart-food/) chạy được trên GitHub Pages vì chỉ là HTML tĩnh.

**Phòng thi trực tuyến** có máy chủ C + SQLite, nên **không chạy trên GitHub Pages**. Cần host kiểu Render (miễn phí).

## Cách deploy (1 lần)

1. Đăng nhập [https://dashboard.render.com](https://dashboard.render.com) bằng tài khoản GitHub (`hoangnam-ND35`).
2. **New +** → **Blueprint** → chọn repo `he-thong-thi-trac-nghiem`.
3. Render đọc file `render.yaml` và tạo web service Docker.
4. Đợi build xong (vài phút). Link dạng:
   `https://phong-thi-truc-tuyen.onrender.com`
5. Mở link đó trên Chrome hoặc điện thoại (4G cũng được).

Hoặc: **New +** → **Web Service** → Connect repo → Runtime **Docker** → Create.

## Sau khi có link

- Xuất **Giao bài** trên server online → link sẽ dùng domain Render (không còn `127.0.0.1`).
- Lần đầu mở có thể chờ 30–60 giây (gói free “ngủ” khi không ai vào, rồi tự thức).

## Tài khoản mẫu

| Vai trò | Tên | Mật khẩu |
| --- | --- | --- |
| Admin | admin | Admin@123 |
| Học sinh | sv.an | Sv@12345 |
