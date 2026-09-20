#pragma once
#include "identity.hpp"
#include <atomic>
#include <filesystem>

namespace stroke::win {
extern HINSTANCE module;
extern std::atomic<long> live_objects;
std::filesystem::path module_path();
HRESULT create_service(REFIID iid, void** result) noexcept;
HRESULT register_server() noexcept;
HRESULT unregister_server() noexcept;
}
