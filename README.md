# libmeritminer

Library that mines merit.

## Goals

- Easily usable by other programs.
- Support connecting to a meritd instance.
- Support connecting to stratum pool.
- Support multithreaded cpu mining.
- Support gpu mining.

## Usage

See the commandline tool [merit-minerd](src/minerd.cpp) for an example of using the library.

## Compiling

    mkdir build
    cd build
    cmake ..
    make
    make install

## Directories

| Directories                            | Description           |
|:---------------------------------------|:----------------------|
| [include](include)                     | Public library header.|
| [src](src)                             | Library Source code.  |


## License

Copyright (c) 2017-2018 The Merit Foundation
libmeritminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Foobar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
