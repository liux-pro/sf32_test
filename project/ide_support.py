import os
import json
from typing import List, Optional

PROJECT_DIR = os.path.join('..')


def ide_support(target, source, env) -> None:
    """
    生成IDE相关配置（CLion + VS Code clangd 共用）：

    1. compile_commands.json
       - CLion：直接读取项目根目录的编译数据库
       - VS Code：clangd 插件同样读取 compile_commands.json
    2. .vscode/settings.json
       - 为 VS Code 添加 SF32 的 PowerShell 终端并设为默认
       - 启用自动保存

    参数:
        env: SCons构建环境对象
    """
    env_vars = json.loads(env.Dump(format="json"))

    # ---------- 1. 生成 compile_commands.json ----------
    build_dir = env_vars["BUILD_DIR_FULL_PATH"]
    compile_db_path = os.path.join(build_dir, "compile_commands.json")

    compiler_path = _find_compiler(env_vars)
    if not compiler_path:
        raise RuntimeError(f"编译器 '{env_vars['CC']}' 在PATH中未找到")

    processed_commands = _process_compile_commands(
        compile_db_path, compiler_path)

    output_path = os.path.join(PROJECT_DIR, 'compile_commands.json')
    with open(output_path, 'w') as output_file:
        json.dump(processed_commands, output_file, indent=4)
        print(f"✓ 已生成 compile_commands.json (CLion / VS Code clangd): {output_path}")

    # ---------- 2. 更新 VS Code settings.json ----------
    vscode_dir = os.path.join(PROJECT_DIR, '.vscode')
    os.makedirs(vscode_dir, exist_ok=True)

    # 根据环境变量找到 export.ps1 位置
    export_file = os.path.join(os.environ.get("SIFLI_SDK_PATH"), "export.ps1")
    settings_file = os.path.join(vscode_dir, 'settings.json')
    try:
        with open(settings_file, 'r', encoding='utf-8') as f:
            settings = json.load(f)
    except Exception:
        # 处理空文件或无效JSON
        settings = {}

    # 添加sf32的shell
    terminal_profiles = settings.setdefault(
        "terminal.integrated.profiles.windows", {})
    terminal_profiles["SF32"] = {
        "path": "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
        "args": [
            "-ExecutionPolicy",
            "Bypass",
            "-NoExit",
            "-File",
            export_file
        ]
    }
    # 将其设为默认
    settings["terminal.integrated.defaultProfile.windows"] = "SF32"
    # 激活自动保存(默认1秒)
    settings["files.autoSave"] = "afterDelay"

    with open(settings_file, 'w', encoding='utf-8') as f:
        json.dump(settings, f, indent=4, ensure_ascii=False)
        print(f"✓ 已更新 VS Code settings.json: {settings_file}")


def _find_compiler(env_vars: dict) -> Optional[str]:
    """在PATH中定位编译器绝对路径"""
    return find_executable_in_path(
        executable_name=env_vars["CC"],
        path_env=env_vars["ENV"]["PATH"]
    )


def _process_compile_commands(db_path: str, compiler_path: str) -> List[dict]:
    """
    处理编译命令数据库

    参数:
        db_path: 原始compile_commands.json路径
        compiler_path: 编译器绝对路径

    返回:
        处理后的编译命令列表
    """
    with open(db_path, 'r', encoding='utf-8') as db_file:
        commands = json.load(db_file)

    processed = []
    for entry in commands:
        # 跳过不存在的源文件
        source_path = _resolve_source_path(entry)
        if not os.path.exists(source_path):
            continue

        # 替换编译器路径
        if "command" in entry:
            tokens = entry["command"].split(" ")
            tokens[0] = compiler_path
            entry["command"] = " ".join(tokens)

        processed.append(entry)

    return processed


def _resolve_source_path(entry: dict) -> str:
    """解析源文件的绝对路径"""
    source = entry["file"]
    return source if os.path.isabs(source) else os.path.join(entry["directory"], source)


def find_executable_in_path(
    executable_name: str,
    path_env: Optional[str] = None
) -> Optional[str]:
    """
    在系统PATH中定位可执行文件

    参数:
        executable_name: 可执行文件名
        path_env: 可选的PATH环境变量字符串

    返回:
        找到的可执行文件完整路径，未找到则返回None
    """
    # 获取系统PATH（如果未提供）
    path_str = path_env or os.environ.get('PATH', '')

    # 根据平台确定路径分隔符
    separator = ';' if os.name == 'nt' else ':'

    for directory in path_str.split(separator):
        dir_path = directory.strip()
        if not dir_path:
            continue

        # 尝试基本路径匹配
        candidate = os.path.join(dir_path, executable_name)
        if os.path.exists(candidate):
            return candidate

        # Windows平台尝试常见扩展名
        if os.name == 'nt':
            for ext in ('.exe', '.com', '.bat', '.cmd'):
                if os.path.exists(candidate + ext):
                    return candidate + ext

    return None