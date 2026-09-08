#!/bin/bash
set -euo pipefail
umask 077
cd -- "$(dirname -- "$0")"
finish() { read -r -p '按回车关闭窗口…' _ || true; }
trap finish EXIT
if [[ "$(uname -s)" != Darwin ]]; then
  echo '此内测包仅用于 macOS 12.3+。'
  exit 1
fi
echo '数码宝贝 Passport · Mac 内测'
echo '首次需要联网下载 Python 依赖；不需要 Git、ESP-IDF、Codex 或管理员权限。'
pet_python=''
for candidate in /Library/Frameworks/Python.framework/Versions/3.13/bin/python3 /Library/Frameworks/Python.framework/Versions/3.12/bin/python3 /Library/Frameworks/Python.framework/Versions/3.11/bin/python3 /opt/homebrew/bin/python3 /usr/local/bin/python3; do
  if [[ -x "$candidate" ]] && "$candidate" -c 'import sys; sys.exit(not ((3,9) <= sys.version_info[:2] < (3,14)))' 2>/dev/null; then
    pet_python="$candidate"
    break
  fi
done
if [[ -z "$pet_python" ]]; then
  echo '未找到支持的 Python 3.9–3.13。请先从 https://www.python.org/downloads/macos/ 安装 Python 3.12，再双击本文件。'
  echo '本工具不会自动安装 Xcode、Homebrew 或修改系统 Python。'
  exit 1
fi
"$pet_python" -u tools/pet_bootstrap.py
