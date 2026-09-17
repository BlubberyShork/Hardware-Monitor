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

WDK_BIN = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64"
SIGNTOOL = os.path.join(WDK_BIN, "signtool.exe")

def is_admin():
    try:
        # Checks if the script is running with administrative privileges
        return ctypes.windll.shell32.IsUserAnAdmin()
    except Exception:
        return False


#######################################
### Windows test-signing helpers ###
def windowsEnsureTestCert(cert_name):
    check_cmd = [
        "powershell", "-Command",
        f"Get-ChildItem Cert:\\CurrentUser\\My -CodeSigningCert "
        f"| Where-Object {{$_.Subject -eq 'CN={cert_name}'}}"
    ]
    result = subprocess.run(check_cmd, capture_output=True, text=True)

    if result.stdout.strip():
        print(f'Test certificate "{cert_name}" already exists.')
        return

    print(f'Creating test certificate "{cert_name}"...')
    create_cmd = [
        "powershell", "-Command",
        f"New-SelfSignedCertificate -Type CodeSigningCert "
        f"-Subject 'CN={cert_name}' "
        f"-CertStoreLocation 'Cert:\\CurrentUser\\My'"
    ]
    try:
        subprocess.run(create_cmd, check=True)
        print(f'Test certificate "{cert_name}" created.')
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to create test certificate. {e}")
        sys.exit(1)


def windowsSignDriver(sys_path, cert_name):
    if not os.path.isfile(SIGNTOOL):
        print(f"Error: signtool.exe not found at {SIGNTOOL}")
        sys.exit(1)

    print(f"Signing {sys_path}...")
    sign_cmd = [
        SIGNTOOL, "sign",
        "/v",
        "/s", "My",
        "/n", cert_name,
        "/fd", "sha256",
        str(sys_path)
    ]
    try:
        subprocess.run(sign_cmd, check=True)
        print(f"Successfully signed {sys_path}")
    except subprocess.CalledProcessError as e:
        print(f"Error: Signing failed. {e}")
        sys.exit(1)


def windowsCheckTestSigningMode():
    try:
        result = subprocess.run(
            ["bcdedit", "/enum", "{current}"],
            capture_output=True, text=True
        )
        output = result.stdout.lower()
        if "testsigning" not in output or "yes" not in output:
            print("WARNING: Test signing mode may not be enabled.")
            print('  Run "bcdedit /set testsigning on" as admin and reboot.')
    except Exception:
        print("WARNING: Could not check test signing status (requires admin).")


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

    # Test-sign the built driver
    driver_name = "WindowsCPUDriver"
    sys_path = target_dir / build_dir / f"{driver_name}.sys"
    if sys_path.exists():
        windowsEnsureTestCert(driver_name)
        windowsSignDriver(sys_path, driver_name)
    else:
        print(f"Warning: Built driver not found at {sys_path}, skipping signing.")

    return_to_root()

# TODO - Eventually, CPUMonitorDriver/ will be drivers/ with their appropriate cpu/, motherboard/, etc.
# We will have a function to search and find driver .sys files to determine binpaths and driver names (third arg in both sc.exe cmds) 
def windowsDeployDrivers():
    windowsCheckTestSigningMode()

    driver_name = "WindowsCPUDriver"
    bin_path = root_dir.joinpath("drivers/windows/CPU/CPUMonitorDriver").joinpath(build_dir).joinpath(f"{driver_name}.sys")

    # Clean up any existing service registration before creating
    query_result = subprocess.run(
        ["sc.exe", "query", driver_name],
        capture_output=True, text=True
    )
    if query_result.returncode == 0:
        print(f'Service "{driver_name}" already exists, stopping and removing...')
        subprocess.run(["sc.exe", "stop", driver_name], capture_output=True)
        subprocess.run(["sc.exe", "delete", driver_name], check=True)

    # sc.exe create
    try:
        subprocess.run(
            ["sc.exe", "create", driver_name, "binPath=", str(bin_path), "type=", "kernel"],
            check=True
        )
    except subprocess.CalledProcessError as e:
        print(f"Error: sc.exe create failed with exit code {e.returncode}.")
        return

    # KMDF drivers loaded via sc.exe need the WDF version registry key,
    # otherwise FxDriverEntry can't initialize the framework.
    kmdf_version = "1.15"
    wdf_reg_key = f"HKLM\\System\\CurrentControlSet\\Services\\{driver_name}\\Parameters\\Wdf"
    try:
        subprocess.run(
            ["reg", "add", wdf_reg_key,
             "/v", "KmdfLibraryVersion", "/t", "REG_SZ", "/d", kmdf_version, "/f"],
            check=True
        )
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to set KMDF version registry key (exit code {e.returncode}).")
        return

    # sc.exe start
    try:
        subprocess.run(
            ["sc.exe", "start", driver_name],
            check=True
        )
    except subprocess.CalledProcessError as e:
        print(f"Error: sc.exe start failed with exit code {e.returncode}.")


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
