# build_control_center.py

import platform
import os
import subprocess
import argparse

from pathlib import Path

MBEDTLS_INCLUDE = "C:\\mbedtls\\include"
MBEDTLS_LIBRARY = "C:\\mbedtls\\build\\library\\Debug\\mbedtls.lib"
MBEDX509_LIBRARY = "C:\\mbedtls\\build\\library\\Debug\\mbedx509.lib"
MBEDCRYPTO_LIBRARY = "C:\\mbedtls\\build\\library\\Debug\\mbedcrypto.lib"

target_dir = Path.cwd().joinpath("control_center")
build_dir = "build"
root_dir = Path.cwd()

CONFIGURATION_CMD = [
    "cmake", "-B", build_dir, "-S", ".", "-G", "Ninja",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "-DUA_ENABLE_ENCRYPTION=MBEDTLS",
    f"-DMBEDTLS_INCLUDE_DIRS={MBEDTLS_INCLUDE}",
    f"-DMBEDTLS_LIBRARY={MBEDTLS_LIBRARY}",
    f"-DMBEDX509_LIBRARY={MBEDX509_LIBRARY}",
    f"-DMBEDCRYPTO_LIBRARY={MBEDCRYPTO_LIBRARY}",
    "-DUA_LOGLEVEL=100"
]
BUILD_CMD = ["cmake", "--build", build_dir]


def return_to_root():
    try:
        os.chdir(root_dir)
    except OSError as e:
        print(f'Error changing to directory "{root_dir}": {e}')


###
# Linux build
def linuxBuild():
    pass

###
# Windows build
def windowsBuild():
    VCVARSALL = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    ARCH = "x64"

    def get_vcvars_env(vcvarsall_path, arch="x64"):
        cmd = f'"{vcvarsall_path}" {arch} && set'
        output = subprocess.check_output(cmd, shell=True, text=True)
        env = os.environ.copy()
        for line in output.splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                env[k] = v
        return env

    try:
        os.chdir(target_dir)
    except OSError as e:
        print(f'Error changing to directory "{target_dir}": {e}')

    try:
        os.makedirs(build_dir, exist_ok=True)
    except OSError as e:
        print(f'Error creating build directory "{Path.cwd().joinpath(build_dir)}": {e}')

    env = get_vcvars_env(VCVARSALL, ARCH)

    # Cmake Ninja Configuration
    try:
        subprocess.run(
            CONFIGURATION_CMD,
            env=env,
            check=True
        )
    except FileNotFoundError as e:
        print(f"Error: The executable could not be found. Details: {e}")
    except subprocess.TimeoutExpired as e:
        print(f"Error: Command timed out after {e.timeout} seconds.")
    except subprocess.CalledProcessError as e:
        print(f"Error: Command '{e.cmd}' failed with exit code {e.returncode}.")

    # Cmake build
    try:
        subprocess.run(
            BUILD_CMD,
            env=env,
            check=True
        )
    except FileNotFoundError as e:
        print(f"Error: The executable could not be found. Details: {e}")
    except subprocess.TimeoutExpired as e:
        print(f"Error: Command timed out after {e.timeout} seconds.")
    except subprocess.CalledProcessError as e:
        print(f"Error: Command '{e.cmd}' failed with exit code {e.returncode}.")


def build():
    op_sys = platform.system()
    if op_sys == 'Windows':
        windowsBuild()
    elif op_sys == 'Linux':
        linuxBuild()
    else:
        raise OSError('Unsupported Operating System. This project only supports Windows and Linux environments!')
    return_to_root()


def run(ret):
    try:
        os.chdir(target_dir.joinpath(build_dir))
    except OSError as e:
        print(f"Error: could not change to build directory: {target_dir.joinpath(build_dir)}: {e}")

    subprocess.run("ControlCenter.exe", check=True)

    if ret:
        return_to_root()


def build_and_run():
    build()
    run(False)
    return_to_root()


def main():
    parser = argparse.ArgumentParser(description="Control center build/run helper.")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("-b", "--build", action="store_true", help="Build only")
    group.add_argument("-br", "--build-run", action="store_true", help="Build and run")
    group.add_argument("-r", "--run", action="store_true", help="Run only")

    args = parser.parse_args()

    if args.build:
        build()
    elif args.build_run:
        build_and_run()
    elif args.run:
        run(True)


if __name__ == "__main__":
    main()
