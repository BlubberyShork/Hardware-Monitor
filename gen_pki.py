#!/usr/bin/env python3
"""
gen_pki.py — local CA + device certificate generator for OPC UA (open62541).
 
All crypto is delegated to the system `openssl` binary via subprocess
 
Layout produced:
 
    pki/
        ca/
            ca.key          # CA private key (keep this off any deployed device)
            ca.crt          # CA public cert (goes into every device's trust list)
        devices/
            <name>/
                <name>.key  # device private key (stays on that device only)
                <name>.crt  # device cert, signed by ca.key
 
Usage:
    # one-time
    python gen_pki.py init-ca
 
    # once per device (server, qt-frontend, plc-sim, ...)
    python gen_pki.py add-device server --uri urn:myorg:telemetry:server --dns localhost --ip 127.0.0.1
    python gen_pki.py add-device plc-sim --uri urn:myorg:telemetry:plc-sim --dns plc-sim.local
"""
 
import argparse
import ipaddress
import os
import secrets
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
 
PKI_ROOT = Path("pki")
CA_DIR = PKI_ROOT / "ca"
DEVICES_DIR = PKI_ROOT / "devices"
 
CA_KEY_PATH = CA_DIR / "ca.key"
CA_CERT_PATH = CA_DIR / "ca.crt"
 
RSA_KEY_SIZE = 2048
CERT_VALIDITY_DAYS = 3650
 
OPENSSL_BIN = r"C:\OpenSSL-Win64\openssl-3.5.7\apps\openssl.exe"

def _require_openssl() -> None:
    if shutil.which("openssl") is None:
        sys.exit("openssl not found on PATH")


# OpenSSL is being very annoying for some reason. 
# It misconfigured internal build scripts relying on a hard-coded default path to openssl.cnf
def _openssl_env() -> dict:
    env = os.environ.copy()
    env["OPENSSL_CONF"] = str(Path(OPENSSL_BIN).parent / "openssl.cnf")
    return env


def _run(cmd: list[str]) -> None:
    result = subprocess.run(cmd, capture_output=True, text=True, env=_openssl_env())
    if result.returncode != 0:
        sys.exit(f"Command failed: {' '.join(cmd)}\n{result.stderr.strip()}")
 
 
def _restrict_to_owner(path: Path) -> None:
    #Lock a file down to the current user only
    if os.name == "nt":
        username = os.environ.get("USERNAME", "")
        domain = os.environ.get("USERDOMAIN", "")
        grant_target = f"{domain}\\{username}" if domain and username else username
        try:
            # Strip inherited permissions and grant
            # full control to the current user only.
            subprocess.run(
                ["icacls", str(path), "/inheritance:r"],
                check=True, capture_output=True,
            )
            subprocess.run(
                ["icacls", str(path), "/grant:r", f"{grant_target}:F"],
                check=True, capture_output=True,
            )
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            print(f"WARNING: could not restrict ACL on {path}: {e}", file=sys.stderr)
    else:
        path.chmod(0o600)
 
 
