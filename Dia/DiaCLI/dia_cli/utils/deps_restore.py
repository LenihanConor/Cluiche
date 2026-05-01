"""Core logic for deps.json manifest loading, download, verify, unpack."""
import hashlib
import json
import shutil
import time
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional
from urllib.parse import urlparse

_SENTINEL_DIR = ".diaenv/deps"
_PLACEHOLDER_SHA = "PLACEHOLDER_COMPUTE_AT_RUNTIME"


class DepsManifestError(Exception):
    pass


def load_deps(repo_root: Path) -> list:
    path = repo_root / "deps.json"
    if not path.exists():
        raise DepsManifestError(f"deps.json not found at {path}\nRun: dia env deps to initialise.")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        raise DepsManifestError(f"deps.json is malformed: {e}") from e
    if data.get("schema") != "diaenv.deps.v1":
        raise DepsManifestError(f"deps.json schema mismatch: expected 'diaenv.deps.v1', got '{data.get('schema')}'")
    return data.get("deps", [])


def get_sentinel_path(repo_root: Path, dep_id: str) -> Path:
    return repo_root / _SENTINEL_DIR / f"{dep_id}.restored"


def is_restored(repo_root: Path, dep: dict) -> bool:
    sentinel = get_sentinel_path(repo_root, dep["id"])
    if not sentinel.exists():
        return False
    try:
        data = json.loads(sentinel.read_text(encoding="utf-8"))
        return data.get("version") == dep["version"]
    except Exception:
        return False


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _download_http(url: str, dest: Path) -> bool:
    """Download via requests with 3 retries. Returns True on success."""
    import requests
    partial = dest.with_suffix(dest.suffix + ".partial")
    for attempt in range(3):
        try:
            resp = requests.get(url, stream=True, timeout=30)
            if resp.status_code == 404:
                return False
            resp.raise_for_status()
            with open(partial, "wb") as f:
                for chunk in resp.iter_content(chunk_size=65536):
                    f.write(chunk)
            partial.rename(dest)
            return True
        except (requests.RequestException, OSError) as e:
            if partial.exists():
                partial.unlink()
            if attempt < 2:
                time.sleep(2 ** attempt)
            else:
                print(f"  HTTP error after 3 attempts: {e}")
    return False


def _download_file(url: str, dest: Path) -> bool:
    """Copy a file:// URI."""
    parsed = urlparse(url)
    src = Path(parsed.path.lstrip("/"))
    if not src.exists():
        # Try without stripping leading slash (Windows absolute path)
        src = Path(url.replace("file:///", "").replace("file://", ""))
    if not src.exists():
        print(f"  file:// mirror not found: {src}")
        return False
    shutil.copy2(src, dest)
    return True


def _download_from_sources(urls: list, tmp_path: Path) -> bool:
    """Try each URL in order. Returns True if any succeeds."""
    for url in urls:
        if not url:
            continue
        print(f"  trying: {url}")
        if url.startswith("file://"):
            if _download_file(url, tmp_path):
                return True
        else:
            if _download_http(url, tmp_path):
                return True
    return False


def _install_zip(archive: Path, unzip_to: Path, strip_root: bool) -> None:
    unzip_to.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "r") as zf:
        members = zf.namelist()
        if strip_root and members:
            root_prefix = members[0].split("/")[0] + "/"
            strip = all(m.startswith(root_prefix) for m in members if m != root_prefix)
        else:
            strip = False

        for member in members:
            if strip:
                rel = member[len(root_prefix):]
                if not rel:
                    continue
            else:
                rel = member
            dest = unzip_to / rel
            if not dest.resolve().is_relative_to(unzip_to.resolve()):
                raise DepsManifestError(
                    f"Zip entry '{member}' resolves outside target directory (zip slip blocked)"
                )
            if member.endswith("/"):
                dest.mkdir(parents=True, exist_ok=True)
            else:
                dest.parent.mkdir(parents=True, exist_ok=True)
                with zf.open(member) as src, open(dest, "wb") as dst:
                    shutil.copyfileobj(src, dst)


