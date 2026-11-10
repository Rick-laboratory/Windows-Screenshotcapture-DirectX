#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include <sstream>
#include <stdexcept>

inline void check_hresult(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << operation << " failed (HRESULT 0x" << std::hex
                << static_cast<unsigned long>(result) << ").";
        throw std::runtime_error(message.str());
    }
}

template <typename Interface>
class ComPtr {
public:
    explicit ComPtr(Interface* pointer = nullptr) noexcept : pointer_(pointer) {}
    ~ComPtr() { if (pointer_ != nullptr) pointer_->Release(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    Interface* get() const noexcept { return pointer_; }
    Interface* operator->() const noexcept { return pointer_; }
    Interface** put() noexcept {
        if (pointer_ != nullptr) pointer_->Release();
        pointer_ = nullptr;
        return &pointer_;
    }

private:
    Interface* pointer_;
};

class ComApartment {
public:
    ComApartment() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        // An existing STA is also usable by WIC, but adds no initialization to balance.
        if (result != RPC_E_CHANGED_MODE) check_hresult(result, "CoInitializeEx");
        owned_ = SUCCEEDED(result);
    }
    ~ComApartment() { if (owned_) CoUninitialize(); }
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

private:
    bool owned_ = false;
};
