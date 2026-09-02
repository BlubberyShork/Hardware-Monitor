# Windows Drivers Build Script
import platform
import subprocess
import os
import sys
import ctypes
import argparse

from pathlib import Path

root_dir = Path.cwd()
def return_to_root():
    try:
        os.chdir(root_dir)
    except OSError as e:
        print(f'Error changing to directory "{root_dir}": {e}')


CONFIG_CMD = ["cmake", "-B", "build", "-S", ".", "-G", "Ninja"]

build_dir = "build"
BUILD_CMD = ["cmake", "--build", build_dir]

def is_admin():
    try:
        # Checks if the script is running with administrative privileges
        return ctypes.windll.shell32.IsUserAnAdmin()
    except Exception:
        return False


# TODO - Eventually, CPUMonitorDriver/ will be drivers/ with their appropriate cpu/, motherboard/, etc.
# We will have a function to search and find driver .sys files to determine binpaths and driver names (third arg in both sc.exe cmds) 
#######################################
### Windows arguments/helpers logic ###
def windowsBuildDrivers():
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
    
    target_dir = root_dir.joinpath("drivers/windows/CPU/CPUMonitorDriver")
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
            CONFIG_CMD,
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

    return_to_root()

# TODO - Eventually, CPUMonitorDriver/ will be drivers/ with their appropriate cpu/, motherboard/, etc.
# We will have a function to search and find driver .sys files to determine binpaths and driver names (third arg in both sc.exe cmds) 
def windowsDeployDrivers():
    bin_path = ""
    SC_CREATE_CPUDRIVER_CMD = ["sc.exe", "create", "CPUMonitorDriver", f'binPath="{bin_path}"', "type=kernel"]
    SC_START_CPUDRIVER_CMD = ["sc.exe", "start", "CPUMonitorDriver"]

    # sc.exe create
    try:
        subprocess.run(
            SC_CREATE_CPUDRIVER_CMD,
            check=True
        )
    except FileNotFoundError as e:
        print(f"Error: The executable could not be found. Details: {e}")
    except subprocess.TimeoutExpired as e:
        print(f"Error: Command timed out after {e.timeout} seconds.")
    except subprocess.CalledProcessError as e:
        print(f"Error: Command '{e.cmd}' failed with exit code {e.returncode}.")

    # sc.exe start
    try:
        subprocess.run(
            SC_START_CPUDRIVER_CMD,
            check=True
        )
    except subprocess.TimeoutExpired as e:
        print(f"Error: Command timed out after {e.timeout} seconds.")
    except subprocess.CalledProcessError as e:
        print(f"Error: Command '{e.cmd}' failed with exit code {e.returncode}.")


def windowsBuildAndDeployDrivers():
    windowsBuildDrivers()
    windowsDeployDrivers()

########################################
### Linux argument & Helper logic ###
def linuxBuildDrivers():
    pass

def linuxDeployDrivers():
    pass

def linuxBuildAndDeployDrivers():
    pass

########################################
### Generic Top-Level argument logic ###
def buildDrivers():
    op_sys = platform.system()
    if op_sys == 'Windows':
        windowsBuildDrivers()
    elif op_sys == 'Linux':
        linuxBuildDrivers()
    else:
        raise OSError('Unsupported Operating System. This project only supports Windows and Linux environments!')

def deployDrivers():
    op_sys = platform.system()
    if op_sys == 'Windows':
        windowsDeployDrivers()
    elif op_sys == 'Linux':
        linuxDeployDrivers()
    else:
        raise OSError('Unsupported Operating System. This project only supports Windows and Linux environments!')

def buildAndDeployDrivers():
    op_sys = platform.system()
    if op_sys == 'Windows':
        windowsBuildAndDeployDrivers()
    elif op_sys == 'Linux':
        linuxBuildAndDeployDrivers()
    else:
        raise OSError('Unsupported Operating System. This project only supports Windows and Linux environments!')

############
### Main ###
def main():
    parser = argparse.ArgumentParser(description = "Drivers build script helper")
    
    group = parser.add_mutually_exclusive_group(required = True)
    group.add_argument("--build", "-b", action = "store_true", help = "builds the drivers")
    group.add_argument("--deploy", "-d", action = "store_true", help = "deploys the existing drivers")
    group.add_argument("--build-and-deploy", "-bd", action = "store_true", help = "builds and deploys all existing project drivers")

    args = parser.parse_args()
    # TODO -> Change these to generic build(), deploy(), build_and_deploy() functions that check for the platform version
    if args.build:
        windowsBuildDrivers()    
    elif args.deploy:
        windowsDeployDrivers()
    elif args.build_and_deploy:
        windowsBuildAndDeployDrivers()


if __name__ == "__main__":
    main()
