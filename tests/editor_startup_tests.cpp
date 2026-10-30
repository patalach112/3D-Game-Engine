#include "editor_app.hpp"
#include "startup.hpp"
#include "engine/platform.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char**) {
    if(argc>1) {
        editor::configure_startup_diagnostics(false);
        RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
        return 99;
    }
    engine::WindowConfig config;config.title="Startup regression";config.visible=false;config.borderless=true;
    config.native_callback=[](const engine::NativeMessage& m,std::intptr_t& result,void*) {
        result=editor::window_proc(static_cast<HWND>(m.window),m.id,static_cast<WPARAM>(m.wparam),static_cast<LPARAM>(m.lparam));return true;
    };
    auto window=engine::createnewwindow(config);const auto hwnd=reinterpret_cast<HWND>(window->native_handle().window);
    assert(!IsWindowVisible(hwnd));editor::show_editor_window(hwnd,SW_HIDE);assert(IsWindowVisible(hwnd));window.reset();
    wchar_t module[32768]{};const auto count=GetModuleFileNameW(nullptr,module,32768);assert(count && count<32768);
    const auto dir=std::filesystem::temp_directory_path()/("engine_startup_"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(dir);
    std::wstring command=L"\""+std::wstring(module,count)+L"\" --crash-child";
    STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
    assert(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,dir.c_str(),&si,&process));
    const DWORD waited=WaitForSingleObject(process.hProcess,10000);
    if(waited!=WAIT_OBJECT_0)TerminateProcess(process.hProcess,1);
    assert(waited==WAIT_OBJECT_0);DWORD code{};assert(GetExitCodeProcess(process.hProcess,&code));
    CloseHandle(process.hThread);CloseHandle(process.hProcess);assert(code==EXCEPTION_ACCESS_VIOLATION);
    std::ifstream crash(dir/"engine_editor.crash.log");const std::string text((std::istreambuf_iterator<char>(crash)),{});
    assert(text.find("0xC0000005")!=std::string::npos);crash.close();std::filesystem::remove_all(dir);
    std::cout<<"Hidden-launch visibility and fatal exception logging passed\n";
}
