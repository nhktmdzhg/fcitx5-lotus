[English](README.en.md) | [Tiếng Việt](README.md)

<a id="readme-top"></a>

<div align="center">
  <a href="https://github.com/LotusInputMethod/fcitx5-lotus">
    <img src="data/icons/scalable/apps/fcitx-lotus.svg" alt="Logo" width="80" height="80">
  </a>

  <h2 align="center">Fcitx5 Lotus</h2>

  <p align="center">
    <b>A simple, high-performance Vietnamese input method for Linux</b>
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
    <a href="#installation"><strong>Installation »</strong></a>
    <br />
    <br />
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/issues/new?template=bug_report.yml">Report Bug</a>
    &middot;
    <a href="https://github.com/LotusInputMethod/fcitx5-lotus/issues/new?template=feature_request.yml">Request Feature</a>
  </p>
</div>

<br />

This project is an optimized fork of [VMK input method](https://github.com/thanhpy2009/VMK). Sincere thanks to the author Thanh for laying the foundation for this input method.

> **Note:** Uninstall and delete the `fcitx5-vmk` configuration before installing `fcitx5-lotus` to avoid errors.
>
> <details>
> <summary><b>Uninstall and delete <code>fcitx5-vmk</code> configuration</b></summary>
>
> <details>
> <summary><b>Arch / Arch-based - AUR</b></summary>
> <br>
>
> You can use `pacman` (recommended), `yay`, or `paru` to uninstall:
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
> > **Note:** Config files in `$HOME` will be kept.
>
> </details>
>
> <details>
> <summary><b>Debian / Ubuntu / Fedora / openSUSE - Open Build Service</b></summary>
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
> Delete (or comment out) the `services.fcitx5-vmk` and `inputs` lines in the config file, then rebuild the system. NixOS will clean up automatically.
>
> </details>
>
> <details>
> <summary><b>Build from source</b></summary>
> <br>
>
> Go back to the built source code directory and run:
>
> ```bash
> sudo make uninstall
> ```
>
> </details>
>
> ---
>
> Delete incompatible `vmk` configuration:
>
> ```bash
> rm ~/.config/fcitx5/conf/vmk-*.conf
> ```
>
> </details>

<details>
  <summary><b>Table of Contents</b></summary>
  <ol>
    <li><a href="#installation">Installation</a></li>
    <li><a href="#enable-input-method">Enable Input Method</a></li>
    <li><a href="#usage-guide">Usage Guide</a></li>
    <li><a href="#uninstallation">Uninstallation</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
  </ol>
</details>

---

<a id="installation"></a>

## 📦 Installation

<details>
<summary><b>Arch / Arch-based - AUR</b></summary>
<br>

Currently, AUR has 3 installation packages to choose from:

| Package            | Description                           |
| ------------------ | ------------------------------------- |
| `fcitx5-lotus`     | Build from stable release source code |
| `fcitx5-lotus-bin` | Use pre-built binary                  |
| `fcitx5-lotus-git` | Build from the latest commit list     |

Install with `yay`:

```bash
# Syntax: yay -S <package-name>
yay -S fcitx5-lotus
```

Or `paru`:

```bash
# Syntax: paru -S <package-name>
paru -S fcitx5-lotus
```

</details>

<details>
<summary><b>Debian / Ubuntu / Fedora / openSUSE - Open Build Service</b></summary>
<br>

Visit the [Open Build Service](https://software.opensuse.org//download.html?project=home%3Aiamnanoka&package=fcitx5-lotus) page for detailed installation instructions for your distro.

[![build result](https://build.opensuse.org/projects/home:iamnanoka/packages/fcitx5-lotus/badge.svg?type=percent)](https://build.opensuse.org/package/show/home:iamnanoka/fcitx5-lotus)
[![build result](https://build.opensuse.org/projects/home:iamnanoka/packages/fcitx5-lotus/badge.svg?type=default)](https://build.opensuse.org/package/show/home:iamnanoka/fcitx5-lotus)

Or you can see the installation method for each distro [here](INSTALL.md).

> **Note:** Arch and Arch-based distros can also use this installation method.

</details>

<details>
<summary><b>NixOS</b></summary>
<br>

Add fcitx5-lotus input to `flake.nix`:

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

Enable fcitx5-lotus service in `configuration.nix`:

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
    user = "your_username"; # Change to your username
  };
}
```

Rebuild the system to install.

</details>

<details>
<summary><b>Build from source</b></summary>
<br>

> **IMPORTANT RECOMMENDATION:**
>
> Please **DO NOT** use this method if your distro is already supported via **Open Build Service**.
>
> Manual compilation requires a good understanding of the system directory structure. If you encounter "Not Available" errors or missing libraries when installing this way on popular distros (Ubuntu/Fedora...), please return to using Open Build Service for stability and automatic updates.

##### System Requirements

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

##### Compilation and Installation

```bash
# Clone repository
git clone https://github.com/LotusInputMethod/fcitx5-lotus.git
cd fcitx5-lotus

# Compile
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/usr/lib . # LIBDIR will vary depending on the distro
make

# Install (root privileges required)
sudo make install

# Or install to a custom directory
sudo make install PREFIX=/usr/local
```

</details>

---

<a id="enable-input-method"></a>

## ⚙️ Enable Input Method

After installation, you need to follow these steps to enable the fcitx5-lotus input method:

### 1. Enable Lotus Server

The server helps the input method interact better with the system (especially sending backspace and fixing errors).

- **Bash / Zsh:**

```bash
# Enable and start the service (automatically fixes missing systemd user errors if any)
sudo systemctl enable --now fcitx5-lotus-server@$(whoami).service || \
(sudo systemd-sysusers && sudo systemctl enable --now fcitx5-lotus-server@$(whoami).service)
```

- **Fish shell:**

```fish
# Enable and start the service (automatically fixes missing systemd user errors if any)
sudo systemctl enable --now fcitx5-lotus-server@(whoami).service; or begin
    sudo systemd-sysusers; and sudo systemctl enable --now fcitx5-lotus-server@(whoami).service
end
```

```bash
# Check status (if you see green active (running), it's OK)
systemctl status fcitx5-lotus-server@$(whoami).service
```

### 2. Set Environment Variables

The input method will not work without these variables.

<details open>
<summary><b>Bash</b></summary>

```bash
# Add the configuration to ~/.bash_profile
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
# Add the configuration to ~/.zprofile
cat <<EOF >> ~/.zprofile
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx
export GLFW_IM_MODULE=ibus
EOF
```

</details>

<details>
<summary><b>Fish shell</b></summary>

```fish
# Add configuration to ~/.config/fish/config.fish
echo 'if status is-login
    set -Ux GTK_IM_MODULE fcitx
    set -Ux QT_IM_MODULE fcitx
    set -Ux XMODIFIERS "@im=fcitx"
    set -gx SDL_IM_MODULE fcitx
    set -gx GLFW_IM_MODULE ibus
end' >> ~/.config/fish/config.fish
```

</details>

Log out and log in to apply changes.

<details>
<summary><b>If you still cannot type after logging out</b></summary>
<br>

If the configuration in `~/.bash_profile`, `~/.zprofile` or `.config/fish/config.fish` doesn't work, you can try setting it in `/etc/environment` to apply to the entire system:

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

> **Note:** A system restart is required after setup.

</details>

### 3. Turn off old input method (IBus) and add Fcitx5 to Autostart

If your machine is using IBus, turn it off before switching to Fcitx5 to avoid conflicts.

```bash
# Turn off IBus
killall ibus-daemon || ibus exit
```

<details>
<summary><b>Add Fcitx5 to Autostart for each DE / WM (GNOME, Hyprland ...)</b></summary>

| DE / WM        | Detailed Instructions                                                                                                        |
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
| **i3 / Sway**  | Add `exec --no-startup-id fcitx5 -d` to the config file (`~/.config/i3/config` or `~/.config/sway/config`)                   |
| **Hyprland**   | Add `exec-once = fcitx5 -d` to `~/.config/hypr/hyprland.conf`                                                                |

> **Note:** Turn off IBus autostart (usually `ibus-daemon` or `ibus`) to avoid conflicts. It's best to uninstall IBus if not in use.

</details>

### 4. Configure Fcitx5

After logging out and logging in again:

1. Open **Fcitx5 Configuration** (find in application menu or run `fcitx5-configtool`).
2. Find **Lotus** in the right column.
3. Click the **<** arrow to add it to the left column.
4. Apply.
<details>
<summary><b>Additional configuration for Wayland (KDE, Hyprland)</b></summary>

- **KDE Plasma:** _System Settings_ → _Keyboard_ → _Virtual Keyboard_ → Select **Fcitx 5**.
- **Hyprland:** Add the following line to `~/.config/hypr/hyprland.conf`:
  ```ini
  permission = fcitx5-lotus-server, keyboard, allow
  ```
  </details>

---

<a id="usage-guide"></a>

## 📖 Usage guide

### 1. Customize input method

- **Access:** Right-click the Lotus icon on the system tray to open customization.
- **Customization options:** _Typing Mode_, _Input Method_, _Charset_, _Spell Check_, _Macro_, _Capitalize Macro_, _Auto non-VN restore_, _Modern Style_, _Free Marking_, _Fix Uinput with ack_, _Lotus status icons_, _Mode menu_.

### 2. Typing mode menu

In any application, press the **`** key to open the typing mode selection menu, where you can use the mouse or shortcuts to select. If the backtick shortcut interferes with your workflow (e.g. when writing Markdown code fences), you can turn it off in Lotus' settings using the **Mode menu** toggle.

| Mode                  | Shortcut | Description                                                                                                                                         |
| :-------------------- | :------: | :-------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Uinput (Smooth)**   |  **1**   | Default mode, fast response.<br>**Optimal:** applications with high input processing speed.                                                         |
| **Uinput (Slow)**     |  **2**   | Similar to Uinput (Smooth) but with a slower key-sending speed.<br>**Optimal:** applications with low input processing speed _(e.g., LibreOffice)_. |
| **Uinput (Hardcore)** |  **3**   | A variant of Uinput (Smooth).<br>**Optimal:** Windows applications via Wine.                                                                        |
| **Surrounding Text**  |  **4**   | Allows editing marks on typed text, works smoothly. <br> **Optimal:** Qt/GTK applications.                                                          |
| **Preedit**           |  **Q**   | Displays underline while typing. <br> **Optimal:** most applications.                                                                               |
| **Emoji Picker**      |  **W**   | Search and input Emojis (EmojiOne source, fuzzy search supported).                                                                                  |
| **OFF**               |  **E**   | Turn off input method.                                                                                                                              |
| **Default Typing**    |  **R**   | Default typing mode configured in the _Typing mode_ option.                                                                                         |
| **Type `**            |  **`**   | Type the **`** character.                                                                                                                           |

The input method automatically saves the most recently used typing mode for each application and restores that configuration when you reopen them.

### 3. Reset typing state

Clicking the mouse or touching the touchpad while typing will automatically reset the typing state, preventing character sticking between words.

---

<a id="uninstallation"></a>

## 🗑️ Uninstallation

<details>
<summary><b>Arch / Arch-based - AUR</b></summary>
<br>

You can use `pacman` (recommended), `yay`, or `paru` to uninstall:

```bash
sudo pacman -Rns fcitx5-lotus
```

```bash
yay -Rns fcitx5-lotus
```

```bash
paru -Rns fcitx5-lotus
```

> **Note:** Config files in `$HOME` will be kept.

</details>

<details>
<summary><b>Debian / Ubuntu / Fedora / openSUSE - Open Build Service</b></summary>
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

Delete (or comment out) the `services.fcitx5-lotus` and `inputs` lines in the config file, then rebuild the system. NixOS will clean up automatically.

</details>

<details>
<summary><b>Build from source</b></summary>
<br>

Go back to the built source code directory and run:

```bash
sudo make uninstall
```

</details>

---

<a id="contributing"></a>

## 🤝 Contributing

Contributions are what make the open-source community such an amazing place to learn, inspire, and create. Any contribution you make is **greatly appreciated**.

Please see the detailed guide [here](CONTRIBUTING.en.md) for how to participate in project development, the Pull Request process, code style rules, and **code of conduct**.

Don't forget to give the project a ⭐! Thank you very much!

---

<a id="license"></a>

## 📃 License

The project is distributed under the GNU General Public License v3. See [`LICENSE`](LICENSE) for details.

---

## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

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

This project follows the [all-contributors](https://github.com/all-contributors/all-contributors) specification. Contributions of any kind welcome!

---

## ✨ Star History

<a href="https://star-history.com/#LotusInputMethod/fcitx5-lotus&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=LotusInputMethod/fcitx5-lotus&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=LotusInputMethod/fcitx5-lotus&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=LotusInputMethod/fcitx5-lotus&type=date&legend=top-left" />
 </picture>
</a>
