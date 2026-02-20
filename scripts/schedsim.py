# TODO: Rewrite to use Python API (pyschedsim)
import subprocess
import shutil
import os
from typing import Mapping, Optional, Union
import concurrent.futures

AllocatorArgValue = Union[str, int, float]

class SchedSimRunner:
    def __init__(self, executable_path: str = "./build/apps/alloc"):
        self.executable = executable_path

    def run_simulation(self,
                      input_file: Optional[str] = None,
                      platform_file: Optional[str] = None,
                      allocator: Optional[str] = None,
                      reclaim: Optional[str] = None,
                      output_file: Optional[str] = None,
                      target: Optional[int] = 1,
                      show_help: bool = False,
                      allocator_args: Optional[Mapping[str, AllocatorArgValue]] = None) -> tuple[int, str, str]:
        cmd = [self.executable]

        if show_help:
            cmd.append("--help")
        else:
            if input_file:
                cmd.extend(["--input", input_file])
            if platform_file:
                cmd.extend(["--platform", platform_file])
            if allocator:
                cmd.extend(["--alloc", allocator])
            if allocator_args:
                for key, value in allocator_args.items():
                    cmd.extend(["--alloc-arg", f"{key}={value}"])
            if reclaim:
                cmd.extend(["--reclaim", reclaim])
            if output_file:
                cmd.extend(["--output", output_file])
            if target is not None:
                cmd.extend(["--target", str(target)])

        try:
            process = subprocess.run(
                cmd,
                text=True,
                capture_output=True,
                check=True
            )
            return process.returncode, process.stdout, process.stderr

        except subprocess.CalledProcessError as e:
            print(f"CalledProcessError: {' '.join(e.cmd)} | {e.stdout}")
            return -1, "", "Error: CalledProcessError"
        except FileNotFoundError:
            print(f"Error: Executable '{self.executable}' not found")
            return -1, "", "Error: Executable not found"
        except Exception as e:
            print(f"Error: {str(e)}")
            return -1, "", f"Error: {str(e)}"

    def simul(
        self,
        sce_dir,
        alloc,
        reclaim,
        platform,
        target,
        logs,
        allocator_args: Optional[Mapping[str, AllocatorArgValue]] = None,
    ):
        if os.path.isdir(logs):
            shutil.rmtree(logs)

        os.mkdir(logs)

        for directory in sorted(os.listdir(sce_dir)):
            current_dir = os.path.join(sce_dir, directory)
            logs_dir = os.path.join(logs, directory)
            if not os.path.isdir(logs_dir):
                os.mkdir(logs_dir)

            if os.path.isdir(current_dir):
                scenarios = sorted(os.listdir(current_dir))
                with concurrent.futures.ThreadPoolExecutor() as executor:
                    futures = [
                        executor.submit(
                            self.run_simulation,
                            os.path.join(current_dir, scenario),
                            platform,
                            alloc,
                            reclaim,
                            os.path.join(logs_dir, scenario),
                            target,
                            allocator_args=allocator_args
                        )
                        for scenario in scenarios
                    ]
                    for future in concurrent.futures.as_completed(futures):
                        future.result()
