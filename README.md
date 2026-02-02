# Fcitx5 VMK (Optimized Fork)

**Bộ gõ tiếng Việt đơn giản, hiệu năng cao dành cho Fcitx5.**

Dự án này là một bản fork được tối ưu hóa từ bộ gõ VMK gốc.

> **Lưu ý:** Phiên bản này đã loại bỏ công cụ cấu hình cũ viết bằng FLTK. Mọi cấu hình giờ đây được thực hiện trực tiếp qua giao diện chuẩn của Fcitx5 hoặc qua Menu phím tắt mới.

---

## 🚀 Các Cải Tiến Nổi Bật (Changelog)

Bản fork này thay đổi hoàn toàn kiến trúc của Server và Addon để đạt hiệu năng tốt nhất trên Linux hiện đại.

### 1. VMK Server (Backend)

Server (phần mềm chạy ngầm để giả lập phím và theo dõi chuột) đã được viết lại (Refactor) theo phong cách **System Programming**:

- **Kiến trúc Event-Driven (Sử dụng `poll`):**
  - **Cũ:** Dùng `usleep(5000)` để kiểm tra sự kiện liên tục (Polling 200Hz). Tốn CPU đánh thức hệ thống ngay cả khi không làm gì.
  - **Mới:** Chuyển sang cơ chế `poll()` với timeout `-1` ở mọi nơi có thể. Server sẽ "ngủ đông" hoàn toàn khi không có sự kiện. **Mức tiêu thụ CPU khi nhàn rỗi là 0.0%**.

- **Single-Threaded (Đơn luồng):** Loại bỏ hoàn toàn `std::thread`. Gộp chung việc lắng nghe Socket và theo dõi Chuột (Libinput) vào một vòng lặp sự kiện duy nhất. Giảm overhead và dung lượng binary.

- **Phản hồi Thời gian thực (Real-time I/O):**
  - **Cũ:** Ghi file log chuột vào ổ cứng (có delay 1s để tránh hỏng ổ).
  - **Mới:** Sử dụng socket để gửi tín hiệu chuột đến addon, không ghi gì vào file, nhận tín hiệu ngay lập tức.

- **Bảo mật socket:**
  - **Cũ:** File socket có quyền 666, và cả file socket và file mouse flag đều đặt ở thư mục `/home`, bất cứ ai cũng có thể gửi socket nếu biết tên file, cũng như bất cứ ai cũng có thể ghi vào file mouse flag, với phần mềm foss có file tường minh, đây là LỖ HỔNG BẢO MẬT NGHIÊM TRỌNG.
  - **Mới:**
    - Sử dụng `getsockopt` để kiểm tra tên tiến trình gửi socket, và chỉ khi nào đúng tiến trình mới xử lý tiếp, không thể giả mạo tên tiến trình.
    - Không sử dụng file socket như bình thường, mà sử dụng abstract socket, khởi tạo ngay trong kernel, không thể bị chiếm chỗ, không thể bị xóa.

### 2. VMK Addon (Frontend)

Cải thiện trải nghiệm người dùng để tiện lợi hơn khi làm việc đa nhiệm:

- **Per-App Configuration (Cấu hình theo từng App):**
  - Tự động ghi nhớ chế độ gõ (Mode) cho từng ứng dụng riêng biệt.
  - _Ví dụ:_ Tự động tắt bộ gõ khi vào Terminal/Vim, tự bật vmk2 khi vào Chrome.

