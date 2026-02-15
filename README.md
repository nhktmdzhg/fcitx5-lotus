<a id="readme-top"></a>

<div align="center">
  <a href="https://github.com/nhktmdzhg/VMK">
    <img src="data/icons/scalable/apps/fcitx-vmk.svg" alt="Logo" width="80" height="80">
  </a>

  <h2 align="center">Fcitx5 VMK</h2>

  <p align="center">
    <b>Bộ gõ tiếng Việt đơn giản, hiệu năng cao cho Linux</b>
    <br />
    <br />
    <a href="https://github.com/nhktmdzhg/VMK/releases">
      <img src="https://img.shields.io/github/v/release/nhktmdzhg/VMK?style=flat&color=success" alt="Release">
    </a>
    <a href="https://github.com/nhktmdzhg/VMK/blob/main/LICENSE">
      <img src="https://img.shields.io/github/license/nhktmdzhg/VMK?style=flat&color=blue" alt="License">
    </a>
    <a href="https://github.com/nhktmdzhg/VMK/stargazers">
      <img src="https://img.shields.io/github/stars/nhktmdzhg/VMK?style=flat&color=yellow" alt="Stars">
    </a>
    <a href="https://github.com/nhktmdzhg/VMK/network/members">
      <img src="https://img.shields.io/github/forks/nhktmdzhg/VMK?style=flat&color=orange" alt="Forks">
    </a>
    <a href="https://github.com/nhktmdzhg/VMK/issues">
      <img src="https://img.shields.io/github/issues/nhktmdzhg/VMK?style=flat&color=red" alt="Issues">
    </a>
  </p>

  <p align="center">
    <a href="#cài-đặt"><strong>Cài đặt »</strong></a>
    <br />
    <br />
    <a href="https://github.com/nhktmdzhg/VMK/issues/new?template=bug_report.yml">Báo lỗi</a>
    &middot;
    <a href="https://github.com/nhktmdzhg/VMK/issues/new?template=feature_request.yml">Yêu cầu tính năng</a>
  </p>
</div>

<br />

