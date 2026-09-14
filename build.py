# build.py

import platform
import os
import sys
import subprocess        # Will use to run developer powershell for windows, will be tricky to resolve pathing for general use
import contextlib
import argparse          # Fine-grain cmd-line argument handling

from pathlib import Path # Enables curr-working directory handling. Modern, clean file path resolution methodologies

# Cmake configuration command
VCPKG_PATH = Path("C:\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake")

CONFIGURATION_CMD = ["cmake", "-B", "build", "-S", ".", "-G", "Ninja", f"-DCMAKE_TOOLCHAIN_FILE={VCPKG_PATH}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DUA_ENABLE_ENCRYPTION=MBEDTLS"]

# Cmake build command
build_dir = "build"
BUILD_CMD = ["cmake", "--build", build_dir]

# Target directory
target_dir = Path.cwd().joinpath('System_Info')

# Root directory
root_dir = Path.cwd()

###
# Linux build
def linuxBuild():
    pass

###
# Windows build -> uses MSVC, so its a bit tricky
def windowsBuild():
    VCVARSALL = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
    ARCH = "x64"

    # MSVC uses the vcvarsall.bat file to configure environment variables
    #   These env variables are being passed to Ninja for ninja.build configuration
    def get_vcvars_env(vcvarsall_path, arch="x64"):
        cmd = f'"{vcvarsall_path}" {arch} && set'
        output = subprocess.check_output(cmd, shell=True, text=True)
        env = os.environ.copy()
        
        # Environment var definitions
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

# Returns to root dir of project
def return_to_root():
    try:
        os.chdir(root_dir)
    except OSError as e:
        print(f'Error changing to directory "{root_dir}": {e}')

# Build without running
def build():
    op_sys = platform.system()
    if op_sys == 'Windows':
        windowsBuild()
    elif op_sys == 'Linux':
        linuxBuild()
    else:
        raise OSError('Unsupported Operating System. This project only supports Windows and Linux environments!')

# Run built executable
def run(ret):
    try:
        os.chdir(target_dir.joinpath(build_dir))
    except OSError as e:
        print(f"Error: could not change to build directory: {target_dir.joinpath(build_dir)}: {e}")
    
    subprocess.run("SystemInfo.exe", check=True)
    
    if ret:
        return_to_root()

# Build and run
def build_and_run():
    build()
    run(False)
    return_to_root()

def main():
    parser = argparse.ArgumentParser(
        description="Build/run helper script."
    )
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
