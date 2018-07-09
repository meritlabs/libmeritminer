/*
 * Copyright (C) 2018 The Merit Foundation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either vedit_refsion 3 of the License, or
 * (at your option) any later vedit_refsion.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * Botan library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than Botan. If you modify file(s) with
 * this exception, you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do
 * so, delete this exception statement from your version. If you delete
 * this exception statement from all source files in the program, then
 * also delete it here.
 */
#ifndef LIBMERITMINER_EXCEPTIONS_H
#define LIBMERITMINER_EXCEPTIONS_H

#include <exception>
#include <string>

class KernelGPUException : public std::exception {
public:
    explicit KernelGPUException() : msg_("An error occurred while working with GPU Kernel"){};
    explicit KernelGPUException(const std::string &msg) : msg_(msg){};

    const char *what() const noexcept {
        return msg_.c_str();
    }

private:
    std::string msg_;
};

class CudaMemoryAllocationException : public KernelGPUException {
public:
    explicit CudaMemoryAllocationException () : KernelGPUException("An error occurred while trying to allocate memory for CUDA device"){};
    explicit CudaMemoryAllocationException (const std::string &msg) : KernelGPUException(msg){};
};

class CudaSetDeviceException : public KernelGPUException {
public:
    explicit CudaSetDeviceException() : KernelGPUException("An error occurred while trying to set device to be used for GPU executions."){};
    explicit CudaSetDeviceException(const std::string &msg) : KernelGPUException(msg){};
};

#endif