Dự án này là một bản fork được tối ưu hóa từ [bộ gõ VMK gốc](https://github.com/thanhpy2009/VMK). Chân thành cảm ơn tác giả Thành đã đặt nền móng cho bộ gõ này.

> **Lưu ý:** Phiên bản này đã loại bỏ công cụ cấu hình cũ viết bằng FLTK. Mọi cấu hình giờ đây được thực hiện trực tiếp qua giao diện chuẩn của Fcitx5 hoặc qua menu phím tắt mới.

<details>
  <summary><b>Mục lục</b></summary>
  <ol>
    <li><a href="#cài-đặt">Cài đặt</a></li>
    <li><a href="#bật-bộ-gõ">Bật bộ gõ</a></li>
    <li><a href="#hướng-dẫn-sử-dụng">Hướng dẫn sử dụng</a></li>
    <li><a href="#gỡ-cài-đặt">Gỡ cài đặt</a></li>
    <li><a href="#cải-tiến-nổi-bật">Cải tiến nổi bật</a></li>
    <li><a href="#đóng-góp">Đóng góp</a></li>
    <li><a href="#giấy-phép">Giấy phép</a></li>
  </ol>
</details>

---

<a id="cài-đặt"></a>

## 📦 Cài đặt

<details>
<summary><b>Arch / Arch-based - AUR</b></summary>
<br>

Hiện tại AUR có 3 gói cài đặt để bạn lựa chọn:

| Gói              | Mô tả                                          |
| ---------------- | ---------------------------------------------- |
| `fcitx5-vmk`     | Build từ mã nguồn release ổn định              |
| `fcitx5-vmk-bin` | Dùng binary đã build sẵn (không cần biên dịch) |
| `fcitx5-vmk-git` | Build từ danh sách commit mới nhất             |

Cài đặt bằng `yay` hoặc `paru`:

```bash
# Cú pháp: yay -S <tên-gói>
yay -S fcitx5-vmk

# Hoặc nếu dùng paru
paru -S fcitx5-vmk
```

</details>

<details>
<summary><b>Debian / Ubuntu / Fedora / openSUSE - Open Build Service</b></summary>
<br>

Truy cập trang [Open Build Service](https://software.opensuse.org//download.html?project=home%3Aiamnanoka&package=fcitx5-vmk) để xem hướng dẫn cài đặt chi tiết cho distro của bạn.

[![build result](https://build.opensuse.org/projects/home:iamnanoka/packages/fcitx5-vmk/badge.svg?type=percent)](https://build.opensuse.org/package/show/home:iamnanoka/fcitx5-vmk)
[![build result](https://build.opensuse.org/projects/home:iamnanoka/packages/fcitx5-vmk/badge.svg?type=default)](https://build.opensuse.org/package/show/home:iamnanoka/fcitx5-vmk)

Hoặc có thể xem cách cài của từng distro [tại đây](INSTALL.md).

> **Lưu ý:** Arch và Arch-based distro cũng có thể dùng cách cài này.

</details>

<details>
<summary><b>NixOS</b></summary>
<br>

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

</details>

<details>
<summary><b>Biên dịch từ nguồn</b></summary>
<br>

> **KHUYẾN CÁO QUAN TRỌNG:**
>
> Vui lòng **KHÔNG** sử dụng cách này nếu distro của bạn đã được hỗ trợ thông qua **Open Build Service**.
>
> Việc biên dịch thủ công đòi hỏi bạn phải hiểu rõ về cấu trúc thư mục của hệ thống. Nếu bạn gặp lỗi "Not Available" hoặc thiếu thư viện khi cài theo cách này trên các distro phổ biến (Ubuntu/Fedora...), hãy quay lại dùng OBS để đảm bảo tính ổn định và tự động cập nhật.

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

</details>

---

<a id="bật-bộ-gõ"></a>

## ⚙️ Bật bộ gõ

Sau khi cài đặt xong, bạn cần thực hiện các bước sau để bật bộ gõ VMK:

### 1. Bật VMK Server

Server giúp bộ gõ tương tác với hệ thống tốt hơn (đặc biệt là gửi phím xóa và sửa lỗi).

```bash
# Bật và khởi động service (tự động fix lỗi thiếu user systemd nếu có)
sudo systemctl enable --now fcitx5-vmk-server@$(whoami).service || \
(sudo systemd-sysusers && sudo systemctl enable --now fcitx5-vmk-server@$(whoami).service)
```

```bash
# Kiểm tra status (nếu thấy active (running) màu xanh là OK)
systemctl status fcitx5-vmk-server@$(whoami).service
```

### 2. Thiết lập biến môi trường

Bộ gõ sẽ không hoạt động nếu thiếu các biến này. Chạy lệnh dưới để thêm vào file cấu hình shell của bạn (`~/.bash_profile` hoặc `~/.zprofile`):

```bash
# Lệnh này sẽ thêm cấu hình vào ~/.bash_profile, với .zprofile làm tương tự
cat <<EOF >> ~/.bash_profile
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx
export GLFW_IM_MODULE=ibus
EOF
```

Log out và log in để áp dụng thay đổi.

<details>
<summary><b>Nếu bạn vẫn chưa gõ được sau khi log out</b></summary>
<br>

Nếu cấu hình tại `~/.bash_profile` hoặc `~/.zprofile` không hoạt động, bạn có thể thử thiết lập tại `/etc/environment` để áp dụng cho toàn bộ hệ thống:

```bash
cat <<EOF | sudo tee -a /etc/environment
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
SDL_IM_MODULE=fcitx
GLFW_IM_MODULE=ibus
EOF
```

> **Lưu ý:** Cần khởi động lại máy sau khi thiết lập.

</details>

### 3. Tắt bộ gõ cũ (IBus) và thêm Fcitx5 vào Autostart

Nếu máy bạn đang dùng IBus, hãy tắt nó đi trước khi chuyển sang Fcitx5 để tránh xung đột.

```bash
# Tắt IBus
killall ibus-daemon || ibus exit
```

Thêm `fcitx5` vào danh sách ứng dụng khởi động cùng hệ thống (Autostart).

<details>
<summary><b>Hướng dẫn Autostart cho từng Desktop Environment (GNOME, KDE, ...)</b></summary>
<br>

| DE / WM        | Hướng dẫn chi tiết                                                                                                             |
| :------------- | :----------------------------------------------------------------------------------------------------------------------------- |
| **GNOME**      | Mở **GNOME Tweaks** → _Startup Applications_ → Add → `Fcitx 5`                                                                 |
| **KDE Plasma** | **System Settings** → _Autostart_ → Add... → Add Application... → `Fcitx 5`                                                    |
| **Xfce**       | **Settings** → _Session and Startup_ → _Application Autostart_ → Add → `Fcitx 5`                                               |
| **Cinnamon**   | **System Settings** → _Startup Applications_ → `+` → Choose application → `Fcitx 5`                                            |
| **MATE**       | **Control Center** → _Startup Applications_ → Add (Name: `Fcitx 5`, Command: `fcitx5`)                                         |
| **Pantheon**   | **System Settings** → _Applications_ → _Startup_ → _Add Startup App..._ → `Fcitx 5`                                            |
| **Budgie**     | **Budgie Desktop Settings** → _Autostart_ → `+` → Add application → `Fcitx 5`                                                  |
| **LXQt**       | **LXQt Configuration Center** → _Session Settings_ → _Autostart_ → _LXQt Autostart_ → Add (Name: `Fcitx 5`, Command: `fcitx5`) |
| **COSMIC**     | **COSMIC Settings** → _Applications_ → _Startup Applications_ → Add app → `Fcitx 5`                                            |
| **i3 / Sway**  | Thêm `exec --no-startup-id fcitx5 -d` vào file config (`~/.config/i3/config` hoặc `~/.config/sway/config`)                     |
| **Hyprland**   | Thêm `exec-once = fcitx5 -d` vào `~/.config/hypr/hyprland.conf`                                                                |

> **Lưu ý:** Hãy tắt autostart của IBus (thường là `ibus-daemon` hoặc `ibus`) để tránh xung đột. Tốt nhất là gỡ cài đặt IBus nếu không sử dụng.

</details>

### 4. Cấu hình Fcitx5

Sau khi đã log out và log in lại:

1. Mở **Fcitx5 Configuration** (tìm trong menu ứng dụng hoặc chạy `fcitx5-configtool`).
2. Tìm **VMK** ở cột bên phải.
3. Nhấn mũi tên **<** để thêm nó sang cột bên trái.
4. Apply.

### 5. Cấu hình cho Wayland (KDE và Hyprland)

Nếu bạn sử dụng **Wayland**, Fcitx5 cần được cấp quyền để hoạt động như bàn phím ảo:

- **KDE Plasma:** Vào _System Settings_ → _Keyboard_ → _Virtual Keyboard_ → Chọn **Fcitx 5**.
- **Hyprland:** Thêm dòng sau vào `~/.config/hypr/hyprland.conf`:
  ```ini
  permission = fcitx5-vmk-server, keyboard, allow
  ```

---

<a id="hướng-dẫn-sử-dụng"></a>

## 📖 Hướng dẫn sử dụng

### 1. Menu chuyển mode nhanh

Khi đang ở trong bất kỳ ứng dụng nào, nhấn phím **`** (dấu huyền) để mở menu chọn nhanh:

| Chế độ                           | Mô tả                                                                                                                                                                                                     |
| :------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🚀 **Mode 1 (Uinput smooth)**    | Chế độ mặc định, tốc độ phản hồi cao. Sử dụng server để gửi phím xóa. <br> _Hạn chế:_ Không tương thích với ứng dụng xử lý chậm (ví dụ: LibreOffice).                                                     |
| 🐢 **Mode 2 (Uinput)**           | Tương tự Mode 1 nhưng tốc độ gửi phím chậm hơn. <br> _Khuyên dùng:_ Cho các ứng dụng có tốc độ xử lý input thấp.                                                                                          |
| 🍷 **Mode 3 (Uinput hardcore)**  | Biến thể của Mode 1. <br> _Khuyên dùng:_ Chạy ứng dụng Windows qua Wine.                                                                                                                                  |
| ✨ **Mode 4 (Surrounding Text)** | Sử dụng cơ chế Surrounding Text của ứng dụng (tối ưu cho Qt/GTK). Cho phép sửa dấu từ đã gõ và hoạt động mượt mà. <br> _Lưu ý:_ Phụ thuộc vào sự hỗ trợ của ứng dụng (có thể không ổn định trên Firefox). |
| 📝 **Mode 5 (Preedit)**          | Hiển thị gạch chân khi gõ. Độ tương thích cao nhất nhưng trải nghiệm không tự nhiên bằng các mode trên.                                                                                                   |
| 😃 **Emoji mode**                | Chế độ tìm kiếm và nhập Emoji (nguồn EmojiOne, hỗ trợ Fuzzy Search). Xem danh sách [tại đây](data/emoji/EMOJI_GUIDE.md).                                                                                  |
| 📴 **OFF**                       | Tắt bộ gõ cho ứng dụng hiện tại.                                                                                                                                                                          |
| 🔄 **Remove app settings**       | Khôi phục cấu hình mặc định cho ứng dụng.                                                                                                                                                                 |
| 🚪 **Close menu and type `**     | Đóng menu và nhập ký tự dấu huyền.                                                                                                                                                                        |

### 2. Cơ chế đặt lại thông minh

Khi bạn click chuột hoặc chạm vào touchpad để đổi vị trí nhập liệu, bộ gõ sẽ tự động đặt lại trạng thái ngay lập tức. Điều này giúp tránh lỗi dính chữ cũ vào từ mới (một lỗi rất phổ biến trên các bộ gõ Linux khác).

---

<a id="gỡ-cài-đặt"></a>

## 🗑️ Gỡ cài đặt

<details>
<summary><b>Arch / Arch-based - AUR</b></summary>
<br>

Bạn có thể dùng `pacman`, `yay` hoặc `paru` để gỡ cài đặt:

```bash
# Sử dụng pacman (Khuyên dùng)
sudo pacman -Rns fcitx5-vmk

# Nếu dùng yay
yay -Rns fcitx5-vmk

# Nếu dùng paru
paru -Rns fcitx5-vmk
```

> **Lưu ý:** Các file config ở `$HOME` sẽ được giữ lại.

</details>

<details>
<summary><b>Debian / Ubuntu / Fedora / openSUSE - Open Build Service</b></summary>
<br>

Gỡ package thông thường qua trình quản lý gói:

```bash
# Debian/Ubuntu
sudo apt remove fcitx5-vmk

# Fedora
sudo dnf remove fcitx5-vmk

# openSUSE
sudo zypper remove fcitx5-vmk
```

</details>

<details>
<summary><b>NixOS</b></summary>
<br>

Xóa (hoặc comment) dòng `services.fcitx5-vmk` và `inputs` trong file config, sau đó rebuild lại system. NixOS sẽ tự dọn dẹp.

</details>

<details>
<summary><b>Biên dịch từ nguồn</b></summary>
<br>

Vào lại thư mục source code đã build và chạy:

```bash
sudo make uninstall
```

</details>

---

<a id="cải-tiến-nổi-bật"></a>

## 🚀 Cải tiến nổi bật

<details>
<summary><b>Click để xem chi tiết kỹ thuật</b></summary>
<br>

Bản fork này thay đổi hoàn toàn kiến trúc của Server và Addon để đạt hiệu năng tối ưu và bảo mật tốt hơn.

### 1. VMK Server (Backend)

Server được viết lại theo phong cách **System Programming** hiện đại:

- **Kiến trúc Event-Driven (Sử dụng `poll`):** Thay thế cơ chế polling liên tục (gây tốn CPU) bằng `poll()` với timeout hợp lý. Server sẽ "ngủ đông" khi không có sự kiện, giúp mức tiêu thụ CPU khi nhàn rỗi gần như 0%.
- **Single-Threaded:** Loại bỏ đa luồng phức tạp, gộp chung việc lắng nghe Socket và sự kiện đầu vào vào một vòng lặp sự kiện duy nhất, giảm overhead và dung lượng binary.
- **Real-time I/O:** Sử dụng socket để giao tiếp trực tiếp giữa server và addon thay vì ghi file log, giúp phản hồi tức thì và bảo vệ ổ cứng.
- **Bảo mật Socket:**
  - Sử dụng **Abstract Socket** (không tạo file trên đĩa) kết hợp với xác thực `getsockopt` để đảm bảo chỉ tiến trình hợp lệ mới có thể gửi tín hiệu.
  - Khắc phục các rủi ro bảo mật liên quan đến quyền truy cập file socket công khai ở phiên bản cũ.

### 2. VMK Addon (Frontend)

Cải thiện trải nghiệm người dùng với các tính năng tiện ích:

- **Per-App Configuration:** Tự động ghi nhớ chế độ gõ (Mode) riêng biệt cho từng ứng dụng (Ví dụ: Tắt bộ gõ ở Terminal, bật ở Trình duyệt).
- **Menu Phím Tắt Thông Minh (`):** Menu ngữ cảnh hiển thị ngay tại con trỏ văn bản, cho phép chuyển đổi chế độ nhanh chóng.
- **Tính năng mở rộng:** Hỗ trợ sửa dấu từ cũ (Surrounding Text), chế độ nhập Emoji và nhiều cải tiến khác.

</details>

---

<a id="đóng-góp"></a>

## 🤝 Đóng góp

Đóng góp là điều làm cho cộng đồng mã nguồn mở trở thành một nơi tuyệt vời để học hỏi, truyền cảm hứng và sáng tạo. Mọi đóng góp của bạn đều được **đánh giá cao**.

Vui lòng xem hướng dẫn chi tiết tại [đây](CONTRIBUTING.md) để biết cách tham gia phát triển dự án, quy trình Pull Request và quy tắc code style.

Đừng quên tặng dự án một ⭐! Cảm ơn bạn rất nhiều!

### Những người đóng góp:

<a href="https://github.com/nhktmdzhg/VMK/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=nhktmdzhg/VMK" alt="contrib.rocks image" />
</a>

---

<a id="giấy-phép"></a>

## 📃 Giấy phép

Dự án được phân phối dưới giấy phép GNU General Public License v3. Xem [`LICENSE`](LICENSE) để biết thêm chi tiết.

---

## ✨ Lịch sử sao

<a href="https://star-history.com/#nhktmdzhg/VMK&Date">
 <img src="https://api.star-history.com/svg?repos=nhktmdzhg/VMK&type=Date" alt="Star History Chart">
</a>

---
