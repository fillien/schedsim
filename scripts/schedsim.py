import subprocess
import sys
import shutil
import os
from typing import Optional
import concurrent.futures

def print_progress_bar(iteration, total, length=50):
    percent = (iteration / float(total)) * 100
    filled = int(length * iteration // total)
    bar = '█' * filled + '-' * (length - filled)
    sys.stdout.write(f'\rProgress: |{bar}| {percent:.1f}% Complete')
    sys.stdout.flush()

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
            if delay:
                cmd.append("--delay")

        try:
            process = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False
            )
            return process.returncode, process.stdout, process.stderr

        except FileNotFoundError:
            return -1, "", f"Error: Executable '{self.executable}' not found"
        except Exception as e:
            return -1, "", f"Error: {str(e)}"

    def simul(self, sce_dir, alloc, sched, platform, logs):
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
                            os.path.join(logs_dir, scenario)
                        )
                        for scenario in scenarios
                    ]
                    for future in concurrent.futures.as_completed(futures):
                        future.result()
