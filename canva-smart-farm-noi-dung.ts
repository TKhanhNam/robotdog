/**
 * Nội dung dán vào Canva Smart Farm — giữ slide 1 nguyên.
 * Template xanh bầu trời; chỉ thay chữ placeholder.
 * Nguồn: Bao_cao_BTNhom3_Smart_Farm_Proteus.docx
 */

export type SlideBlock = {
  slide: number;
  canvaTitle: string;
  keepLayout: string;
  fields: { label: string; text: string }[];
};

export const slides: SlideBlock[] = [
  {
    slide: 1,
    canvaTitle: "Trang bìa",
    keepLayout: "GIỮ NGUYÊN — không sửa",
    fields: [],
  },
  {
    slide: 2,
    canvaTitle: "MARKET OPPORTUNITY → Cơ hội / bối cảnh",
    keepLayout: "2 mục 01 · 02 + ảnh bên phải",
    fields: [
      {
        label: "Tiêu đề",
        text: "CƠ HỘI & BỐI CẢNH",
      },
      {
        label: "01",
        text: "Nông nghiệp chuyển từ kinh nghiệm sang dữ liệu. Theo dõi nhiệt độ và độ ẩm đất giúp tưới đúng lúc, giảm khô hạn / úng nước, nâng năng suất.",
      },
      {
        label: "02",
        text: "Lấy cảm hứng BLKLab Smart Farm. Trên Proteus 8: Arduino Uno + LM35 + biến trở + LED + Buzzer + COMPIM — lớp nền tảng đo – so ngưỡng – cảnh báo.",
      },
    ],
  },
  {
    slide: 3,
    canvaTitle: "PROBLEM STATEMENT → Vấn đề",
    keepLayout: "3 vòng 01 · 02 · 03",
    fields: [
      { label: "Tiêu đề", text: "VẤN ĐỀ ĐẶT RA" },
      {
        label: "01",
        text: "Theo dõi thủ công khó liên tục → cây dễ héo hoặc úng khi môi trường lệch ngưỡng.",
      },
      {
        label: "02",
        text: "Kit ESP32 + DHT11 + OLED khó mô phỏng đầy đủ trên Proteus theo yêu cầu bài.",
      },
      {
        label: "03",
        text: "Cần mô hình đơn giản: cảm biến → xử lý → cảnh báo, dễ giải thích và kiểm thử.",
      },
    ],
  },
  {
    slide: 4,
    canvaTitle: "OUR SOLUTION → Giải pháp",
    keepLayout: "2 cột",
    fields: [
      { label: "Tiêu đề", text: "GIẢI PHÁP CỦA NHÓM" },
      {
        label: "Cột trái",
        text: "Trạm Smart Farm mô phỏng Proteus 8\n• Arduino Uno R3\n• LM35 (nhiệt độ A0)\n• POT giả lập ẩm đất A1\n• LED D8 + Buzzer D9\n• Serial qua COMPIM",
      },
      {
        label: "Cột phải",
        text: "Chức năng chính\n1. Đo nhiệt độ (°C)\n2. Giám sát ẩm đất 0–100%\n3. Ngưỡng: 20–35°C · 30–70%\n4. Cảnh báo khi vượt ngưỡng\n5. In dữ liệu ra Serial",
      },
    ],
  },
  {
    slide: 5,
    canvaTitle: "VALUE PROPOSITION → Luồng giá trị",
    keepLayout: "timeline ngang các bước",
    fields: [
      { label: "Tiêu đề", text: "LUỒNG HOẠT ĐỘNG" },
      { label: "Bước 1", text: "Đọc LM35 & POT (ADC)" },
      { label: "Bước 2", text: "Quy đổi °C và % ẩm đất" },
      { label: "Bước 3", text: "So sánh ngưỡng an toàn" },
      { label: "Bước 4", text: "LED + Buzzer + Serial" },
    ],
  },
  {
    slide: 6,
    canvaTitle: "TARGET AUDIENCE → Đối tượng / ứng dụng",
    keepLayout: "lưới 2×2",
    fields: [
      { label: "Tiêu đề", text: "ĐỐI TƯỢNG & ỨNG DỤNG" },
      {
        label: "Ô 1",
        text: "Nhà kính / vườn rau nhỏ — cảnh báo nóng hoặc đất khô",
      },
      {
        label: "Ô 2",
        text: "Góc trồng trong nhà, ban công — nhắc tưới đúng lúc",
      },
      {
        label: "Ô 3",
        text: "Lab / lớp học — demo nguyên lý IoT nông nghiệp",
      },
      {
        label: "Ô 4",
        text: "Nền tảng mở rộng — bơm tưới, LCD, WiFi / app sau này",
      },
    ],
  },
  {
    slide: 7,
    canvaTitle: "COMPETITIVE ANALYSIS → So sánh",
    keepLayout: "biểu đồ / bảng so sánh",
    fields: [
      { label: "Tiêu đề", text: "SO SÁNH VỚI BẢN GỐC ESP32" },
      {
        label: "Nội dung bảng",
        text: "Hạng mục | Bản gốc ESP32 | Bản nhóm (Proteus)\nMCU | ESP32 | Arduino Uno R3\nNhiệt độ | DHT11 | LM35\nẨm không khí | Có | Không\nẨm đất | Cảm biến thật | Biến trở POT\nMàn hình | OLED | Serial / COMPIM\nTưới / IoT | Relay + Blynk | Chưa (hướng mở rộng)",
      },
    ],
  },
  {
    slide: 8,
    canvaTitle: "MARKETING → Phần cứng & linh kiện",
    keepLayout: "giữ ảnh; thay text thành danh mục linh kiện",
    fields: [
      { label: "Tiêu đề", text: "LINH KIỆN TRÊN PROTEUS" },
      {
        label: "Nội dung",
        text: "• Arduino Uno R3 — xử lý & điều khiển\n• LM35 — cảm biến nhiệt độ\n• POT-HG — giả lập ẩm đất\n• LED-RED + RES 220Ω — báo hiệu\n• Buzzer — cảnh báo âm thanh\n• COMPIM — xem Serial trên PC",
      },
    ],
  },
  {
    slide: 9,
    canvaTitle: "IMPLEMENTATION PLAN → Triển khai",
    keepLayout: "giữ bố cục + ảnh",
    fields: [
      { label: "Tiêu đề", text: "THIẾT KẾ & TRIỂN KHAI" },
      {
        label: "Nội dung",
        text: "Phần cứng: LM35→A0 · POT→A1 · LED→D8 · Buzzer→D9 · COMPIM 9600\n\nPhần mềm: setup → đọc ADC → tính °C/% → so ngưỡng → alarm → Serial → delay 0.8s\n\nKiểm thử: trong ngưỡng tắt báo; >35°C hoặc ẩm <30% / >70% thì LED+Buzzer bật.",
      },
    ],
  },
  {
    slide: 10,
    canvaTitle: "CONCLUSION → Kết luận",
    keepLayout: "2 cột bullets",
    fields: [
      { label: "Tiêu đề", text: "KẾT LUẬN & HƯỚNG PHÁT TRIỂN" },
      {
        label: "Đã làm được",
        text: "• Hoàn thành trạm Smart Farm trên Proteus 8\n• Đo nhiệt độ, giám sát ẩm đất giả lập, cảnh báo vượt ngưỡng\n• Nắm quy trình đo – xử lý – cảnh báo hệ nhúng",
      },
      {
        label: "Tiếp theo",
        text: "• Thêm LCD 16×2 trên Proteus\n• Relay / motor giả lập bơm tưới\n• Chỉnh ngưỡng bằng POT hoặc nút\n• Chuyển kit thật: ESP32 + DHT11 + đất + OLED + Blynk",
      },
    ],
  },
  {
    slide: 11,
    canvaTitle: "THANK YOU",
    keepLayout: "slide cảm ơn — chỉnh ngắn",
    fields: [
      { label: "Dòng chính", text: "CẢM ƠN THẦY CÔ VÀ CÁC BẠN" },
      {
        label: "Dòng phụ",
        text: "Nhóm 3 — Trạm giám sát nông nghiệp thông minh (Smart Farm) · Proteus 8",
      },
    ],
  },
];
