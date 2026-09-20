#include <windows.h>
#include <msctf.h>
#include <cstdio>
#include <type_traits>

static_assert(std::is_base_of_v<IUnknown, ITfTextInputProcessorEx>);
static_assert(std::is_base_of_v<IUnknown, ITfCandidateListUIElement>);

int main() {
    // Force TSF/COM import and GUID linkage without creating or registering a text service.
    decltype(&CoCreateInstance) volatile create_com_object = &CoCreateInstance;
    const GUID thread_manager_id = CLSID_TF_ThreadMgr;
    const auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!create_com_object || thread_manager_id.Data1 == 0 || !resource) {
        return 1;
    }
    std::printf("MSVC %ld; architecture=%u-bit; TSF headers/imports/GUIDs/resource OK\n",
                static_cast<long>(_MSC_FULL_VER), static_cast<unsigned>(sizeof(void*) * 8));
    return 0;
}
