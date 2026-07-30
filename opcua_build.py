# opcua_build.py

import platform
import os
import subprocess
import argparse

from pathlib import Path

VCPKG_PATH = Path("C:\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake")

target_dir = Path.cwd().joinpath("OPC_UA")
build_dir = "build"
root_dir = Path.cwd()

CONFIGURATION_CMD = [
    "cmake", "-B", build_dir, "-S", ".", "-G", "Ninja",
    f"-DCMAKE_TOOLCHAIN_FILE={VCPKG_PATH}",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "-DUA_ENABLE_ENCRYPTION=MBEDTLS",
]
BUILD_CMD = ["cmake", "--build", build_dir]


def get_vcvars_env(vcvarsall_path, arch="x64"):
    cmd = f'"{vcvarsall_path}" {arch} && set'
    output = subprocess.check_output(cmd, shell=True, text=True)
    env = os.environ.copy()
    for line in output.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            env[k] = v
    return env


def build():
    if platform.system() != "Windows":
        raise OSError("build_opcua.py currently only supports Windows.")

    vcvarsall = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

    os.chdir(target_dir)
    os.makedirs(build_dir, exist_ok=True)

    env = get_vcvars_env(vcvarsall)

    subprocess.run(CONFIGURATION_CMD, env=env, check=True)
    subprocess.run(BUILD_CMD, env=env, check=True)

    os.chdir(root_dir)


def run():
    os.chdir(target_dir.joinpath(build_dir))
    subprocess.run("OpcUaServer.exe", check=True)
    os.chdir(root_dir)


def main():
    parser = argparse.ArgumentParser(description="OPC UA server build/run helper.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("-b", "--build", action="store_true", help="Build only")
    group.add_argument("-br", "--build-run", action="store_true", help="Build and run")
    group.add_argument("-r", "--run", action="store_true", help="Run only")

    args = parser.parse_args()

    if args.build:
        build()
    elif args.build_run:
        build()
        run()
    elif args.run:
        run()


if __name__ == "__main__":
    main()