- **Menu Phím Tắt Thông Minh (`` ` ``):**
  - Nhấn `` ` `` (dấu huyền) để mở menu chọn nhanh chế độ ngay tại con trỏ văn bản chuẩn UI Fcitx5.
  - Trạng thái hiện tại của App được đánh dấu rõ ràng trong danh sách chọn.

---

## 📦 Cài đặt

### Arch Linux / Arch-based distro (systemd) (AUR)

Hiện tại AUR đã có đầy đủ 3 gói cài đặt:

| Gói              | Mô tả                      |
| ---------------- | -------------------------- |
| `fcitx5-vmk`     | Build từ tag release       |
| `fcitx5-vmk-bin` | Prebuilt binary            |
| `fcitx5-vmk-git` | Build theo commit mới nhất |

```bash
# Sử dụng yay
yay -S fcitx5-vmk
yay -S fcitx5-vmk-bin
yay -S fcitx5-vmk-git

# Hoặc sử dụng paru
paru -S fcitx5-vmk
paru -S fcitx5-vmk-bin
paru -S fcitx5-vmk-git
```

### NixOS

Thêm input của fcitx5-vmk vào `flake.nix`:

```
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    fcitx5-vmk = {
      url = "github:nhktmdzhg/VMK";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
  ...
}
```

Bật fcitx5-vmk service trong `configuration.nix`:

```
{
  inputs,
  ...
}: {
  imports = [
    inputs.fcitx5-vmk.nixosModules.fcitx5-vmk
  ];

  services.fcitx5-vmk = {
    enable = true;
    user = "your_username"; # Sửa thành tên user của bạn
  };
}
```

Rebuild lại system để cài đặt.

### Các Distro khác (Ubuntu/Fedora/Debian/openSUSE) và Arch Linux/Arch-based distro (systemd)

Bạn có thể cài đặt fcitx5-vmk thông qua Open Build Service (OBS), nơi cung cấp các package đã được biên dịch sẵn cho nhiều distro khác nhau.

#### Cách 1: Cài đặt qua Open Build Service (Khuyên dùng)

Truy cập trang [Open Build Service](https://software.opensuse.org//download.html?project=home%3Aiamnanoka&package=fcitx5-vmk) để xem hướng dẫn cài đặt chi tiết cho distro của bạn.

[![build result](https://build.opensuse.org/projects/home:iamnanoka/packages/fcitx5-vmk/badge.svg?type=percent)](https://build.opensuse.org/package/show/home:iamnanoka/fcitx5-vmk)
[![build result](https://build.opensuse.org/projects/home:iamnanoka/packages/fcitx5-vmk/badge.svg?type=default)](https://build.opensuse.org/package/show/home:iamnanoka/fcitx5-vmk)

> Lưu ý: Arch và Arch-based distro cũng có thể dùng cách cài này.

#### Cách 2: Biên dịch từ mã nguồn (Build from source)

(Không khuyến khích các bạn cài bằng cách này nếu không hiểu rõ cấu trúc thư mục của distro của bạn, vì ta cần set đúng LIBDIR thì vmk mới có thể chạy được)

Nếu bạn muốn biên dịch từ mã nguồn, hãy làm theo các bước sau:

##### Yêu cầu hệ thống

```bash
# Ubuntu/Debian
sudo apt-get install cmake extra-cmake-modules libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev libinput-dev libudev-dev g++ golang hicolor-icon-theme pkg-config libx11-dev

# Fedora/RHEL
sudo dnf install cmake extra-cmake-modules fcitx5-devel libinput-devel libudev-devel gcc-c++ golang hicolor-icon-theme systemd-devel libX11-devel

# openSUSE
sudo zypper install cmake extra-cmake-modules fcitx5-devel libinput-devel systemd-devel gcc-c++ go hicolor-icon-theme systemd-devel libX11-devel udev
```

##### Biên dịch và cài đặt

```bash
# Clone repository
git clone https://github.com/nhktmdzhg/VMK.git
cd VMK

# Biên dịch
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/usr/lib . # Tùy vào distro mà LIBDIR sẽ khác nhau
make

# Cài đặt (cần quyền root)
sudo make install

# Hoặc cài đặt vào thư mục tùy chỉnh
sudo make install PREFIX=/usr/local
```

##### Gỡ cài đặt

```bash
# Gỡ cài đặt
sudo make uninstall

# Hoặc nếu đã cài đặt với PREFIX tùy chỉnh
sudo make uninstall PREFIX=/usr/local
```

---

## ⚙️ Bật Bộ Gõ

Sau khi cài đặt xong, bạn cần thực hiện các bước sau để bật bộ gõ VMK:

### 1. Bật VMK Server

```bash
# Bật và khởi động service
sudo systemctl enable --now fcitx5-vmk-server@$(whoami).service

# Kiểm tra status
systemctl status fcitx5-vmk-server@$(whoami).service
```

Nếu service bị **failed**, hãy chạy lệnh sau để tạo user systemd cần thiết:

```bash
sudo systemd-sysusers
```

Sau đó thử bật lại service:

```bash
sudo systemctl enable --now fcitx5-vmk-server@$(whoami).service
```

### 2. Thoát hoàn toàn IBus (nếu có)

Nếu hệ thống của bạn đang sử dụng IBus, hãy thoát hoàn toàn trước khi chuyển sang Fcitx5:

```bash
# Kill ibus-daemon
killall ibus-daemon
# Hoặc
ibus exit
```

### 3. Thiết lập biến môi trường

Export các biến môi trường sau vào file cấu hình shell của bạn (`~/.bash_profile`, `~/.zprofile`, hoặc `~/.profile`):

```bash
# Thêm vào ~/.bash_profile, ~/.zprofile, hoặc ~/.profile
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
```

### 4. Thêm Fcitx5 vào Autostart

Tùy thuộc vào Desktop Environment/Window Manager và Distro của bạn:

- **GNOME:** GNOME Tweak -> Startup Applications -> Add -> `fcitx5`
- **KDE Plasma:** System Settings -> Startup and Shutdown -> Autostart -> Add... -> Add application... -> `fcitx5`
- **Xfce:** Settings -> Session and Startup -> Application Autostart -> Add -> `fcitx5`
- **i3/Sway:** Thêm `exec fcitx5 -d` vào file cấu hình (`~/.config/i3/config` hoặc `~/.config/sway/config`)
- **Hyprland:** Thêm `exec-once = fcitx5 -d` vào `~/.config/hypr/hyprland.conf`

> **Lưu ý:** Hãy xóa autostart của IBus nếu có (thường là `ibus-daemon` hoặc `ibus`), hoặc tốt nhất là gỡ luôn ibus ra khỏi máy cho nó khỏe người.

### 5. Log out / Login

Để các thay đổi có hiệu lực, bạn cần log out và login lại vào hệ thống.

### 6. Cấu hình Fcitx5

Sau khi login lại:

1. Mở **Fcitx5 Configuration**:

   ```bash
   fcitx5-configtool
   ```

2. Trong tab **Input Method**

3. Tìm và chọn **VMK** trong danh sách.

4. Nhấn **<-** để thêm vào danh sách bộ gõ.

### 7. Lưu ý cho Wayland (KDE và Hyprland)

Nếu bạn sử dụng **Wayland** trên KDE Plasma hoặc Hyprland, bạn cần thêm **Virtual Keyboard**:

- **KDE Plasma (Wayland):** System Settings -> Keyboard -> Virtual Keyboard -> Fcitx 5
- **Hyprland:** thêm `permission = fcitx5-vmk-server, keyboard, allow` vào `~/.config/hypr/hyprland.conf`

Điều này cần thiết vì trên Wayland, Fcitx5 không thể hoạt động như X11.

---

## 📖 Hướng dẫn sử dụng

### 1. Menu Chuyển Mode Nhanh

Khi đang ở trong bất kỳ ứng dụng nào, nhấn phím:

```
` (Phím dấu huyền)
```

Menu sẽ hiện ra cho phép bạn chọn số từ 1-7 và `` ` ``:

- **Mode 1 (Uinput):** Chế độ mặc định, tương thích tốt nhất (dùng server gửi phím xóa).
- **Mode 2 (Surrounding Text):** Dùng cơ chế surrounding text của ứng dụng, hoạt động rất tốt trên các app qt và gtk, hoặc các app tự implement surrounding text (khá bất ngờ là neovide, một app frontend cho neovim lại dùng mode này cực ổn).
- **Mode 3 (Preedit):** Hiện gạch chân, tương thích cao nhưng không tự nhiên bằng Mode 1.
- **Mode 4 (Uinput hardcore):** Cũng là mode 1 nhưng độ tương thích khá chấm hỏi, chỉ khuyến khích nếu dùng wine, các app native nên dùng mode 1.
- **OFF:** Tắt bộ gõ cho ứng dụng này.
- **Emoji mode:** Chuyển sang chế độ gõ emoji, nguồn emoji từ EmojiOne, search emoji bằng thuật toàn fuzzy search tiện lợi, có thể tra cứu tại bảng [sau](data/emoji/EMOJI_GUIDE.md).
- **Xóa thiết lập cho app:** Quay về dùng cấu hình mặc định.
- **Tắt menu và gõ phím `:** Thoát menu và in ký tự dấu huyền.

### 2. Cơ chế Reset thông minh

Khi bạn click chuột hoặc chạm vào touchpad để đổi vị trí nhập liệu, bộ gõ sẽ tự động Reset trạng thái ngay lập tức. Điều này giúp tránh lỗi dính chữ cũ vào từ mới (một lỗi rất phổ biến trên các bộ gõ Linux khác).

---

## 🙏 Lời cảm ơn (Credits)

Dự án này được phát triển dựa trên ý tưởng và mã nguồn gốc của tác giả Thành (tác giả gốc của VMK).

Chân thành cảm ơn tác giả đã đặt nền móng cho một bộ gõ tiếng Việt gọn nhẹ trên Linux.

---

## 📄 License

[GPL-3.0-or-later](LICENSE)

---

## 🔗 Liên kết

- **GitHub Repository:** https://github.com/nhktmdzhg/VMK
- **Báo lỗi:** https://github.com/nhktmdzhg/VMK/issues
- **Open Build Service:** https://software.opensuse.org//download.html?project=home%3Aiamnanoka&package=fcitx5-vmk
