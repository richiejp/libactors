# libactors

Basic components of the Actor model and message passing. Uses userland
read-copy-update (liburcu) concurrent data structures and pthread.

## Build & Install

To build and install to your CMake user repository.

```sh
$ mkdir build && cd build
$ cmake -DCMAKE_EXPORT_PACKAGE_REGISTRY=ON ..
$ make
$ make test
$ cmake --install ./ --prefix ~/.local
```

You can then use it in another CMake project (e.g. ltp-executor) by
putting the following in your CMakeLists.txt

```cmake
...
find_package(libactors REQUIRED)
...
target_link_libraries(<my target> actors)
...
```

## Usage

Documentation is scarce at the moment, however this is presently used
by the experimental LTP executor and also there are some tests (in
`/tests`).
