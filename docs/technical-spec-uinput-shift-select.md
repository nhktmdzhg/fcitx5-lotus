# Tài Liệu Kỹ Thuật Chuyên Sâu: Cơ Chế Shift+Left Selection Rewrite (`UinputShiftSelect`)

Tài liệu đặc tả kỹ thuật nội bộ (Technical Specification & Architecture) dành cho lập trình viên bảo trì và phát triển **Fcitx5 Lotus**. Tài liệu này mô tả chi tiết kiến trúc luồng dữ liệu, máy trạng thái, giao thức IPC, cơ chế đồng bộ hóa tuần tự và chiến lược phòng chống race condition.

---

## 1. Tổng Quan Kiến Trúc (Architecture Overview)

Chế độ `UinputShiftSelect` được thiết kế nhằm thay thế cơ chế xóa bằng `KEY_BACKSPACE` truyền thống bằng phương pháp **bôi đen vùng chọn và ghi đè (Selection-and-Overwrite)**.

### 1.1. Các thành phần tham gia

```
┌─────────────────────────────────────────────────────────────────┐
│                     Fcitx5 Process (User Space)                 │
│                                                                 │
│  [KeyEvent In] ──► [LotusState::keyEvent]                       │
│                           │                                     │
│       ┌───────────────────┴───────────────────┐                 │
│       ▼                                       ▼                 │
│  [is_deleting_ == true]              [is_deleting_ == false]    │
│       │                                       │                 │
│  [Push to buffered_keys_]            [EngineProcessKeyEvent]    │
│  [filterAndAccept()]                          │                 │
│                                      [compareAndSplitStrings]   │
│                                               │                 │
│                                      [performShiftSelect]       │
│                                               │                 │
│                                      (send ShiftSelectMsg)      │
│                                               │                 │
└───────────────────────────────────────────────┼─────────────────┘
                                                │ Unix Domain Socket
                                                │ (/tmp/fcitx5-lotus.sock)
┌───────────────────────────────────────────────┼─────────────────┐
│  fcitx5-lotus-server Daemon (Root / Input)    │                 │
│                                               ▼                 │
│                       [Server recv ShiftSelectMsg]              │
│                                   │                             │
│                      [KEY_LEFTSHIFT Down]                       │
│                      [Delay 2ms]                                │
│                      [KEY_LEFT x N (2ms/phím)]                  │
│                      [Delay 2ms]                                │
│                      [KEY_LEFTSHIFT Up]                         │
│                                   │                             │
│                      [Send ACK 'D' to socket] ──────────────────┼──┐
└─────────────────────────────────────────────────────────────────┘  │
                                                                     │
┌─────────────────────────────────────────────────────────────────┐  │
│  Fcitx5 EventLoop (IO Callback & Timers)                        │  │
│                                                                 │  │
│  [shiftSelectAckListener_] ◄────────────────────────────────────┴──┘
│            │
│            ▼
│  [onShiftSelectDone()] ──► [Commit First Char]
│                                   │
│                            [Delay 5ms via EventLoop]
│                                   │
│                            [Commit Rest Chars]
│                                   │
│                            [Delay 20ms Barrier]
│                                   │
│                            [is_deleting_ = false]
│                                   │
│                            [replayBufferedKeys()] ──► [Tuần tự hóa từng key]
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Giao Thức Giao Tiếp IPC (Socket Protocol)

- **Kênh truyền**: Unix Domain Stream Socket (`/tmp/fcitx5-lotus.sock`).
- **Phía Server**: Chạy quyền `root` để ghi vào `/dev/uinput`.
- **Phía Client**: Chạy trong ngữ cảnh người dùng của process `fcitx5`.

### 2.1. Cấu trúc thông điệp yêu cầu (`ShiftSelectMsg`)

Định nghĩa tại `src/lotus-state.h`:

```cpp
#pragma pack(push, 1)
struct ShiftSelectMsg {
    uint8_t type;    // Luôn bằng 1 (Đại diện cho lệnh Shift-Select)
    int32_t nLeft;   // Số lượng ký tự cần lùi (số lần nhấn phím KEY_LEFT)
};
#pragma pack(pop)
```

- `type`: Định danh tác vụ. `1` = Bôi đen ngược về bên trái `nLeft` ký tự.
- `nLeft`: Số codepoint UTF-8 cần chọn lùi, tính bằng `utf8::length(deletedPart)`.

### 2.2. Tín hiệu phản hồi (Acknowledgement)

- Server gửi đúng **1 byte duy nhất**: `'D'` (ASCII `0x44` - Done).
- Client không sử dụng hàm `read()` chặn (blocking), mà đăng ký IO Watcher trên `EventLoop` của Fcitx5:
  ```cpp
  el.addIOEvent(fd, IOEventFlag::In, [this, icRef](EventSourceIO*, int, IOEventFlags) -> bool { ... });
  ```

---

## 3. Quản Lý Trạng Thái & Bộ Nhớ Chia Sẻ (State & Concurrency)

### 3.1. Các cờ nguyên tử (Atomic Flags)

| Biến nguyên tử | Vị trí định nghĩa | Ý nghĩa kỹ thuật | Memory Order |
| :--- | :--- | :--- | :--- |
| `is_deleting_` | `src/lotus-utils.h` | Đánh dấu hệ thống đang trong chu trình xóa/rewrite hoặc trong rào cản hậu commit. Khi `true`, **tuyệt đối không cho phép phím mới lọt vào engine**. | `std::memory_order_acquire` / `release` |
| `lastCommitTimeUsec_` | `src/lotus-utils.h` | Thời điểm kết thúc lần commit chuỗi gần nhất (tính theo `CLOCK_MONOTONIC` microsecond). Dùng để từ chối các tín hiệu reset giả từ hệ thống. | `std::memory_order_acquire` / `release` |
| `needEngineReset` | `src/lotus-utils.h` | Báo hiệu chuột click từ server (`lotus-monitor`). Bị trì hoãn xử lý nếu `is_deleting_ == true`. | `std::memory_order_acquire` / `release` |

### 3.2. Cấu trúc hàng đợi đệm phím (`buffered_keys_`)

Định nghĩa tại `src/lotus-state.h`:

```cpp
struct BufferedKeyInfo {
    uint32_t sym;    // KeySym của phím (ví dụ: FcitxKey_n)
    uint32_t state;  // Trạng thái tổ hợp phím (Modifier state: Shift, Caps, etc.)
};

