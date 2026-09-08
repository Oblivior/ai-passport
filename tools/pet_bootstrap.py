#!/usr/bin/env python3
"""Set up a private runtime without simultaneous pip/installer processes."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from pet_install import DATA_HOME, exclusive, private_dir


def setup(home=DATA_HOME, confirm=input):
    requirements = Path(__file__).with_name("requirements-pet-installer.txt")
    with exclusive(home):
        runtime = private_dir(Path(home) / "runtime-v1")
        python = runtime / "bin/python"
        marker = runtime / ".ready"
        if not python.is_file() or not marker.is_file() or marker.read_bytes() != requirements.read_bytes():
            if confirm("将从公开 PyPI 安装固定版本依赖到本机私有环境。输入 YES 继续：").strip() != "YES":
                return None
            if not python.is_file():
                subprocess.run([sys.executable, "-m", "venv", str(runtime)], check=True)
            subprocess.run([str(python), "-m", "pip", "--isolated", "install", "--index-url",
                            "https://pypi.org/simple", "-r", str(requirements)], check=True)
            with tempfile.NamedTemporaryFile(dir=runtime, delete=False) as file:
                file.write(requirements.read_bytes())
                file.flush()
                os.fsync(file.fileno())
            os.replace(file.name, marker)
        return python


if __name__ == "__main__":
    try:
        os.umask(0o077)
        python = setup()
        if python:
            raise SystemExit(subprocess.call([str(python), "-u", str(Path(__file__).with_name("pet_install.py"))]))
    except (KeyboardInterrupt, EOFError):
        print("\n已取消。")
        sys.exit(130)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print("启动未完成：", str(exc) if isinstance(exc, ValueError) else type(exc).__name__)
        print("请检查网络和 Python，重新打开启动器重试；未自动修改系统环境。")
        sys.exit(1)
