# Hệ thống thi trắc nghiệm trực tuyến

Máy chủ viết bằng C, dữ liệu nằm trong SQLite (`data/exam.db`, lược đồ ở `sql/schema.sql`). Giao diện là các trang HTML, CSS và JavaScript tách riêng. Không cần cài SQL Server.

## Chạy

1. Cài gcc (MinGW). Mở thư mục dự án.
2. Chạy `build.bat` để biên dịch `build\online_exam.exe`.
3. Chạy `run.bat`. Mở http://127.0.0.1:8080

Tài khoản mẫu:

| Vai trò | Tên đăng nhập | Mật khẩu |
| --- | --- | --- |
| Quản trị viên | admin | Admin@123 |
| Giảng viên Java | gv.anva | Gv@12345 |
| Giảng viên CSDL | gv.thib | Gv@12345 |
| Sinh viên | sv.an | Sv@12345 |

Các sinh viên `sv.binh`, `sv.chi`, `sv.dung`, `sv.em` dùng cùng mật khẩu `Sv@12345`.

## Bốn cơ chế chính

- Trộn câu hỏi và trộn thứ tự đáp án khi sinh viên bắt đầu. Bài làm lưu `QuestionId` và `AnswerId`, không lưu chữ A/B/C/D.
- Đồng hồ lấy từ máy chủ. Trình duyệt chỉ nội suy giữa các lần hỏi giờ, rồi bị máy chủ chỉnh lại.
- Mất mạng: câu trả lời ghi vào Local Storage. Dưới 30 giây thì báo nhẹ, từ 30 giây đến 5 phút thì cảnh báo đang lưu cục bộ, quá 5 phút thì không cộng giờ.
- Hết giờ thì khóa bài và nộp. Nếu đang mất mạng, bài ở trạng thái chờ nộp và được gửi khi có mạng, trong thời gian ân hạn của kỳ thi.

Trang làm bài có công tắc **Mô phỏng mất mạng** để thử mà không cần rút dây mạng. Trong lúc mô phỏng, đồng hồ vẫn chạy theo mốc giờ máy chủ đã nhận.
