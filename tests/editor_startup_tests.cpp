#include "editor_app.hpp"
#include "startup.hpp"
#include "engine/platform.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
int main(int argc,char** argv) {
    if(argc>1) {
        editor::configure_startup_diagnostics(false);
        if(std::string_view(argv[1])=="--assert-child") {
            assert(false && "startup assertion probe");return 99;
        }
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
    { std::ofstream previous(dir/"engine_editor.crash.log");previous<<"Previous crash record\n"; }
    const auto child=[&](const wchar_t* argument,DWORD expected) {
        std::wstring command=L"\""+std::wstring(module,count)+L"\" "+argument;
        STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
        assert(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,dir.c_str(),&si,&process));
        const DWORD waited=WaitForSingleObject(process.hProcess,10000);
        if(waited!=WAIT_OBJECT_0)TerminateProcess(process.hProcess,1);
        assert(waited==WAIT_OBJECT_0);DWORD code{};assert(GetExitCodeProcess(process.hProcess,&code));
        CloseHandle(process.hThread);CloseHandle(process.hProcess);assert(code==expected);
    };
    child(L"--crash-child",EXCEPTION_ACCESS_VIOLATION);
    child(L"--assert-child",3);
    std::ifstream crash(dir/"engine_editor.crash.log");const std::string text((std::istreambuf_iterator<char>(crash)),{});
    assert(text.find("Previous crash record")!=std::string::npos);
    assert(text.find("0xC0000005")!=std::string::npos);
    assert(text.find("startup assertion probe")!=std::string::npos);
    assert(text.find("SIGABRT")!=std::string::npos);crash.close();std::filesystem::remove_all(dir);
    std::cout<<"Hidden-launch visibility, fatal exception and assertion logging passed\n";
}
