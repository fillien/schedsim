import subprocess
import sys
import shutil
import os
from typing import Optional
import concurrent.futures

class SchedSimRunner:
    def __init__(self, executable_path: str = "schedsim"):
        self.executable = executable_path

    def run_simulation(self,
                      input_file: Optional[str] = None,
                      platform_file: Optional[str] = None,
                      allocator: Optional[str] = None,
                      scheduler: Optional[str] = None,
                      output_file: Optional[str] = None,
                      delay: bool = False,
                      target: Optional[int] = 1,
                      show_help: bool = False,
                      show_version: bool = False) -> tuple[int, str, str]:
        cmd = [self.executable]

        if show_help:
            cmd.append("--help")
        elif show_version:
            cmd.append("--version")
        else:
            if input_file:
                cmd.extend(["--input", input_file])
            if platform_file:
                cmd.extend(["--platform", platform_file])
            if allocator:
                cmd.extend(["--alloc", allocator])
            if scheduler:
                cmd.extend(["--sched", scheduler])
            if output_file:
                cmd.extend(["--output", output_file])
            if target is not None:
                cmd.extend(["--target", str(target)])
            if delay:
                cmd.append("--delay")

        try:
            process = subprocess.run(
                cmd,
                text=True,
                check=True
            )
            return process.returncode, process.stdout, process.stderr

        except subprocess.CalledProcessError as e:
            return -1, "", "Error: CalledProcessError"
        except FileNotFoundError:
            print(f"Error: Executable '{self.executable}' not found")
            return -1, "", "Error: Executable not found"
        except Exception as e:
            print(f"Error: {str(e)}")
            return -1, "", f"Error: {str(e)}"

    def simul(self, sce_dir, alloc, sched, platform, target, logs):
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
                            sched,
                            os.path.join(logs_dir, scenario),
                            False,
                            target
                        )
                        for scenario in scenarios
                    ]
                    for future in concurrent.futures.as_completed(futures):
                        future.result()
