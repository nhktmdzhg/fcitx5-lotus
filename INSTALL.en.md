[English](INSTALL.en.md) | [Tiếng Việt](INSTALL.md)

# Installation guide for fcitx5-lotus on distros: Fedora, Debian, OpenSUSE, Ubuntu

# How to remove OBS repository (if fcitx5-lotus was previously installed via OBS)

## Arch Linux

### Step 1: Remove the repository

Open `/etc/pacman.conf` and delete the following lines:

```ini
[home_iamnanoka_Arch]
Server = https://download.opensuse.org/repositories/home:/iamnanoka/Arch/$arch
```

### Step 2: Remove the key

```bash
sudo pacman-key --finger | grep "home:iamnanoka" -B 1 # To view the key ID
sudo pacman-key --delete "ID_OF_KEY" # Replace ID_OF_KEY with the ID above (fingerprint, 40 characters long)
sudo pacman-key --updatedb
sudo pacman -Syy
```

## Debian / Ubuntu

```bash
sudo rm -f /etc/apt/sources.list.d/home:iamnanoka.list
sudo rm -f /etc/apt/trusted.gpg.d/home_iamnanoka.gpg
sudo apt update
```

## Fedora

```bash
sudo rm /etc/yum.repos.d/home:iamnanoka.repo
```

## openSUSE

```bash
sudo zypper removerepo home_iamnanoka
```

# Installation guide for fcitx5-lotus

## Debian / Ubuntu

### Step 1: Import GPG key

```bash
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://fcitx5-lotus.pages.dev/pubkey.gpg \
  | sudo gpg --dearmor -o /etc/apt/keyrings/fcitx5-lotus.gpg
```

### Step 2: Add repository

Replace `CODENAME` according to the table below:

```bash
echo "deb [signed-by=/etc/apt/keyrings/fcitx5-lotus.gpg] \
  https://fcitx5-lotus.pages.dev/apt/CODENAME CODENAME main" \
  | sudo tee /etc/apt/sources.list.d/fcitx5-lotus.list
```

| Operating System | CODENAME   |
| ---------------- | ---------- |
| Debian 12        | `bookworm` |
| Debian 13        | `trixie`   |
| Debian Testing   | `testing`  |
| Debian Unstable  | `sid`      |
| Ubuntu 22.04 LTS | `jammy`    |
| Ubuntu 24.04 LTS | `noble`    |
| Ubuntu 25.04     | `plucky`   |
| Ubuntu 25.10     | `questing` |

Example with Debian 12:

```bash
echo "deb [signed-by=/etc/apt/keyrings/fcitx5-lotus.gpg] \
  https://fcitx5-lotus.pages.dev/apt/bookworm bookworm main" \
  | sudo tee /etc/apt/sources.list.d/fcitx5-lotus.list
```

### Step 3: Installation

```bash
sudo apt update
sudo apt install fcitx5-lotus
```

---

## Fedora

### Step 1: Import GPG key

```bash
sudo rpm --import https://fcitx5-lotus.pages.dev/pubkey.gpg
```

### Step 2: Add repository

Replace `RELEASEVER` with `42`, `43` or `rawhide`:

```bash
sudo dnf config-manager addrepo \
  --from-repofile=https://fcitx5-lotus.pages.dev/rpm/fedora/fcitx5-lotus-RELEASEVER.repo
```

Example with Fedora 43:

```bash
sudo dnf config-manager addrepo \
  --from-repofile=https://fcitx5-lotus.pages.dev/rpm/fedora/fcitx5-lotus-43.repo
```

### Step 3: Installation

```bash
sudo dnf install fcitx5-lotus
```

---

## openSUSE Tumbleweed

### Step 1: Import GPG key

```bash
sudo rpm --import https://fcitx5-lotus.pages.dev/pubkey.gpg
```

### Step 2: Add repository

```bash
sudo zypper addrepo \
  https://fcitx5-lotus.pages.dev/rpm/opensuse/fcitx5-lotus-tumbleweed.repo
sudo zypper refresh
```

### Step 3: Installation

```bash
sudo zypper install fcitx5-lotus
```

---

## Manual Installation (without repo)

Download `.deb` or `.rpm` files directly from [GitHub Releases](https://github.com/LotusInputMethod/fcitx5-lotus/releases/latest):

```bash
# Debian/Ubuntu
sudo dpkg -i fcitx5-lotus_*.deb

# Fedora / openSUSE
sudo rpm -i fcitx5-lotus-*.rpm
```
