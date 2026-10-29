#include "MainFrame.h"
#include "log.h"
#include "AboutDlg.h"
#include "archive.h"
#include "file_paths.h"
#include <process.h>
#include <memory>
#include "keyboard_input.h"
#include "settings_dialog.h"
#include "ui_thread_wait.h"
#include "cd_prepare_dialog.h"
#include "machine_commands.h"
#include "host_runtime.h"
#include "host_settings.h"
#include <shellapi.h>
#include <cwctype>
#include <stdexcept>

void CMainFrame::ShowFailure(const char* detail, const char* title) {
    {
        rxpce_config::DialogInputScope input;
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        MessageBox(detail, title, MB_OK | MB_ICONERROR);
    }
    RefreshCursor();
}
LRESULT CMainFrame::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
    hwnd = m_hWnd;
    m_menu = GetMenu();
    try {
        auto* loop = _Module.GetMessageLoop();
        if (!loop || !loop->AddMessageFilter(this) || !loop->AddIdleHandler(this))
            throw std::runtime_error("Cannot register application message handlers");
        int count = 0;
        const auto free_args = [](wchar_t** args) { LocalFree(args); };
        std::unique_ptr<wchar_t*, decltype(free_args)> args(
            CommandLineToArgvW(GetCommandLineW(), &count), free_args);
        if (!args)
            throw std::runtime_error("Cannot parse command line");
        if (count == 2)
            m_commandPath = args.get()[1];
        if (count > 2)
            throw std::runtime_error("Specify one ROM / ZIP / CUE / CHD path");
        m_profileLoaded = LoadProfile();
        if (!m_profileLoaded) {
            PostMessage(WM_CLOSE);
            return 0;
        }
        PrepareVideoWindow();
        PostMessage(WM_VIDEO_INITIALIZE);
    } catch (const std::exception& error) {
        ShowFailure(error.what(), "rxpce");
        PostMessage(WM_CLOSE);
    } catch (...) {
        ShowFailure("Unexpected initialization failure", "rxpce");
        PostMessage(WM_CLOSE);
    }
    return 0;
}
BOOL CMainFrame::OnIdle() {
    const bool ready = MachineCommands().ready();
    for (const UINT id : {IDC_RESET, IDC_FULLSCREEN, IDC_STOP})
        ::EnableMenuItem(m_menu, id, MF_BYCOMMAND | (ready ? MF_ENABLED : MF_GRAYED));
    for (const UINT id : {IDC_SAVE, IDC_LOAD})
        ::EnableMenuItem(m_menu, id, MF_BYCOMMAND | (ready ? MF_ENABLED : MF_GRAYED));
    ::EnableMenuItem(m_menu, IDC_ZOOM1,
                     MF_BYCOMMAND | (bFullScreen.load() ? MF_GRAYED : MF_ENABLED));
    ::EnableMenuItem(m_menu, IDC_SETTINGS, MF_BYCOMMAND | MF_ENABLED);
    return FALSE;
}
LRESULT CMainFrame::OnInitMenu(UINT, WPARAM, LPARAM, BOOL&) {
    OnIdle();
    return 0;
}
LRESULT CMainFrame::OnVideoInitialize(UINT, WPARAM, LPARAM, BOOL&) {
    try {

        if (!InitializeVideo()) {
            PostMessage(WM_CLOSE);
            return 0;
        }
        if (!m_commandPath.empty())
            LoadMediaPath(m_commandPath);
        return 0;
    } catch (const std::exception& error) {
        ShowFailure(error.what(), "rxpce operation failed");
        PostMessage(WM_CLOSE);
    } catch (...) {
        ShowFailure("Unexpected application failure", "rxpce operation failed");
        PostMessage(WM_CLOSE);
    }
    return 0;
}
unsigned __stdcall CMainFrame::InvokeMainLoop(void* parameter) {
    std::unique_ptr<WorkerStart> start(static_cast<WorkerStart*>(parameter));
    int result = -1;
    try {
        result = rxpce_host::RunEmulation(rxpce_paths::executable().parent_path() / L"backup.dat");
    } catch (const std::exception& error) {
        LogDump("Emulation start failed: %s\n", error.what());
    }
    start->frame->PostMessage(WM_APP + 8, start->generation);
    return result;
}
BOOL CMainFrame::StartMainLoop() {
    ResetMachineStop();
    if (!MachineCommands().initialize()) {
        PostMessage(WM_CLOSE);
        return FALSE;
    }
    std::unique_ptr<WorkerStart> start;
    try {
        start = std::make_unique<WorkerStart>(WorkerStart{this, ++m_generation});
    } catch (const std::bad_alloc&) {
        MachineCommands().reset();
        PostMessage(WM_CLOSE);
        return FALSE;
    }
    m_thread.reset(reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 0, InvokeMainLoop, start.get(), 0, nullptr)));
    if (!m_thread) {
        MachineCommands().reset();
        PostMessage(WM_CLOSE);
        return FALSE;
    }
    start.release();
    return TRUE;
}
void CMainFrame::StopMainLoop() {
    ++m_generation; // Invalidate even a completion notification already in the UI queue.
    if (m_thread) {
        m_stopping = true;
        LogDumpDetail("Stopping emulation worker\n");
        RequestMachineStop();
        rxpce_ui::wait_for_thread(m_thread.get());
        LogDumpDetail("Emulation worker stopped\n");
        m_thread.reset();
        m_stopping = false;
    }
    MachineCommands().reset();
}
LRESULT CMainFrame::OnClose(UINT, WPARAM, LPARAM, BOOL& handled) {
    if (m_preparing) {
        m_cancelPrepare = true;
        m_closeAfterPrepare = true;
    } else if (m_stopping)
        PostMessage(WM_CLOSE);
    else
        handled = FALSE;
    return 0;
}
LRESULT CMainFrame::OnMainLoopExited(UINT, WPARAM generation, LPARAM, BOOL&) {
    if (generation == m_generation)
        StopEmulation();
    return 0;
}
LRESULT CMainFrame::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    StopMainLoop();
    ShutdownVideo();
    if (m_profileLoaded) {
        try {
            SaveProfile();
        } catch (...) {
            LogDump("Cannot save application settings during shutdown\n");
        }
    }
    auto* loop = _Module.GetMessageLoop();
    if (loop) {
        loop->RemoveMessageFilter(this);
        loop->RemoveIdleHandler(this);
    }
    ::SetMenu(m_hWnd, nullptr);
    ::DestroyMenu(m_menu);
    m_menu = nullptr;
    PostQuitMessage(0);
    return 0;
}
LRESULT CMainFrame::OnActivateApp(UINT, WPARAM active, LPARAM, BOOL& handled) {
    bActiveApp = active != 0;
    if (!active) {
        rxpce_input::keyboard_state().reset();
        m_videoShortcuts.reset();
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
    } else
        RefreshCursor();
    handled = FALSE;
    return 0;
}
void CMainFrame::OnExit() {
    PostMessage(WM_CLOSE);
}
void CMainFrame::OnFullScreen() {
    if (MachineCommands().ready()) {
        ChangeWindowMode();
        RefreshCursor();
    }
}
void CMainFrame::OnZoom1() {
    ZoomWindow();
}
void CMainFrame::OnOpen() {
    std::vector<wchar_t> filename(32768);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = m_hWnd;
    dialog.lpstrFilter =
        L"ROM / ZIP / CD image (*.pce;*.zip;*.cue;*.chd)\0*.pce;*.zip;*.cue;*.chd\0CD image (*.cue;*.chd)\0*.cue;*.chd\0HuCard ROM / ZIP (*.pce;*.zip)\0*.pce;*.zip\0All files (*.*)\0*.*\0";
    dialog.lpstrTitle = L"Open ROM / CD image";
    dialog.lpstrInitialDir = rxpce_host::runtime.rom_directory.empty()
                                 ? nullptr
                                 : rxpce_host::runtime.rom_directory.c_str();
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    bool selected = false;
    {
        rxpce_config::DialogInputScope input;
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        selected = GetOpenFileNameW(&dialog) != FALSE;
    }
    RefreshCursor();
    if (selected)
        LoadMediaPath(filename.data());
}
BOOL CMainFrame::LoadMediaPath(const std::wstring& path) {
    if (m_preparing)
        return FALSE;
    auto suffix = path.substr(
        path.find_last_of(L'.') == std::wstring::npos ? path.size() : path.find_last_of(L'.'));
    for (auto& ch : suffix)
        ch = static_cast<wchar_t>(std::towlower(ch));
    if (suffix != L".cue" && suffix != L".chd") {
        try {
            return LoadPceFile(std::filesystem::path(path));
        } catch (const std::exception& ex) {
            MessageBox(ex.what(), "Cannot load ROM / ZIP", MB_OK | MB_ICONERROR);
            return FALSE;
        }
    }
    std::unique_ptr<rxpce_cd::Session> session;
    std::string error;
    m_preparing = true;
    m_cancelPrepare = false;
    bool ready = false;
    try {
        rxpce_config::DialogInputScope input;
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        ready = PrepareCdWithProgress(m_hWnd, path, rxpce_config::to_wide(m_config.cd.bios_file),
                                      m_cancelPrepare, session, error);
    } catch (...) {
        m_preparing = false;
        if (m_closeAfterPrepare) {
            m_closeAfterPrepare = false;
            PostMessage(WM_CLOSE);
        }
        throw;
    }
    m_preparing = false;
    m_videoShortcuts.reset();
    RefreshCursor();
    if (m_closeAfterPrepare) {
        m_closeAfterPrepare = false;
        PostMessage(WM_CLOSE);
        return FALSE;
    }
    if (!ready) {
        if (!m_cancelPrepare)
            MessageBox(error.c_str(), "Cannot load CD image", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    StopMainLoop();
    CaptureRuntime();
    const bool rebuild_video =
        video_settings.adapter != m_config.video.adapter ||
        video_settings.palette_file != rxpce_paths::from_utf8(m_config.video.palette_file);
    ApplySettings();
    m_overrides = {};
    if (rebuild_video) {
        ShutdownVideo();
        if (!InitializeVideo()) {
            PostMessage(WM_CLOSE);
            return FALSE;
        }
    }
    SetVideoPresentation(m_config.video.aspect_mode, m_config.video.zoom);
    rxpce_cd::queue(std::move(session));
    rxpce_host::runtime.rom_directory = std::filesystem::path(path).parent_path();
    return StartMainLoop();
}
BOOL CMainFrame::LoadPceFile(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path);
    rxpce_rom::Cartridge image;
    std::string error;
    if (!rxpce_rom::load(absolute, image, error)) {
        MessageBox(error.c_str(), "Cannot load ROM / ZIP", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    const auto source = image.source_path;
    StopMainLoop();
    CaptureRuntime();
    const bool rebuild_video =
        video_settings.adapter != m_config.video.adapter ||
        video_settings.palette_file != rxpce_paths::from_utf8(m_config.video.palette_file);
    ApplySettings();
    m_overrides = {};
    if (rebuild_video) {
        ShutdownVideo();
        if (!InitializeVideo()) {
            PostMessage(WM_CLOSE);
            return FALSE;
        }
    }
    SetVideoPresentation(m_config.video.aspect_mode, m_config.video.zoom);
    rxpce_cd::queue(nullptr);
    rxpce_rom::queue(std::move(image));
    rxpce_host::runtime.rom_directory = source.parent_path();
    if (!StartMainLoop())
        return FALSE;
    return TRUE;
}
LRESULT CMainFrame::OnCommand(WORD, WORD id, HWND, BOOL&) {
    try {

        if (m_preparing)
            return 0;
        if (m_stopping) {
            PostMessage(WM_COMMAND, id);
            return 0;
        }
        DWORD event = 0;
        switch (id) {
        case IDC_EXIT:
            OnExit();
            break;
        case IDC_OPEN:
            OnOpen();
            break;
        case IDC_STOP:
            StopEmulation();
            break;
        case IDC_SETTINGS:
            OnSettings();
            break;
        case IDC_ABOUT: {
            {
                rxpce_config::DialogInputScope input;
                ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
                CAboutDlg dialog;
                dialog.DoModal(m_hWnd);
            }
            RefreshCursor();
            break;
        }
        case IDC_ZOOM1:
            OnZoom1();
            break;
        case IDC_FULLSCREEN:
            OnFullScreen();
            break;
        case IDC_SAVE:
            event = EV_SAVE;
            break;
        case IDC_LOAD:
            event = EV_LOAD;
            break;
        case IDC_RESET:
            event = EV_RESET;
            break;
        }
        if (event)
            MachineCommands().post(event);
        return 0;
    } catch (const std::exception& error) {
        ShowFailure(error.what(), "rxpce operation failed");
    } catch (...) {
        ShowFailure("Unexpected application failure", "rxpce operation failed");
    }
    return 0;
}
LRESULT CMainFrame::OnVideoSize(UINT, WPARAM width, LPARAM height, BOOL&) {
    ResizeVideoWindow(static_cast<int>(width), static_cast<int>(height));
    return 0;
}
LRESULT CMainFrame::OnVideoDpiChanged(UINT, WPARAM, LPARAM position, BOOL&) {
    const RECT& r = *reinterpret_cast<const RECT*>(position);
    ::SetWindowPos(m_hWnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    ResizeVideoWindow(0, 0);
    return 0;
}
BOOL CMainFrame::PreTranslateMessage(MSG* message) {
    if (rxpce_config::dialog_open)
        return FALSE;
    auto& keyboard = rxpce_input::keyboard_state();
    keyboard.event(message->message, static_cast<unsigned>(message->wParam), message->lParam);
    if (message->wParam == VK_SHIFT || message->wParam == VK_CONTROL ||
        message->wParam == VK_MENU) {
        const auto scan = static_cast<UINT>((message->lParam >> 16) & 255) |
                          ((message->lParam & (LPARAM(1) << 24)) ? 0xe000 : 0);
        const auto side = MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);
        if (side)
            keyboard.event(message->message, side, message->lParam);
    }
    const auto result = m_videoShortcuts.process(message->message, message->wParam, message->lParam,
                                                 MachineCommands().ready(), bFullScreen.load());
    if (result.command == rxpce_video::ShortcutCommand::quit)
        OnExit();
    if (result.command == rxpce_video::ShortcutCommand::stop)
        StopEmulation();
    if (result.command == rxpce_video::ShortcutCommand::zoom)
        OnZoom1();
    if (result.command == rxpce_video::ShortcutCommand::fullscreen)
        OnFullScreen();
    DWORD event = 0;
    if (result.command == rxpce_video::ShortcutCommand::save)
        event = EV_SAVE;
    if (result.command == rxpce_video::ShortcutCommand::load)
        event = EV_LOAD;
    if (result.command == rxpce_video::ShortcutCommand::reset)
        event = EV_RESET;
    if (result.command == rxpce_video::ShortcutCommand::toggle_fps)
        rxpce_host::runtime.show_fps = !rxpce_host::runtime.show_fps.load();
    if (result.command == rxpce_video::ShortcutCommand::skip_down)
        event = EV_DOWNSKIPRATE;
    if (result.command == rxpce_video::ShortcutCommand::skip_up)
        event = EV_UPSKIPRATE;
    if (result.command == rxpce_video::ShortcutCommand::toggle_auto)
        event = EV_AUTOFRAMESKIP;
    if (event)
        MachineCommands().post(event);
    if (result.handled)
        return TRUE;
    return WTL::CFrameWindowImpl<CMainFrame>::PreTranslateMessage(message);
}
BOOL CMainFrame::LoadProfile() {
    std::string error;
    if (!rxpce_config::load(m_configPath, m_config, error)) {
        m_configWritable = false;
        MessageBox(
            (error + "\nUsing defaults. The file will be replaced only if you save Settings.")
                .c_str(),
            "Cannot load rxpce.json", MB_OK | MB_ICONWARNING);
        m_config = rxpce_config::Settings{};
    }
    ApplySettings();
    return TRUE;
}
void CMainFrame::ApplySettings() {
    video_settings.adapter = m_config.video.adapter;
    video_settings.zoom = m_config.video.zoom;
    video_settings.aspect_mode = m_config.video.aspect_mode;
    video_settings.palette_file = rxpce_paths::from_utf8(m_config.video.palette_file);
    rxpce_host::runtime.auto_frame_skip = m_config.video.auto_frame_skip;
    rxpce_host::runtime.frame_skip = m_config.video.frame_skip;
    rxpce_host::runtime.sound = {m_config.audio.enabled,
                                 m_config.audio.backend == "wasapi"
                                     ? rxpce_audio::OutputBackend::wasapi
                                     : rxpce_audio::OutputBackend::directsound,
                                 static_cast<unsigned>(m_config.audio.sample_rate),
                                 static_cast<unsigned>(m_config.audio.channels),
                                 static_cast<unsigned>(m_config.audio.latency_ms),
                                 static_cast<unsigned>(m_config.advanced.base_clock_hz),
                                 m_config.audio.cooperative_level};
    rxpce_config::active_input = m_config.input;
    rxpce_host::runtime.rom_directory = rxpce_paths::from_utf8(m_config.folders.rom);
    rxpce_host::runtime.state_directory = rxpce_paths::from_utf8(m_config.folders.state);
    rxpce_host::runtime.base_clock_hz = m_config.advanced.base_clock_hz;
    rxpce_host::runtime.show_fps = m_config.advanced.show_fps;
    SetLogDumpDetail(m_config.advanced.log);
}
void CMainFrame::CaptureRuntime() {
    auto runtime = m_config;
    runtime.video.zoom = video_settings.zoom;
    runtime.video.aspect_mode = video_settings.aspect_mode;
    runtime.video.auto_frame_skip = rxpce_host::runtime.auto_frame_skip.load();
    runtime.video.frame_skip = rxpce_host::runtime.frame_skip.load();
    runtime.advanced.show_fps = rxpce_host::runtime.show_fps != FALSE;
    runtime.folders.rom = rxpce_paths::utf8(rxpce_host::runtime.rom_directory);
    rxpce_config::merge_runtime(m_config, runtime, m_overrides);
}
void CMainFrame::SaveProfile() {
    if (!m_configWritable)
        return;
    CaptureRuntime();
    std::string error;
    if (!rxpce_config::save(m_configPath, m_config, error))
        MessageBox(error.c_str(), "Cannot save rxpce.json", MB_OK | MB_ICONERROR);
}
void CMainFrame::OnSettings() {
    CaptureRuntime();
    const auto original = m_config;
    SettingsDialog dialog(m_config);
    INT_PTR result = IDCANCEL;
    {
        rxpce_config::DialogInputScope input;
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        result = dialog.DoModal(m_hWnd);
    }
    RefreshCursor();
    m_videoShortcuts.reset();
    if (result != IDOK)
        return;
    std::string error;
    if (!rxpce_config::save(m_configPath, dialog.value, error)) {
        MessageBox(error.c_str(), "Cannot save rxpce.json", MB_OK | MB_ICONERROR);
        return;
    }
    m_overrides.auto_skip |= dialog.value.video.auto_frame_skip != original.video.auto_frame_skip;
    m_overrides.skip |= dialog.value.video.frame_skip != original.video.frame_skip;
    m_overrides.fps |= dialog.value.advanced.show_fps != original.advanced.show_fps;
    m_overrides.rom |= dialog.value.folders.rom != original.folders.rom;
    m_config = dialog.value;
    m_configWritable = true;
    SetVideoPresentation(m_config.video.aspect_mode, m_config.video.zoom);
}
void CMainFrame::StopEmulation() {
    StopMainLoop();
    CaptureRuntime();
    if (bFullScreen)
        ChangeWindowMode();
    ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
    if (!ResetVideoToIdle()) {
        PostMessage(WM_CLOSE);
        return;
    }
    m_videoShortcuts.reset();
    OnIdle();
    LogDumpDetail("Emulation stopped; idle\n");
}
LRESULT CMainFrame::OnSetCursor(UINT, WPARAM, LPARAM hit, BOOL& handled) {
    if (SetFullscreenCursor(LOWORD(hit), rxpce_config::dialog_open.load()))
        return TRUE;
    handled = FALSE;
    return 0;
}
void CMainFrame::RefreshCursor() {
    POINT point{};
    if (!GetCursorPos(&point) || WindowFromPoint(point) != m_hWnd ||
        GetForegroundWindow() != m_hWnd)
        return;
    if (!SetFullscreenCursor(HTCLIENT, rxpce_config::dialog_open.load()))
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
}
