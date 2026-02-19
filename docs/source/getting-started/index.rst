Getting Started
===============

.. toctree::
   :maxdepth: 2

   concepts

Prerequisites
-------------

- CMake 3.20+
- Clang with C++23 support
- Or Nix for reproducible builds

Installation
------------

Using Nix:

.. code-block:: bash

   nix build

Using CMake:

.. code-block:: bash

   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