def _generate_pkcs8_key(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    _run([
        OPENSSL_BIN, "genpkey",
        "-algorithm", "RSA",
        "-pkeyopt", f"rsa_keygen_bits:{RSA_KEY_SIZE}",
        "-out", str(path),
    ])
    _restrict_to_owner(path)
 
 
def _random_serial_hex() -> str:
    """20 random bytes (160 bits) with the top bit cleared, so the value is
    unambiguously positive once DER-encoded as an INTEGER — mirrors what
    cryptography's x509.random_serial_number() did in the previous version.
    Passed to openssl's -set_serial, which accepts a 0x-prefixed hex literal."""
    serial_bytes = bytearray(secrets.token_bytes(20))
    serial_bytes[0] &= 0x7F
    return "0x" + serial_bytes.hex()
 
 
def _write_ext_file(contents: str) -> str:
    with tempfile.NamedTemporaryFile("w", suffix=".cnf", delete=False) as f:
        f.write(contents)
        return f.name
 
 
def init_ca(force: bool = False) -> None:
    if CA_KEY_PATH.exists() and not force:
        print(f"CA already exists at {CA_KEY_PATH}. Use --force to regenerate.")
        return
 
    CA_DIR.mkdir(parents=True, exist_ok=True)
    _generate_pkcs8_key(CA_KEY_PATH)
 
    _run([
        OPENSSL_BIN, "req", "-x509", "-new",
        "-key", str(CA_KEY_PATH),
        "-sha256",
        "-days", str(CERT_VALIDITY_DAYS),
        "-subj", "/CN=Local Telemetry Dev CA",
        "-set_serial", _random_serial_hex(),
        "-addext", "basicConstraints=critical,CA:TRUE,pathlen:0",
        "-addext", "keyUsage=critical,keyCertSign,cRLSign",
        "-outform", "DER",
        "-out", str(CA_CERT_PATH),
    ])
 
    print(f"CA created:\n  {CA_KEY_PATH}\n  {CA_CERT_PATH}")
 
 
def add_device(name: str, app_uri: str, dns_names: list[str], ip_addrs: list[str]) -> None:
    if not CA_KEY_PATH.exists() or not CA_CERT_PATH.exists():
        sys.exit("No CA found. Run 'init-ca' first.")
 
    device_dir = DEVICES_DIR / name
    if device_dir.exists():
        sys.exit(f"Error: Device '{name}' already exists. Choose a different name or explicitly remove/delete the existing device")
 
    for ip in ip_addrs:
        ipaddress.ip_address(ip)  # fail fast on a bad --ip before shelling out to openssl
 
    device_dir.mkdir(parents=True, exist_ok=True)
    key_path = device_dir / f"{name}.key"
    cert_path = device_dir / f"{name}.crt"
    csr_path = device_dir / f"{name}.csr"
 
    _generate_pkcs8_key(key_path)
 
    _run([
        OPENSSL_BIN, "req", "-new",
        "-key", str(key_path),
        "-subj", f"/CN={name}",
        "-out", str(csr_path),
    ])
 
    # OPC UA requires the ApplicationUri to appear as a URI entry in the
    # certificate's SubjectAlternativeName, or the stack will reject the
    # cert during the secure channel handshake (BadCertificateUriInvalid).
    san_parts = [f"URI:{app_uri}"]
    san_parts += [f"DNS:{d}" for d in dns_names]
    san_parts += [f"IP:{ip}" for ip in ip_addrs]
    san_line = ",".join(san_parts)
 
    ext_path = _write_ext_file(
        "basicConstraints = critical, CA:FALSE\n"
        "keyUsage = critical, digitalSignature, keyEncipherment\n"
        f"subjectAltName = {san_line}\n"
    )
    try:
        _run([
            OPENSSL_BIN, "x509", "-req",
            "-in", str(csr_path),
            "-CA", str(CA_CERT_PATH), "-CAform", "DER",
            "-CAkey", str(CA_KEY_PATH),
            "-set_serial", _random_serial_hex(),
            "-days", str(CERT_VALIDITY_DAYS),
            "-sha256",
            "-extfile", ext_path,
            "-outform", "DER",
            "-out", str(cert_path),
        ])
    finally:
        os.unlink(ext_path)
        csr_path.unlink(missing_ok=True)  # only the key+cert are meant to persist per device
 
    print(f"Device '{name}' created:\n  {key_path}\n  {cert_path}\n")
    print(f"  ApplicationUri: {app_uri}\n")
    print(f"  SAN: {san_line}\n")
    print(f"\nDeploy alongside {CA_CERT_PATH} (as the trust list entry).")


def main() -> None:
    _require_openssl()
 
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
 
    p_init = sub.add_parser("init-ca", help="Create the local CA (run once)")
    p_init.add_argument("--force", action="store_true", help="Regenerate even if CA already exists")
 
    p_add = sub.add_parser("add-device", help="Generate a device key+cert signed by the CA")
    p_add.add_argument("name", help="Device name, e.g. 'server', 'qt-frontend', 'plc-sim'")
    p_add.add_argument("--uri", required=True, help="ApplicationUri, e.g. urn:myorg:telemetry:server")
    p_add.add_argument("--dns", nargs="*", default=[], help="DNS names for the SAN (e.g. localhost)")
    p_add.add_argument("--ip", nargs="*", default=[], help="IP addresses for the SAN (e.g. 127.0.0.1)")
 
    args = parser.parse_args()
 
    if args.command == "init-ca":
        init_ca(force=args.force)
    elif args.command == "add-device":
        add_device(args.name, args.uri, args.dns, args.ip)
 
 
if __name__ == "__main__":
    main()
 