std::vector<BufferedKeyInfo> buffered_keys_;
static constexpr size_t MAX_BUFFERED_KEYS = 32;
```

---

## 4. Cơ Chế Thực Thi Chi Tiết (Implementation Details)

### 4.1. Khởi động Rewrite: `performShiftSelectReplacement`

Được kích hoạt khi bộ gõ phát hiện chuỗi mới khác chuỗi cũ và có phần cần xóa (`deletedPart` không rỗng):

```cpp
void LotusState::performShiftSelectReplacement(const std::string& deletedPart, const std::string& addedPart);
```

1. **Kiểm tra `nLeft`**:
   - Nếu `nLeft <= 0`: Không có ký tự nào cần bôi đen xóa, trực tiếp gọi `ic_->commitString(addedPart)`, bật `is_deleting_ = true` và hẹn giờ barrier 20ms.
2. **Tách chuỗi thêm mới (`addedPart`)**:
   - Tách thành ký tự UTF-8 đầu tiên (`shiftSelectFirstChar_`) và phần còn lại (`shiftSelectRestChars_`).
   - Việc tách này đảm bảo ký tự đầu tiên sẽ ghi đè và xóa sạch vùng đang chọn (highlighted text).
3. **Bật rào cản**:
   - `is_deleting_.store(true, std::memory_order_release)`.
4. **Gửi lệnh IPC**:
   - Gửi `ShiftSelectMsg{1, nLeft}` tới socket server.
5. **Đăng ký IO Event phi chặn**:
   - Sử dụng `EventLoop::addIOEvent` để theo dõi socket FD.
   - Nhận `'D'` -> hủy IO watcher -> gọi `onShiftSelectDone()`.

### 4.2. Tiếp nhận ACK & Commit: `onShiftSelectDone`

```cpp
void LotusState::onShiftSelectDone();
```

Quy trình commit 2 giai đoạn:
1. **Commit First Char**: Sau thời gian `uinputShiftSelectDelayMs` (mặc định 0ms), commit `shiftSelectFirstChar_`. Ký tự này ngay lập tức xóa vùng chọn trên UI ứng dụng.
2. **Commit Rest Chars**: Nếu `shiftSelectRestChars_` không rỗng, hẹn giờ sau **5ms** commit tiếp chuỗi còn lại.
3. **Post-Commit Barrier (20ms)**:
   - Cả 2 nhánh đều kết thúc bằng:
     ```cpp
     scheduleShiftSelect(20, [this]() {
         is_deleting_.store(false);
         replayBufferedKeys();
     });
     ```
   - Trong suốt 20ms này, `is_deleting_` vẫn là `true`.

### 4.3. Timer An Toàn Phi Chặn: `scheduleShiftSelect`

```cpp
void LotusState::scheduleShiftSelect(uint32_t delayMs, std::function<void()> callback);
```

- Tuyệt đối không dùng `sleep` hay `this_thread::sleep_for` trên main thread của Fcitx5 (tránh treo toàn bộ hệ thống nhập liệu).
- Sử dụng `EventLoop::addTimeEvent(CLOCK_MONOTONIC, ...)` của Fcitx5.
- Cơ chế tự giải phóng an toàn trước khi gọi callback:
  ```cpp
  auto completed = std::move(shiftSelectTimer_);
  cb();
  ```
  Ngăn chặn việc callback gọi tiếp `scheduleShiftSelect` dẫn đến việc timer tự hủy chính nó trong khi callback đang chạy.

### 4.4. Cơ Chế Tuần Tự Hóa Hàng Đợi: `replayBufferedKeys`

```cpp
void LotusState::replayBufferedKeys();
```

Khi người dùng gõ chuỗi phím cực nhanh (ví dụ: `k h o o n g` với phím `o` và `n` sát nhau):
1. Phím `n` và `g` đến khi `is_deleting_ == true` -> Được gom vào `buffered_keys_`.
2. Khi barrier 20ms của `ô` hết hạn, `replayBufferedKeys()` được gọi.
3. **Nguyên tắc "One Key at a Time"**:
   - Lấy phím đầu tiên: `keyInfo = buffered_keys_.front()`, `buffered_keys_.erase(...)`.
   - Bơm vào engine qua `EngineProcessKeyEvent`.
   - Nếu sinh ra chuỗi cần commit (kể cả ký tự mới không có thay thế):
     - Gọi `ic_->commitString(...)`.
     - Cập nhật `lastCommitTimeUsec_`.
     - **Tái kích hoạt cờ bận**: `is_deleting_.store(true, std::memory_order_release)`.
     - **Hẹn giờ 20ms tiếp theo**:
       ```cpp
       scheduleShiftSelect(20, [this]() {
           is_deleting_.store(false);
           replayBufferedKeys();
       });
       ```
     - Kết thúc hàm (`return`).
4. **Kết quả**: Mỗi ký tự trong buffer được phân tách cách nhau chính xác ít nhất 20ms, đảm bảo thứ tự commit trên ứng dụng luôn đạt 100% độ chính xác (`khô` -> đợi 20ms -> `khôn` -> đợi 20ms -> `không`).

---

## 5. Chiến Lược Chống Reset (Anti-Reset Strategy)

Một trong những nguyên nhân lớn nhất làm hỏng quá trình gõ tiếng Việt là các sự kiện Reset đột ngột từ hệ điều hành hoặc ứng dụng khi nhận được các phím tổng hợp từ uinput (`Shift`, `Left`).

### 5.1. Hàm `shouldRejectReset()` (`src/lotus-utils.cpp`)

```cpp
bool shouldRejectReset() {
    // 1. Đang trong quá trình xóa / rewrite
    if (is_deleting_.load(std::memory_order_acquire)) {
        return true;
    }
    // 2. Nằm trong cửa sổ an toàn 50ms sau commit
    uint64_t last = lastCommitTimeUsec_.load(std::memory_order_acquire);
    if (last > 0) {
        uint64_t now = fcitx::now(CLOCK_MONOTONIC);
        if (now >= last && (now - last) < 50000ULL) {
            return true;
        }
    }
    return false;
}
```

### 5.2. Các điểm chốt chặn (Interception Points)

1. **`LotusEngine::reset()`**:
   ```cpp
   if (shouldRejectReset()) return;
   ```
2. **`LotusState::reset()`**:
   ```cpp
   if (shouldRejectReset()) return;
   ```
3. **`LotusState::clearAllBuffers()`**:
   ```cpp
   if (shouldRejectReset()) return;
   ```
4. **`LotusState::checkForwardSpecialKey()`**:
   Khi các phím `Shift` hoặc `Left` từ uinput dội ngược về frontend của Fcitx5:
   ```cpp
   if (keyEvent.key().isCursorMove() || keyEvent.key().hasModifier()) {
       if (shouldRejectReset()) {
           return true; // Forward phím mà KHÔNG reset buffer engine
       }
   }
   ```
5. **Bỏ qua kiểm tra Backspace ở đầu `keyEvent()`**:
   ```cpp
   if (realMode != LotusMode::UinputShiftSelect && expected_backspaces_ > 0 && 
       current_backspace_count_ >= expected_backspaces_ && is_deleting_.load()) {
       // Chỉ chạy với các mode dùng Backspace
   }
   ```

---

## 6. Tham Số Điều Khiển (Configuration Parameters)

Được cấu hình thông qua file cấu hình Fcitx5 hoặc giao diện cài đặt (`src/lotus-config.h`):

| Tên cấu hình | Kiểu dữ liệu | Mặc định | Ý nghĩa |
| :--- | :--- | :--- | :--- |
| `rewriteMode` | `LotusMode` | `UinputShiftSelect` | Chế độ viết lại văn bản (Enums: Off, SurroundingText, Uinput, Smooth, SuperSmooth, Minecraft, Preedit, Emoji, UinputShiftSelect). |
| `uinputShiftSelectDelayMs` | `int` | `20` ms | Thời gian chờ giữa lúc nhận ACK từ uinput server và lúc commit ký tự đầu tiên đè lên vùng chọn. |
| *Barrier Delay* (Hardcoded) | `uint32_t` | `20` ms | Thời gian rào cản cách ly giữa các lần commit ký tự liên tiếp để ứng dụng kịp đồng bộ buffer đồ họa. |
| *Post-commit Anti-reset* (Hardcoded) | `uint64_t` | `50` ms | Cửa sổ thời gian từ chối mọi yêu cầu reset engine từ hệ thống sau khi commit chuỗi. |

---

## 7. Hướng Dẫn Debug & Kiểm Tra Lỗi

### 7.1. Bật log debug Fcitx5

Chạy Fcitx5 trong terminal để xem log chi tiết thời gian thực:

```bash
G_MESSAGES_DEBUG=all fcitx5 -r --verbose *=5
```

### 7.2. Các log điểm mốc quan trọng (Log Trace)

Một chu kỳ viết lại thành công của `k h o o n g` sẽ có trace log như sau:

```text
[INFO] Perform shift-select replacement: o -> ô
[INFO] Sent shift-select msg: nLeft=1
[WARN] Typing so fast, add key to queue: n   <-- Phím 'n' được bảo vệ trong buffer
[INFO] ShiftSelect done ACK received
[INFO] ShiftSelect commit first: ô
[INFO] Starting replay buffered keys
[INFO] Replay commit added: n                 <-- Phím 'n' được commit sau barrier 20ms
[INFO] Starting replay buffered keys
[INFO] Replay commit added: g                 <-- Phím 'g' được commit sau barrier 20ms
```
