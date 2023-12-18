# TaskSet Generator

The TaskSet Generator is a tool for generating random task sets with a Weibull distribution.

## Usage

To use the TaskSet Generator, run the executable with the desired options. Here is the basic usage:

```bash
./build/generator/generator [options]
```

## Options

The following command-line options are available:

 - `-h, --help`: Display the help message.
 - `-c, --cores N`: Set the number of processor cores to N.
 - `-t, --tasks N`: Set the number of tasks in the generated set to N.
 - `-j, --jobs N`: Set the number of jobs per task in the generated set to N.
 - `-u, --totalu U`: Set the total utilization of the generated task set to U.
 - `-o, --output FILE`: Set the output file for the generated task set (must be a .json file).

## Example

To generate a random task set with specific characteristics, use the above options. For example:

```bash
./build/generator/generator -c 4 -t 5 -j 3 -u 0.8 -o output.json
```

This command generates a task set with 5 tasks, each having 3 jobs, on a 4-core processor, and with a total utilization of 0.8. The generated task set is saved to the file "output.json"

## Building

To build the TaskSet Generator, ensure you have the necessary dependencies installed and run the following commands:

```bash
cmake -S . -B build
cmake --build build -t generator
```
