# Hướng dẫn cài đặt fcitx5-lotus trên các distro: Fedora, Debian, OpenSUSE, Ubuntu

# Hướng dẫn cài đặt fcitx5-lotus

## Debian / Ubuntu

### Bước 1: Import GPG key

```bash
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://fcitx5-lotus.pages.dev/pubkey.gpg \
  | sudo gpg --dearmor -o /etc/apt/keyrings/fcitx5-lotus.gpg
```

### Bước 2: Thêm repository

Thay `CODENAME` theo bảng bên dưới:

```bash
echo "deb [signed-by=/etc/apt/keyrings/fcitx5-lotus.gpg] \
  https://fcitx5-lotus.pages.dev/apt/CODENAME CODENAME main" \
  | sudo tee /etc/apt/sources.list.d/fcitx5-lotus.list
```

| Hệ điều hành     | CODENAME   |
| ---------------- | ---------- |
| Debian 12        | `bookworm` |
| Debian 13        | `trixie`   |
| Debian Testing   | `testing`  |
| Debian Unstable  | `sid`      |
| Ubuntu 22.04 LTS | `jammy`    |
| Ubuntu 24.04 LTS | `noble`    |
| Ubuntu 25.04     | `plucky`   |
| Ubuntu 25.10     | `questing` |

Ví dụ với Debian 12:

```bash
echo "deb [signed-by=/etc/apt/keyrings/fcitx5-lotus.gpg] \
  https://fcitx5-lotus.pages.dev/apt/bookworm bookworm main" \
  | sudo tee /etc/apt/sources.list.d/fcitx5-lotus.list
```

### Bước 3: Cài đặt

```bash
sudo apt update
sudo apt install fcitx5-lotus
```

---

## Fedora

### Bước 1: Import GPG key

```bash
sudo rpm --import https://fcitx5-lotus.pages.dev/pubkey.gpg
```

### Bước 2: Thêm repository

Thay `RELEASEVER` bằng `42`, `43` hoặc `rawhide`:

```bash
sudo dnf config-manager addrepo \
  --from-repofile=https://fcitx5-lotus.pages.dev/rpm/fedora/fcitx5-lotus-RELEASEVER.repo
```

Ví dụ với Fedora 43:

```bash
sudo dnf config-manager addrepo \
  --from-repofile=https://fcitx5-lotus.pages.dev/rpm/fedora/fcitx5-lotus-43.repo
```

### Bước 3: Cài đặt

```bash
sudo dnf install fcitx5-lotus
```

---

## openSUSE Tumbleweed

### Bước 1: Import GPG key

```bash
sudo rpm --import https://fcitx5-lotus.pages.dev/pubkey.gpg
```

### Bước 2: Thêm repository

```bash
sudo zypper addrepo \
  https://fcitx5-lotus.pages.dev/rpm/opensuse/fcitx5-lotus-tumbleweed.repo
sudo zypper refresh
```

### Bước 3: Cài đặt

```bash
sudo zypper install fcitx5-lotus
```

---

## Cài thủ công (không dùng repo)

Tải file `.deb` hoặc `.rpm` trực tiếp từ [GitHub Releases](https://github.com/LotusInputMethod/fcitx5-lotus/releases/latest):

```bash
# Debian/Ubuntu
sudo dpkg -i fcitx5-lotus_*.deb

# Fedora / openSUSE
sudo rpm -i fcitx5-lotus-*.rpm
```
