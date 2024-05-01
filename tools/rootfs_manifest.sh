#!/usr/bin/env bash
# Shared list of root filesystem files (same layout as the former packed initrd).
# Source with ROOT set to the OpenHobbyOS tree. Defines ohos_rootfs_append_entries
# which appends "SRC::/dst/path" lines to the array name passed as $1.

ohos_rootfs_append_entries() {
    local -n _out="$1"

    _out+=("$ROOT/assets/etc/motd.txt::/etc/motd.txt")
    _out+=("$ROOT/assets/etc/goshrc::/etc/goshrc")
    _out+=("$ROOT/assets/etc/os-release::/etc/os-release")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/config.jsonc::/etc/xdg/fastfetch/config.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/minimal.jsonc::/etc/xdg/fastfetch/minimal.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo.txt::/etc/xdg/fastfetch/ohos_logo.txt")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo_small.txt::/etc/xdg/fastfetch/ohos_logo_small.txt")
    _out+=("$ROOT/assets/shell-colors.jsonc::/etc/shell-colors.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/config.jsonc::/root/.config/fastfetch/config.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/minimal.jsonc::/root/.config/fastfetch/minimal.jsonc")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo.txt::/root/.config/fastfetch/ohos_logo.txt")
    _out+=("$ROOT/assets/etc/xdg/fastfetch/ohos_logo_small.txt::/root/.config/fastfetch/ohos_logo_small.txt")
    _out+=("$ROOT/build/user/hello.elf::/bin/hello")
    _out+=("$ROOT/build/user/uname.elf::/bin/uname")
    _out+=("$ROOT/build/user/sh.elf::/bin/sh")

    # Coreutils part starts from here.
    for _cu in cat chmod cp cut dd date dirname echo false fmt fold head ln link \
               ls mkdir mv paste pr printf rm rmdir seq sleep sort sum sync \
               tac tail tee touch tr true truncate unlink wc yes; do
        if [[ -f "$ROOT/build/ports/sysroot/bin/$_cu" ]]; then
            _out+=("$ROOT/build/ports/sysroot/bin/$_cu::/bin/$_cu")
        fi
    done
    if [[ -f "$ROOT/build/ports/sysroot/bin/gosh" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/gosh::/bin/gosh")
    fi
    _out+=("$ROOT/build/user/test_fb.elf::/bin/test_fb")
    _out+=("$ROOT/build/user/net_test.elf::/bin/net_test")
    _out+=("$ROOT/build/user/net_info.elf::/bin/net_info")
    _out+=("$ROOT/build/user/ohloader.elf::/bin/ohloader")
    _out+=("$ROOT/build/user/ohpleasepanic.elf::/bin/ohpleasepanic")
    _out+=("$ROOT/build/user/login.elf::/bin/login")

    if [[ -f "$ROOT/build/user/ohpkg_read.elf" ]]; then
        _out+=("$ROOT/build/user/ohpkg_read.elf::/bin/ohpkg-read")
    fi
    if [[ -f "$ROOT/build/user/ohpkg_run.elf" ]]; then
        _out+=("$ROOT/build/user/ohpkg_run.elf::/bin/ohpkg-run")
    fi

    # .ohpkg application packages
    local ohpkg_dir="$ROOT/build/ohpkg"
    if [[ -d "$ohpkg_dir" ]]; then
        for f in "$ohpkg_dir"/*.ohpkg; do
            if [[ -f "$f" ]]; then
                local name
                name=$(basename "$f")
                _out+=("$f::/bin/$name")
            fi
        done
    fi

    if [[ -f "$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch" ]]; then
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/bin/fastfetch")
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/bin/fetch")
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/usr/bin/fastfetch")
        _out+=("$ROOT/build/ports/fastfetch/install/usr/bin/fastfetch::/usr/bin/fastfetch.upstream")
    fi

    local preset
    for preset in all.jsonc neofetch.jsonc archey.jsonc screenfetch.jsonc; do
        if [[ -f "$ROOT/build/ports/fastfetch/install/usr/share/fastfetch/presets/$preset" ]]; then
            _out+=("$ROOT/build/ports/fastfetch/install/usr/share/fastfetch/presets/$preset::/usr/share/fastfetch/presets/$preset")
        fi
    done

    if [[ -f "$ROOT/build/ports/xnx/install/bin/xnx-compositor" ]]; then
        _out+=("$ROOT/build/ports/xnx/install/bin/xnx-compositor::/bin/xnx-compositor")
        _out+=("$ROOT/build/ports/xnx/install/bin/xnx-compositor::/usr/bin/xnx-compositor")
    fi

    if [[ -f "$ROOT/build/ports/xnx/install/bin/xnx-demo" ]]; then
        _out+=("$ROOT/build/ports/xnx/install/bin/xnx-demo::/bin/xnx-demo")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/installer" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/installer::/bin/installer")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/milkyway" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/milkyway::/bin/milkyway")
    fi

    if [[ -f "$ROOT/build/ports/ohplay/install/bin/ohplay" ]]; then
        _out+=("$ROOT/build/ports/ohplay/install/bin/ohplay::/bin/ohplay")
        _out+=("$ROOT/build/ports/ohplay/install/bin/ohplay::/bin/play")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/doom" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/doom::/bin/doom")
    fi


    if [[ -f "$ROOT/assets/fonts/Monospace/Monospace.ttf" ]]; then
        _out+=("$ROOT/assets/fonts/Monospace/Monospace.ttf::/fonts/Monospace.ttf")
    fi

    if [[ -f "$ROOT/assets/Doom1.WAD" ]]; then
        _out+=("$ROOT/assets/Doom1.WAD::/doom1.wad")
        _out+=("$ROOT/assets/Doom1.WAD::/DOOM1.WAD")
        _out+=("$ROOT/assets/Doom1.WAD::/bin/doom1.wad")
        _out+=("$ROOT/assets/Doom1.WAD::/root/doom1.wad")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/gears" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/gears::/bin/gears")
    fi

    if [[ -d "$ROOT/build/ports/sysroot/bin" ]]; then
        for coreutil in basename chmod cmp dd hostname logname mktemp nice nproc \
                        od pathchk readlink realpath stat sum test \
                        uname uniq whoami; do
            if [[ -f "$ROOT/build/ports/sysroot/bin/$coreutil" ]]; then
                _out+=("$ROOT/build/ports/sysroot/bin/$coreutil::/bin/$coreutil")
            fi
        done
    fi

    # Toolbox fallback keeps the live shell usable even when coreutils
    # applets are missing from the current build.
    if [[ -f "$ROOT/build/user/toolbox.elf" ]]; then
        _out+=("$ROOT/build/user/toolbox.elf::/bin/toolbox")
        for tool in ls cat stat pwd env id echo clear sleep mkdir parallel yield \
                    poweroff reboot suspend help uname; do
            _out+=("$ROOT/build/user/toolbox.elf::/bin/$tool")
        done
    fi


    if [[ -d "$ROOT/build/ports/sysroot/plugins/platforms" ]]; then
        for plugin in "$ROOT/build/ports/sysroot/plugins/platforms/"*.so; do
            if [[ -f "$plugin" ]]; then
                local plugin_name
                plugin_name=$(basename "$plugin")
                _out+=("$plugin::/usr/plugins/platforms/$plugin_name")
            fi
        done
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/test_write" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/test_write::/bin/test_write")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/qt_demo" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/qt_demo::/bin/qt_demo")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/bin/gtkdemo" ]]; then
        _out+=("$ROOT/build/ports/sysroot/bin/gtkdemo::/bin/gtkdemo")
    fi

    if [[ -f "$ROOT/build/ports/sysroot/lib/ld-openhobbyos.so.1" ]]; then
        _out+=("$ROOT/build/ports/sysroot/lib/ld-openhobbyos.so.1::/usr/lib/ld-openhobbyos.so.1")
    fi
}