def _install_single_file(downloaded: Path, install_to: Path, repo_root: Path) -> None:
    if not install_to.resolve().is_relative_to(repo_root.resolve()):
        raise DepsManifestError(
            f"install_to '{install_to}' resolves outside repo root (path traversal blocked)"
        )
    install_to.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(downloaded, install_to)


def _install_exe(archive: Path, dep: dict, repo_root: Path) -> int:
    import subprocess
    exe_args = dep.get("exe_args", ["/quiet", "/norestart"])
    cmd = [str(archive)] + exe_args
    result = subprocess.run(cmd)
    return result.returncode


def restore_dep(dep: dict, repo_root: Path, force: bool = False, quiet: bool = False) -> int:
    dep_id = dep["id"]

    if is_restored(repo_root, dep) and not force:
        if not quiet:
            print(f"  {dep_id} already restored, skipping")
        return 0

    # Build URL list: primary + mirrors
    urls = []
    if dep.get("url"):
        urls.append(dep["url"])
    urls.extend(dep.get("mirrors", []))

    if not urls:
        print(f"ERROR: {dep_id} has no url or mirrors — cannot restore")
        return 1

    # Clean up any stale .tmp file
    tmp_path = repo_root / f".diaenv/deps/{dep_id}.tmp"
    tmp_path.parent.mkdir(parents=True, exist_ok=True)
    if tmp_path.exists():
        tmp_path.unlink()

    if not quiet:
        print(f"  {dep_id} downloading...")

    if not _download_from_sources(urls, tmp_path):
        print(f"ERROR: {dep_id} download failed. Tried:\n" + "\n".join(f"  {u}" for u in urls))
        if tmp_path.exists():
            tmp_path.unlink()
        return 1

    # SHA-256 verify
    expected = dep.get("sha256", "")
    if expected == _PLACEHOLDER_SHA:
        if not quiet:
            print(f"  WARNING: {dep_id} sha256 is placeholder — download not integrity-verified")
    elif expected:
        actual = _sha256_file(tmp_path)
        if actual != expected:
            print(f"ERROR: {dep_id} SHA-256 mismatch\n  expected: {expected}\n  actual:   {actual}")
            tmp_path.unlink()
            return 1

    # Install
    install_type = dep.get("install_type", "zip")
    unzip_to = repo_root / dep["unzip_to"] if "unzip_to" in dep else None

    if install_type == "zip" and unzip_to:
        _install_zip(tmp_path, unzip_to, dep.get("strip_root", False))
    elif install_type == "single_file":
        if "install_to" not in dep:
            print(f"ERROR: {dep_id} install_type is 'single_file' but 'install_to' is missing")
            tmp_path.unlink()
            return 1
        _install_single_file(tmp_path, repo_root / dep["install_to"], repo_root)
    elif install_type == "exe":
        code = _install_exe(tmp_path, dep, repo_root)
        if code != 0:
            tmp_path.unlink()
            return 1

    tmp_path.unlink()

    # Write sentinel
    sentinel = get_sentinel_path(repo_root, dep_id)
    sentinel.parent.mkdir(parents=True, exist_ok=True)
    sentinel.write_text(json.dumps({
        "id": dep_id,
        "version": dep["version"],
        "sha256": dep.get("sha256", ""),
        "restored_at": datetime.now(timezone.utc).isoformat(),
    }), encoding="utf-8")

    if not quiet:
        print(f"  {dep_id} restored")
    return 0


def restore_all(repo_root: Path, force: bool = False, quiet: bool = False) -> int:
    try:
        deps = load_deps(repo_root)
    except DepsManifestError as e:
        print(f"ERROR: {e}")
        return 3

    failed = 0
    for dep in deps:
        code = restore_dep(dep, repo_root, force=force, quiet=quiet)
        if code != 0:
            failed += 1
    return 0 if failed == 0 else 1
