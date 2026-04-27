"""
deps.json schema and content validation tests.
Run from repo root: python -m pytest tests/test_deps_json.py -v
"""
import json
import os
import pytest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPS_PATH = os.path.join(REPO_ROOT, "deps.json")

REQUIRED_FIELDS_ZIP = {"id", "version", "url", "sha256", "install_type", "unzip_to"}
REQUIRED_FIELDS_SINGLE_FILE = {"id", "version", "url", "sha256", "install_type", "install_to"}
VALID_INSTALL_TYPES = {"zip", "single_file"}

PROHIBITED_IDS = {"webix-5.2.1", "webix-2.4.7", "webix-3.1.2"}
REQUIRED_IDS = {"alpinejs", "tailwindcss", "daisyui"}

EXPECTED_SINGLE_FILE = {
    "alpinejs":   "External/Alpine/alpine.min.js",
    "tailwindcss": "External/Tailwind/tailwind.min.js",
    "daisyui":    "External/DaisyUI/daisyui.min.css",
}


@pytest.fixture(scope="module")
def deps():
    with open(DEPS_PATH, encoding="utf-8") as f:
        return json.load(f)


@pytest.fixture(scope="module")
def deps_by_id(deps):
    return {d["id"]: d for d in deps["deps"]}


def test_schema_version_present(deps):
    assert deps.get("schema") == "diaenv.deps.v1"


def test_deps_is_list(deps):
    assert isinstance(deps["deps"], list)
    assert len(deps["deps"]) > 0


def test_no_webix_entries(deps_by_id):
    for prohibited in PROHIBITED_IDS:
        assert prohibited not in deps_by_id, f"Webix entry '{prohibited}' must be removed from deps.json"


def test_required_js_deps_present(deps_by_id):
    for required in REQUIRED_IDS:
        assert required in deps_by_id, f"Required dep '{required}' not found in deps.json"


@pytest.mark.parametrize("dep_id,expected_path", EXPECTED_SINGLE_FILE.items())
def test_single_file_install_to_paths(deps_by_id, dep_id, expected_path):
    dep = deps_by_id[dep_id]
    assert dep["install_type"] == "single_file", f"{dep_id} must use install_type 'single_file'"
    assert dep["install_to"] == expected_path, (
        f"{dep_id} install_to must be '{expected_path}', got '{dep.get('install_to')}'"
    )


@pytest.mark.parametrize("dep_id", REQUIRED_IDS)
def test_single_file_has_url(deps_by_id, dep_id):
    dep = deps_by_id[dep_id]
    assert dep.get("url"), f"{dep_id} must have a non-empty url"


def test_all_entries_have_required_fields(deps):
    for dep in deps["deps"]:
        install_type = dep.get("install_type")
        assert install_type in VALID_INSTALL_TYPES, (
            f"dep '{dep.get('id')}' has unknown install_type '{install_type}'"
        )
        if install_type == "zip":
            for field in REQUIRED_FIELDS_ZIP:
                assert field in dep, f"dep '{dep.get('id')}' missing required field '{field}'"
        elif install_type == "single_file":
            for field in REQUIRED_FIELDS_SINGLE_FILE:
                assert field in dep, f"dep '{dep.get('id')}' missing required field '{field}'"


def test_no_duplicate_ids(deps):
    ids = [d["id"] for d in deps["deps"]]
    assert len(ids) == len(set(ids)), f"Duplicate dep IDs found: {[i for i in ids if ids.count(i) > 1]}"


def test_all_entries_have_version(deps):
    for dep in deps["deps"]:
        assert dep.get("version"), f"dep '{dep.get('id')}' missing version"
