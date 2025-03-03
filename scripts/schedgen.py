import subprocess
import os
from typing import Optional, Union

class SchedGenError(Exception):
    """Custom exception for errors when running schedgen."""
    pass

class SchedGen:
    """A Python wrapper for the schedgen binary executable."""

    def __init__(self, binary_path: str = "schedgen"):
        """
        Initialize the SchedGen wrapper.

        Args:
            binary_path (str): Path to the schedgen binary. Defaults to 'schedgen'
                              (assumes it's in PATH).
        """
        self.binary_path = binary_path
        if not os.path.isfile(self.binary_path) or not os.access(self.binary_path, os.X_OK):
            raise SchedGenError(f"Binary '{self.binary_path}' not found or not executable.")

    def _run_command(self, args: list) -> str:
        """
        Execute the schedgen command with the given arguments.

        Args:
            args (list): List of command-line arguments.

        Returns:
            str: Output from the command.

        Raises:
            SchedGenError: If the command fails.
        """
        try:
            result = subprocess.run(
                [self.binary_path, "taskset"] + args,
                capture_output=True,
                text=True,
                check=True
            )
            return result.stdout
        except subprocess.CalledProcessError as e:
            raise SchedGenError(f"Command failed with error: {e.stderr}")
        except Exception as e:
            raise SchedGenError(f"Unexpected error: {str(e)}")

    def generate_taskset(
        self,
        tasks: Optional[int] = None,
        totalu: Optional[float] = None,
        umax: Optional[float] = None,
        success: Optional[float] = None,
        compression: Optional[float] = None,
        output: Optional[str] = None
    ) -> str:
        """
        Generate a task set using the schedgen tool.

        Args:
            tasks (int, optional): Number of tasks to generate.
            totalu (float, optional): Total utilization of the task set.
            umax (float, optional): Maximum utilization per task (0 to 1).
            success (float, optional): Success rate of deadlines met (0 to 1).
            compression (float, optional): Compression ratio for tasks (0 to 1).
            output (str, optional): Output file path.

        Returns:
            str: Command output (if any).

        Raises:
            SchedGenError: If arguments are invalid or the command fails.
            ValueError: If argument ranges are invalid.
        """
        args = []

        if tasks is not None:
            if not isinstance(tasks, int) or tasks <= 0:
                raise ValueError("Tasks must be a positive integer.")
            args.extend(["-t", str(tasks)])

        if totalu is not None:
            if not isinstance(totalu, (int, float)) or totalu < 0:
                raise ValueError("Total utilization must be non-negative.")
            args.extend(["-u", str(totalu)])

        if umax is not None:
            if not isinstance(umax, (int, float)) or not 0 <= umax <= 1:
                raise ValueError("Maximum utilization must be between 0 and 1.")
            args.extend(["-m", str(umax)])

        if success is not None:
            if not isinstance(success, (int, float)) or not 0 <= success <= 1:
                raise ValueError("Success rate must be between 0 and 1.")
            args.extend(["-s", str(success)])

        if compression is not None:
            if not isinstance(compression, (int, float)) or not 0 <= compression <= 1:
                raise ValueError("Compression ratio must be between 0 and 1.")
            args.extend(["-c", str(compression)])

        if output is not None:
            if not isinstance(output, str):
                raise ValueError("Output must be a string (file path).")
            args.extend(["-o", output])

        return self._run_command(args)


if __name__ == "__main__":
    try:
        schedgen = SchedGen(binary_path="./schedgen")

        print("Generating task set:")
        output = schedgen.generate_taskset(
            tasks=5,
            totalu=0.75,
            umax=0.5,
            success=0.9,
            compression=0.2,
            output="taskset.csv"
        )
        print(output)
        print("Task set saved to taskset.csv")

    except SchedGenError as e:
        print(f"Error: {e}")
    except ValueError as e:
        print(f"Invalid input: {e}")
