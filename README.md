[English](README.en.md) | [Tiếng Việt](README.md)

<a id="readme-top"></a>

<div align="center">
  <a href="https://github.com/LotusInputMethod/fcitx5-lotus">
    <img src="data/icons/scalable/apps/fcitx-lotus.svg" alt="Logo" width="80" height="80">
  </a>

<h2 align="center">Fcitx5 Lotus</h2>

<p align="center">
    <b>Bộ gõ tiếng Việt đơn giản, hiệu năng cao cho Linux</b>
    <br />
    <br />
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/releases">
      <img src="https://img.shields.io/github/v/release/LotusInputMethod/fcitx5-lotus?style=flat&color=success" alt="Release">
    </a>
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/blob/main/LICENSE">
      <img src="https://img.shields.io/github/license/LotusInputMethod/fcitx5-lotus?style=flat&color=blue" alt="License">
    </a>
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/stargazers">
      <img src="https://img.shields.io/github/stars/LotusInputMethod/fcitx5-lotus?style=flat&color=yellow" alt="Stars">
    </a>
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/network/members">
      <img src="https://img.shields.io/github/forks/LotusInputMethod/fcitx5-lotus?style=flat&color=orange" alt="Forks">
    </a>
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/issues">
      <img src="https://img.shields.io/github/issues/LotusInputMethod/fcitx5-lotus?style=flat&color=red" alt="Issues">
    </a>
    <a href="#contributors-">
      <img src="https://img.shields.io/badge/all_contributors-5-orange.svg?style=flat-square" alt="All Contributors">
    </a>
  </p>

<p align="center">
    <a href="#cài-đặt"><strong>Cài đặt »</strong></a>
    <br />
    <br />
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/issues/new?template=bug_report.yml">Báo lỗi</a>
    ·
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/issues/new?template=feature_request.yml">Yêu cầu tính năng</a>
  </p>
</div>

<br />

