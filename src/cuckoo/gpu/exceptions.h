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
