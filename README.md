# libactors

Basic components of the Actor model and message passing. Uses userland
read-copy-update (liburcu) concurrent data structures and pthread.

## Build & Install

To build and install to your CMake user repository.

```sh
$ mkdir build && cd build
$ cmake -DCMAKE_EXPORT_PACKAGE_REGISTRY=ON ..
$ make
```

## Usage

This is presently used by the experimental LTP executor and also there are
some tests (in `/tests`).
