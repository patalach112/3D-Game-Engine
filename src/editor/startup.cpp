#include "editor_app.hpp"
#include "startup.hpp"
#include "engine/log.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
namespace editor {
namespace {
const char* crash_file="engine_editor.crash.log";
void write_crash(const char* message) noexcept {
    // Do not acquire the engine logger mutex from a fatal exception handler.
    const auto file=CreateFileA(crash_file,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written{};DWORD length{};while(message[length]&&length<4096)++length;
    WriteFile(file,message,length,&written,nullptr);FlushFileBuffers(file);CloseHandle(file);
}
LONG WINAPI fatal_exception(EXCEPTION_POINTERS* info) {
    char text[256]{};
    std::snprintf(text,sizeof(text),"Fatal Windows exception 0x%08lX at %p, thread %lu\r\n",
        info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress,GetCurrentThreadId());
    write_crash(text);return EXCEPTION_EXECUTE_HANDLER;
}
void abort_signal(int) {write_crash("Fatal SIGABRT (assertion or abort)\r\n");std::_Exit(3);}
#ifdef _MSC_VER
int report_hook(int,char* message,int*) {write_crash(message);return FALSE;}
#endif
}
void configure_startup_diagnostics(bool designer) {
    crash_file=designer?"editor_hud.crash.log":"engine_editor.crash.log";
    // GUI applications normally have no stderr. Preserve assertion/CRT messages.
    FILE* sink{};
#ifdef _MSC_VER
    (void)freopen_s(&sink,crash_file,"a",stderr);
    _CrtSetReportHook2(_CRT_RPTH_INSTALL,report_hook);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#else
    sink=std::freopen(crash_file,"a",stderr);
#endif
    (void)sink;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal_exception);
    std::signal(SIGABRT,abort_signal);
    std::set_terminate([] {
        write_crash("Unhandled C++ termination\r\n");
        if(auto error=std::current_exception())try{std::rethrow_exception(error);}catch(const std::exception& e){write_crash(e.what());write_crash("\r\n");}catch(...){}
        std::abort();
    });
}
void show_editor_window(HWND hwnd,int requested_show) {
    // A launcher/IDE can pass SW_HIDE to a GUI entry point. This is an editor,
    // not a background utility: never enter the render loop with a hidden HWND.
    ShowWindow(hwnd,requested_show==SW_HIDE?SW_SHOWNORMAL:requested_show);
    if(!IsWindowVisible(hwnd))ShowWindow(hwnd,SW_SHOW); // STARTF_USESHOWWINDOW can override the first call.
    UpdateWindow(hwnd);
    RECT client{};GetClientRect(hwnd,&client);
    LOG_INFO("startup","Window shown: visible=%d minimized=%d client=%ldx%ld show=%d",
        IsWindowVisible(hwnd)!=FALSE,IsIconic(hwnd)!=FALSE,client.right,client.bottom,requested_show);
}
}
