# PlatformIO pre-script: FW_VERSION from git (SPEC 7.7), so a release's version is its tag.
# "v1.2.0" -> "1.2.0"; between tags "1.2.0-3-gabc1234"; "-dirty" with uncommitted changes.
# No tags yet: the short commit hash. No git: "unknown".
import subprocess

Import("env")  # noqa: F821 (PlatformIO provides it)


def describe():
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env["PROJECT_DIR"],  # noqa: F821
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip()
    except Exception:
        return "unknown"


version = describe()
if version.startswith("v"):
    version = version[1:]
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])  # noqa: F821
print("FW_VERSION", version)
