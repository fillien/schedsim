# Schedsim

A suite of simulation tools for testing scheduling policies.

## Principle

![Simulator inputs/outputs](doc/input-output.png)


## Building

The project depends on tools and libraries that must be installed before the project can be built :
  - [CMake](https://cmake.org/)
  - [Doxygen](https://www.doxygen.nl/)
  - [Graphviz](https://graphviz.org/)

The rest of the dependencies are managed by CMake.

Build the project with CMake, like this:
```bash
  cmake -S . -B build -G Ninja
  cmake --build build -t simu
```

## Documentation

Build the documentation localy as follows:

```bash
  cmake -S . -B build -G Ninja
  cmake --build build -t doxygen
```

And open the generated index located at `/doc/html/index.html`


## Usage/Examples

To simulate a scenario, give a scenario file in YAML as an argument to the simulator:

```bash
  ./build/src/launch scenario/multi-ex4.yml
```