Dự án này là bản fork được tối ưu hóa từ [bộ gõ VMK](https://github.com/thanhpy2009/VMK). Chân thành cảm ơn tác giả Thành đã đặt nền móng cho bộ gõ này.

> **Lưu ý:** Gỡ và xoá cấu hình `fcitx5-vmk` trước khi cài đặt `fcitx5-lotus` để tránh phát sinh lỗi.
>
> <details>
> <summary><b>Gỡ và xoá cấu hình <code>fcitx5-vmk</code></b></summary>
>
> <details>
> <summary><b>Arch / Arch-based - AUR</b></summary>
> <br>
>
> Bạn có thể dùng `pacman` (khuyên dùng), `yay` hoặc `paru` để gỡ cài đặt:
>
> ```bash
> sudo pacman -Rns fcitx5-vmk
> ```
>
> ```bash
> yay -Rns fcitx5-vmk
> ```
>
> ```bash
> paru -Rns fcitx5-vmk
> ```
>
> > **Lưu ý:** Các file config ở `$HOME` sẽ được giữ lại.
>
> </details>
>
> <details>
> <summary><b>Debian / Ubuntu / Fedora / openSUSE</b></summary>
> <br>
>
> - <b>Debian/Ubuntu</b>
>
> ```bash
> sudo apt remove fcitx5-vmk
> ```
>
> - <b>Fedora</b>
>
> ```bash
> sudo dnf remove fcitx5-vmk
> ```
>
> - <b>openSUSE</b>
>
> ```bash
> sudo zypper remove fcitx5-vmk
> ```
>
> </details>
>
> <details>
> <summary><b>NixOS</b></summary>
> <br>
>
> Xóa (hoặc comment) dòng `services.fcitx5-vmk` và `inputs` trong file config, sau đó rebuild lại system. NixOS sẽ tự dọn dẹp.
>
> </details>
>
> <details>
> <summary><b>Biên dịch từ nguồn</b></summary>
> <br>
>
> Vào lại thư mục source code đã build và chạy:
>
> ```bash
> sudo make uninstall
> ```
>
> </details>
>
> ---
>
> Xóa cấu hình `vmk` không tương thích:
>
> ```bash
> rm ~/.config/fcitx5/conf/vmk-*.conf
> ```
>
> </details>

<details>
  <summary><b>Mục lục</b></summary>
  <ol>
    <li><a href="#cài-đặt">Cài đặt</a></li>
    <li><a href="#bật-bộ-gõ">Bật bộ gõ</a></li>
    <li><a href="#hướng-dẫn-sử-dụng">Hướng dẫn sử dụng</a></li>
    <li><a href="#gỡ-cài-đặt">Gỡ cài đặt</a></li>
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

| Gói                | Mô tả                              |
| ------------------ | ---------------------------------- |
| `fcitx5-lotus`     | Build từ mã nguồn release ổn định  |
| `fcitx5-lotus-bin` | Dùng binary đã build sẵn           |
| `fcitx5-lotus-git` | Build từ danh sách commit mới nhất |

Cài đặt bằng `yay`:

```bash
# Cú pháp: yay -S <tên-gói>
yay -S fcitx5-lotus
```

Hoặc `paru`:

```bash
# Cú pháp: paru -S <tên-gói>
paru -S fcitx5-lotus
```

</details>

<details>
<summary><b>Debian / Ubuntu / Fedora / openSUSE</b></summary>
<br>

Bạn có thể xem cách cài của từng distro [tại đây](INSTALL.md).

</details>

<details>
<summary><b>NixOS</b></summary>
<br>

Thêm input của fcitx5-lotus vào `flake.nix`:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    fcitx5-lotus = {
      url = "github:LotusInputMethod/fcitx5-lotus";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
  ...
}
```

Bật fcitx5-lotus service trong `configuration.nix`:

```nix
{
  inputs,
  ...
}: {
  imports = [
    inputs.fcitx5-lotus.nixosModules.fcitx5-lotus
  ];

  services.fcitx5-lotus = {
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
> Vui lòng **KHÔNG** sử dụng cách này nếu distro của bạn đã được hỗ trợ thông qua **Cloudflare Pages**.
>
> Việc biên dịch thủ công đòi hỏi bạn phải hiểu rõ về cấu trúc thư mục của hệ thống. Nếu bạn gặp lỗi "Not Available" hoặc thiếu thư viện khi cài theo cách này trên các distro phổ biến (Ubuntu/Fedora...), hãy quay lại dùng Cloudflare Pages để đảm bảo tính ổn định và tự động cập nhật.

##### Yêu cầu hệ thống

- **Debian/Ubuntu**

```bash
sudo apt-get install cmake extra-cmake-modules libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev libinput-dev libudev-dev g++ golang hicolor-icon-theme pkg-config libx11-dev
```

- **Fedora/RHEL**

```bash
sudo dnf install cmake extra-cmake-modules fcitx5-devel libinput-devel libudev-devel gcc-c++ golang hicolor-icon-theme systemd-devel libX11-devel
```

- **openSUSE**

```bash
sudo zypper install cmake extra-cmake-modules fcitx5-devel libinput-devel systemd-devel gcc-c++ go hicolor-icon-theme systemd-devel libX11-devel udev
```

##### Biên dịch và cài đặt

```bash
# Clone repository
git clone https://github.com/LotusInputMethod/fcitx5-lotus.git
cd fcitx5-lotus

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

Sau khi cài đặt xong, bạn cần thực hiện các bước sau để bật bộ gõ fcitx5-lotus:

### 1. Bật Lotus Server

Server giúp bộ gõ tương tác với hệ thống tốt hơn (đặc biệt là gửi phím xóa và sửa lỗi).

<details open>
<summary><b>Bash / Zsh</b></summary>

```bash
# Bật và khởi động service (tự động fix lỗi thiếu user systemd nếu có)
sudo systemctl enable --now fcitx5-lotus-server@$(whoami).service || \
(sudo systemd-sysusers && sudo systemctl enable --now fcitx5-lotus-server@$(whoami).service)
```

</details>

<details open>
<summary><b>Fish shell</b></summary>

```fish
# Bật và khởi động service (tự động fix lỗi thiếu user systemd nếu có)
sudo systemctl enable --now fcitx5-lotus-server@(whoami).service; or begin
    sudo systemd-sysusers; and sudo systemctl enable --now fcitx5-lotus-server@(whoami).service
end
```

</details>

```bash
# Kiểm tra status (nếu thấy active (running) màu xanh là OK)
systemctl status fcitx5-lotus-server@$(whoami).service
```

### 2. Thiết lập biến môi trường

Bộ gõ sẽ không hoạt động nếu thiếu các biến này.

<details open>
<summary><b>Bash</b></summary>

```bash
# Thêm cấu hình vào ~/.bash_profile
cat <<EOF >> ~/.bash_profile
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx
export GLFW_IM_MODULE=ibus
EOF
```

</details>

<details open>
<summary><b>Zsh</b></summary>

```bash
# Thêm cấu hình vào ~/.zprofile
cat <<EOF >> ~/.zprofile
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx
export GLFW_IM_MODULE=ibus
EOF
```

</details>
<details open>
<summary><b>Fish shell</b></summary>

```fish
# Thêm cấu hình vào ~/.config/fish/config.fish
echo 'if status is-login
    set -Ux GTK_IM_MODULE fcitx
    set -Ux QT_IM_MODULE fcitx
    set -Ux XMODIFIERS "@im=fcitx"
    set -gx SDL_IM_MODULE fcitx
    set -gx GLFW_IM_MODULE ibus
end' >> ~/.config/fish/config.fish
```

</details>

Log out và log in để áp dụng thay đổi.

<details>
<summary><b>Nếu bạn vẫn chưa gõ được sau khi log out</b></summary>
<br>

Nếu cấu hình tại `~/.bash_profile`, `~/.zprofile` hay `.config/fish/config.fish` không hoạt động, bạn có thể thử thiết lập tại `/etc/environment` để áp dụng cho toàn bộ hệ thống:

<details open>
<summary><b>Bash/Zsh</b></summary>

```bash
cat <<EOF | sudo tee -a /etc/environment
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
SDL_IM_MODULE=fcitx
GLFW_IM_MODULE=ibus
EOF
```

</details>

<details open>
<summary><b>Fish shell</b></summary>

```fish
echo "GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
SDL_IM_MODULE=fcitx
GLFW_IM_MODULE=ibus" | sudo tee -a /etc/environment
```

</details>

> **Lưu ý:** Cần khởi động lại máy sau khi thiết lập.

</details>

### 3. Tắt bộ gõ cũ (IBus) và thêm Fcitx5 vào Autostart

Nếu máy bạn đang dùng IBus, hãy tắt nó đi trước khi chuyển sang Fcitx5 để tránh xung đột.

```bash
# Tắt IBus
killall ibus-daemon || ibus exit
```

<details>
<summary><b>Thêm Fcitx5 vào Autostart cho từng DE / WM (GNOME, Hyprland ...)</b></summary>

| DE / WM        | Hướng dẫn chi tiết                                                                                                           |
| :------------- | :--------------------------------------------------------------------------------------------------------------------------- |
| **GNOME**      | _GNOME Tweaks_ → _Startup Applications_ → Add → `Fcitx 5`                                                                    |
| **KDE Plasma** | _System Settings_ → _Autostart_ → Add... → Add Application... → `Fcitx 5`                                                    |
| **Xfce**       | _Settings_ → _Session and Startup_ → _Application Autostart_ → Add → `Fcitx 5`                                               |
| **Cinnamon**   | _System Settings_ → _Startup Applications_ → `+` → Choose application → `Fcitx 5`                                            |
| **MATE**       | _Control Center_ → _Startup Applications_ → Add (Name: `Fcitx 5`, Command: `fcitx5`)                                         |
| **Pantheon**   | _System Settings_ → _Applications_ → _Startup_ → _Add Startup App..._ → `Fcitx 5`                                            |
| **Budgie**     | _Budgie Desktop Settings_ → _Autostart_ → `+` → Add application → `Fcitx 5`                                                  |
| **LXQt**       | _LXQt Configuration Center_ → _Session Settings_ → _Autostart_ → _LXQt Autostart_ → Add (Name: `Fcitx 5`, Command: `fcitx5`) |
| **COSMIC**     | _COSMIC Settings_ → _Applications_ → _Startup Applications_ → Add app → `Fcitx 5`                                            |
| **i3 / Sway**  | Thêm `exec --no-startup-id fcitx5 -d` vào file config (`~/.config/i3/config` hoặc `~/.config/sway/config`)                   |
| **Hyprland**   | Thêm `exec-once = fcitx5 -d` vào `~/.config/hypr/hyprland.conf`                                                              |

> **Lưu ý:** Hãy tắt autostart của IBus (thường là `ibus-daemon` hoặc `ibus`) để tránh xung đột. Tốt nhất là gỡ cài đặt IBus nếu không sử dụng.

</details>

### 4. Cấu hình Fcitx5

Sau khi đã log out và log in lại:

1. Mở **Fcitx5 Configuration** (tìm trong menu ứng dụng hoặc chạy `fcitx5-configtool`).
2. Tìm **Lotus** ở cột bên phải.
3. Nhấn mũi tên **<** để thêm nó sang cột bên trái.
4. Apply.

<details>
   <summary><b>Cấu hình thêm cho Wayland (KDE, Hyprland)</b></summary>
   
- **KDE Plasma:** _System Settings_ → _Keyboard_ → _Virtual Keyboard_ → Chọn **Fcitx 5**.
- **Hyprland:** Thêm dòng sau vào `~/.config/hypr/hyprland.conf`:

```ini
permission = fcitx5-lotus-server, keyboard, allow
```

</details>

---

<a id="hướng-dẫn-sử-dụng"></a>

## 📖 Hướng dẫn sử dụng

### 1. Tùy chỉnh bộ gõ

- **Truy cập:** Nhấp chuột phải vào biểu tượng Lotus trên system tray để mở tuỳ chỉnh.

| Tùy chọn                | Mô tả                                                                                                            | Mặc định        |
| :---------------------- | :--------------------------------------------------------------------------------------------------------------- | :-------------- |
| **Typing Mode**         | Chọn chế độ gõ.                                                                                                  | Uinput (Smooth) |
| **Input Method**        | Chọn kiểu gõ.                                                                                                    | Telex           |
| **Charset**             | Chọn bảng mã.                                                                                                    | Unicode         |
| **Spell Check**         | Bật/tắt kiểm tra lỗi chính tả tiếng Việt.                                                                        | Bật             |
| **Macro**               | Bật/tắt gõ tắt.                                                                                                  | Bật             |
| **Capitalize Macro**    | Bật/tắt gõ tắt chữ hoa.                                                                                          | Bật             |
| **Auto non-VN restore** | Bật/tắt tự động khôi phục với từ không phải tiếng Việt.                                                          | Bật             |
| **Modern Style**        | Bật/tắt kiểu đặt dấu thanh hiện đại _(ví dụ: oà, *uý thay vì òa, *úy)_.                                          | Bật             |
| **Free Marking**        | Bật/tắt bỏ dấu tự do.                                                                                            | Bật             |
| **Fix Uinput with ack** | Bật/tắt sửa lỗi chế độ Uinput với ack.<br/>Nên bật khi sử dụng các ứng dụng Chromium (Chrome, Brave, Edge, ...). | Tắt             |
| **Lotus Icons**         | Bật/tắt sử dụng icon Lotus thay vì icon mặc định V E.                                                            | Tắt             |
| **Typing Mode Menu**    | Bật/tắt mở menu chuyển chế độ gõ bằng phím **`** (tắt để nhập ký tự trực tiếp).                                  | Bật             |

### 2. Menu chuyển chế độ gõ

Khi con trỏ đang ở trong ô nhập liệu (có thể gõ văn bản), nhấn phím **`** để mở menu chọn chế độ gõ; bạn có thể dùng chuột hoặc phím tắt để chọn chế độ mong muốn.

| Chế độ                | Phím tắt | Mô tả                                                                                                                                |
| :-------------------- | :------: | :----------------------------------------------------------------------------------------------------------------------------------- |
| **Uinput (Smooth)**   |  **1**   | Chế độ mặc định, phản hồi nhanh.<br>**Tối ưu:** ứng dụng có tốc độ xử lý input cao.                                                  |
| **Uinput (Slow)**     |  **2**   | Tương tự Uinput (Smooth) nhưng tốc độ gửi phím chậm hơn.<br>**Tối ưu:** ứng dụng có tốc độ xử lý input thấp _(ví dụ: Libre Office)_. |
| **Uinput (Hardcore)** |  **3**   | Biến thể của Uinput (Smooth).<br>**Tối ưu:** ứng dụng Windows qua Wine.                                                              |
| **Surrounding Text**  |  **4**   | Cho phép sửa dấu trên văn bản đã gõ, hoạt động mượt. <br> **Tối ưu:** ứng dụng Qt/GTK.                                               |
| **Preedit**           |  **Q**   | Hiển thị gạch chân khi gõ. <br> **Tối ưu:** hầu hết ứng dụng.                                                                        |
| **Emoji Picker**      |  **W**   | Tìm kiếm và nhập Emoji (nguồn EmojiOne, hỗ trợ fuzzy search).                                                                        |
| **OFF**               |  **E**   | Tắt bộ gõ.                                                                                                                           |
| **Default Typing**    |  **R**   | Chế độ gõ mặc định được cấu hình tại tuỳ chọn _Typing mode_.                                                                         |
| **Type `**            |  **`**   | Nhập ký tự **`**.                                                                                                                    |

Bộ gõ sẽ tự động lưu chế độ gõ đã dùng gần nhất cho từng ứng dụng và khôi phục cấu hình đó khi bạn mở lại chúng.

### 3. Đặt lại trạng thái đang gõ

Nhấp chuột hoặc chạm touchpad trong khi gõ sẽ tự động đặt lại trạng thái đang gõ, ngăn chặn hiện tượng dính ký tự giữa các từ.

---

<a id="gỡ-cài-đặt"></a>

## 🗑️ Gỡ cài đặt

<details>
<summary><b>Arch / Arch-based - AUR</b></summary>
<br>

Bạn có thể dùng `pacman` (khuyên dùng), `yay` hoặc `paru` để gỡ cài đặt:

```bash
sudo pacman -Rns fcitx5-lotus
```

```bash
yay -Rns fcitx5-lotus
```

```bash
paru -Rns fcitx5-lotus
```

> **Lưu ý:** Các file config ở `$HOME` sẽ được giữ lại.

</details>

<details>
<summary><b>Debian / Ubuntu / Fedora / openSUSE</b></summary>
<br>

- **Debian/Ubuntu**

```bash
sudo apt remove fcitx5-lotus
```

- **Fedora**

```bash
sudo dnf remove fcitx5-lotus
```

- **openSUSE**

```bash
sudo zypper remove fcitx5-lotus
```

</details>

<details>
<summary><b>NixOS</b></summary>
<br>

Xóa (hoặc comment) dòng `services.fcitx5-lotus` và `inputs` trong file config, sau đó rebuild lại system. NixOS sẽ tự dọn dẹp.

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

<a id="đóng-góp"></a>

## 🤝 Đóng góp

Đóng góp là điều làm cho cộng đồng mã nguồn mở trở thành một nơi tuyệt vời để học hỏi, truyền cảm hứng và sáng tạo. Mọi đóng góp của bạn đều được **đánh giá cao**.

Vui lòng xem hướng dẫn chi tiết [tại đây](CONTRIBUTING.md) để biết cách tham gia phát triển dự án, quy trình Pull Request, quy tắc code style và **quy tắc ứng xử**.

Đừng quên tặng dự án một ⭐! Cảm ơn bạn rất nhiều!

---

## Những người đóng góp ✨

Cảm ơn những con người tuyệt vời ([chú thích emoji](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/nhktmdzhg"><img src="https://avatars.githubusercontent.com/u/57983253?v=4?s=100" width="100px;" alt="Nguyen Hoang Ky"/><br /><sub><b>Nguyen Hoang Ky</b></sub></a><br /><a href="#blog-nhktmdzhg" title="Blogposts">📝</a> <a href="https://github.com/LotusInputMethod/fcitx5-lotus/commits?author=nhktmdzhg" title="Code">💻</a> <a href="https://github.com/LotusInputMethod/fcitx5-lotus/commits?author=nhktmdzhg" title="Documentation">📖</a> <a href="#projectManagement-nhktmdzhg" title="Project Management">📆</a> <a href="https://github.com/LotusInputMethod/fcitx5-lotus/pulls?q=is%3Apr+reviewed-by%3Anhktmdzhg" title="Reviewed Pull Requests">👀</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/hthienloc"><img src="https://avatars.githubusercontent.com/u/148019203?v=4?s=100" width="100px;" alt="Loc Huynh"/><br /><sub><b>Loc Huynh</b></sub></a><br /><a href="https://github.com/LotusInputMethod/fcitx5-lotus/issues?q=author%3Ahthienloc" title="Bug reports">🐛</a> <a href="https://github.com/LotusInputMethod/fcitx5-lotus/commits?author=hthienloc" title="Documentation">📖</a> <a href="#design-hthienloc" title="Design">🎨</a> <a href="#translation-hthienloc" title="Translation">🌍</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/justanoobcoder"><img src="https://avatars.githubusercontent.com/u/57614330?v=4?s=100" width="100px;" alt="Nguyễn Hồng Hiệp"/><br /><sub><b>Nguyễn Hồng Hiệp</b></sub></a><br /><a href="https://github.com/LotusInputMethod/fcitx5-lotus/commits?author=justanoobcoder" title="Documentation">📖</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/Miho1254"><img src="https://avatars.githubusercontent.com/u/83270073?v=4?s=100" width="100px;" alt="Đặng Quang Hiển"/><br /><sub><b>Đặng Quang Hiển</b></sub></a><br /><a href="https://github.com/LotusInputMethod/fcitx5-lotus/commits?author=Miho1254" title="Documentation">📖</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/Zebra2711"><img src="https://avatars.githubusercontent.com/u/89755535?v=4?s=100" width="100px;" alt="Zebra2711"/><br /><sub><b>Zebra2711</b></sub></a><br /><a href="https://github.com/LotusInputMethod/fcitx5-lotus/issues?q=author%3AZebra2711" title="Bug reports">🐛</a> <a href="https://github.com/LotusInputMethod/fcitx5-lotus/commits?author=Zebra2711" title="Code">💻</a></td>
    </tr>
  </tbody>
  <tfoot>
    <tr>
      <td align="center" size="13px" colspan="7">
        <img src="https://raw.githubusercontent.com/all-contributors/all-contributors-cli/1b8533af435da9854653492b1327a23a4dbd0a10/assets/logo-small.svg">
          <a href="https://all-contributors.js.org/docs/en/bot/usage">Add your contributions</a>
        </img>
      </td>
    </tr>
  </tfoot>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

Dự án này tuân thủ cấu trúc của [all-contributors](https://github.com/all-contributors/all-contributors). Mọi đóng góp đều được hoan nghênh!

---

<a id="giấy-phép"></a>

## 📃 Giấy phép

Dự án được phân phối dưới giấy phép GNU General Public License v3. Xem [`LICENSE`](LICENSE) để biết thêm chi tiết.

---

## ✨ Lịch sử sao

<a href="https://star-history.com/#LotusInputMethod/fcitx5-lotus&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=LotusInputMethod/fcitx5-lotus&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=LotusInputMethod/fcitx5-lotus&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=LotusInputMethod/fcitx5-lotus&type=date&legend=top-left" />
 </picture>
</a>
