#include "shell_location.h"

#include <objbase.h>
#include <oleauto.h>
#include <exdisp.h>
#include <shldisp.h>

namespace wintea {
namespace {

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ~ComPtr() { reset(); }

    T* get() const { return ptr_; }
    T** put() {
        reset();
        return &ptr_;
    }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

class ComApartment {
public:
    ComApartment() : hr_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
    ~ComApartment() {
        if (initialized()) CoUninitialize();
    }

    bool usable() const {
        return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE;
    }

private:
    bool initialized() const {
        return hr_ == S_OK || hr_ == S_FALSE;
    }

    HRESULT hr_;
};

HWND rootWindow(HWND hwnd) {
    HWND root = hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr;
    return root ? root : hwnd;
}

bool isDirectory(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring activeFolderPath(IWebBrowser2* browser) {
    ComPtr<IDispatch> document;
    if (FAILED(browser->get_Document(document.put())) || !document)
        return {};

    ComPtr<IShellFolderViewDual> view;
    if (FAILED(document->QueryInterface(IID_PPV_ARGS(view.put()))) || !view)
        return {};

    ComPtr<Folder> folder;
    if (FAILED(view->get_Folder(folder.put())) || !folder)
        return {};

    ComPtr<Folder2> folder2;
    if (FAILED(folder->QueryInterface(IID_PPV_ARGS(folder2.put()))) || !folder2)
        return {};

    ComPtr<FolderItem> self;
    if (FAILED(folder2->get_Self(self.put())) || !self)
        return {};

    BSTR rawPath = nullptr;
    if (FAILED(self->get_Path(&rawPath)) || !rawPath)
        return {};

    std::wstring path(rawPath, SysStringLen(rawPath));
    SysFreeString(rawPath);
    return isDirectory(path) ? path : std::wstring{};
}

} // namespace

std::wstring ExplorerDirectoryFromWindow(HWND foreground) {
    foreground = rootWindow(foreground);
    if (!foreground) return {};

    ComApartment com;
    if (!com.usable()) return {};

    ComPtr<IShellWindows> shellWindows;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                                IID_PPV_ARGS(shellWindows.put()))) ||
        !shellWindows) {
        return {};
    }

    long count = 0;
    if (FAILED(shellWindows->get_Count(&count))) return {};

    for (long i = 0; i < count; ++i) {
        VARIANT index;
        VariantInit(&index);
        index.vt = VT_I4;
        index.lVal = i;

        ComPtr<IDispatch> dispatch;
        HRESULT itemHr = shellWindows->Item(index, dispatch.put());
        VariantClear(&index);
        if (FAILED(itemHr) || !dispatch) continue;

        ComPtr<IWebBrowser2> browser;
        if (FAILED(dispatch->QueryInterface(IID_PPV_ARGS(browser.put()))) || !browser)
            continue;

        SHANDLE_PTR shellHwnd = 0;
        if (FAILED(browser->get_HWND(&shellHwnd))) continue;
        if (reinterpret_cast<HWND>(shellHwnd) != foreground) continue;

        return activeFolderPath(browser.get());
    }

    return {};
}

} // namespace wintea
