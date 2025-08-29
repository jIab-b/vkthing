#!/usr/bin/env python3
import argparse
import os
import platform
import shutil
import subprocess
import sys
from typing import Optional


def which(prog: str) -> Optional[str]:
    return shutil.which(prog)


def run(cmd: list[str], cwd: Optional[str] = None):
    print("$", " ".join(cmd))
    subprocess.check_call(cmd, cwd=cwd)


def is_linux() -> bool:
    return sys.platform.startswith("linux")


def default_generator() -> str:
    # Prefer Ninja; otherwise Unix Makefiles
    if which("ninja"):
        return "Ninja"
    return "Unix Makefiles"


def is_single_config(gen: str) -> bool:
    return gen in {"Ninja", "Unix Makefiles", "NMake Makefiles"}


def read_cached_generator(build_dir: str) -> Optional[str]:
    cache = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache):
        return None
    try:
        with open(cache, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.startswith("CMAKE_GENERATOR:"):
                    parts = line.strip().split("=", 1)
                    if len(parts) == 2:
                        return parts[1]
    except Exception:
        return None
    return None


def cmake_configure(build_dir: str, generator: Optional[str], config: str, build_samples: bool):
    gen = generator or default_generator()
    cached = read_cached_generator(build_dir)
    if cached and cached != gen:
        raise RuntimeError(
            f"CMake build dir '{build_dir}' already configured with generator '{cached}'. "
            f"Requested '{gen}'. Use --clean or a different --build-dir."
        )
    args = ["cmake", "-S", ".", "-B", build_dir, "-G", gen, f"-DBUILD_SAMPLES={'ON' if build_samples else 'OFF'}"]
    if is_single_config(gen):
        args += [f"-DCMAKE_BUILD_TYPE={config}"]
    run(args)


def cmake_build(build_dir: str, config: str, parallel: Optional[int]):
    args = ["cmake", "--build", build_dir, "--config", config]
    if parallel and parallel > 1:
        args += ["--parallel", str(parallel)]
    run(args)


def check_env() -> bool:
    ok = True
    if not which("glslangValidator"):
        print("[WARN] glslangValidator not found on PATH. Install Vulkan SDK (or distro package providing it).")
        ok = False
    if which("pkg-config"):
        try:
            subprocess.check_call(["pkg-config", "--exists", "glfw3"])
        except subprocess.CalledProcessError:
            print("[WARN] pkg-config cannot find glfw3. Install it (e.g., sudo apt install libglfw3-dev).")
            ok = False
    else:
        print("[WARN] pkg-config not found; GLFW discovery may fail.")
        ok = False
    return ok


def find_executable(build_dir: str) -> Optional[str]:
    path = os.path.join(build_dir, "app", "sandbox", "sandbox")
    return path if os.path.isfile(path) else None


def main():
    if not is_linux():
        print("[ERROR] build_linux.py is intended to be run on Linux.")
        sys.exit(2)

    parser = argparse.ArgumentParser(description="Linux build script for vkthing")
    parser.add_argument("--build-dir", default="build_linux", help="Build directory (default: build_linux)")
    parser.add_argument("--config", default="Release", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], help="Build type")
    parser.add_argument("--generator", default=None, help="CMake generator (e.g., Ninja, Unix Makefiles)")
    parser.add_argument("--clean", action="store_true", help="Remove build dir before configure")
    parser.add_argument("--with-samples", action="store_true", help="Build optional sample projects (if present)")
    parser.add_argument("--run", action="store_true", help="Run sandbox after build")
    parser.add_argument("-j", "--jobs", type=int, default=None, help="Parallel build jobs")
    args = parser.parse_args()

    print(f"[INFO] Host: {platform.system()} {platform.release()} | Python {platform.python_version()}")
    env_ok = check_env()

    bdir = os.path.abspath(args.build_dir)
    if args.clean and os.path.isdir(bdir):
        print(f"[INFO] Removing build directory: {bdir}")
        shutil.rmtree(bdir)
    os.makedirs(bdir, exist_ok=True)

    try:
        cmake_configure(bdir, args.generator, args.config, args.with_samples)
        cmake_build(bdir, args.config, args.jobs)
    except RuntimeError as e:
        print("[ERROR]", e)
        sys.exit(3)
    except subprocess.CalledProcessError as e:
        print("[ERROR] Build failed:", e)
        sys.exit(e.returncode)

    exe = find_executable(bdir)
    if exe:
        print(f"[INFO] Built sandbox: {exe}")
    else:
        print("[WARN] Could not locate sandbox executable.")

    if args.run:
        if not exe:
            print("[ERROR] Cannot run; executable not found.")
            sys.exit(4)
        try:
            run([exe])
        except subprocess.CalledProcessError as e:
            print("[ERROR] Sandbox run failed:", e)
            sys.exit(e.returncode)

    if not env_ok:
        print("[NOTE] Some environment checks failed; if CMake succeeded you can ignore this.")


if __name__ == "__main__":
    main()
