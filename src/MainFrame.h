#pragma once
#include "wtl_support.h"
#include "video.h"
#include "video_shortcuts.h"
#include "settings.h"
#include "unique_handle.h"
class CMainFrame : public WTL::CFrameWindowImpl<CMainFrame>,
                   public WTL::CMessageFilter,
                   public WTL::CIdleHandler {
  public:
    DECLARE_FRAME_WND_CLASS("rxpce_main", IDR_MAINFRAME)
    explicit CMainFrame(const std::filesystem::path& profile) : m_configPath(profile) {}
    BOOL PreTranslateMessage(MSG* message) override;
    BOOL OnIdle() override;
    BEGIN_MSG_MAP(CMainFrame)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_ACTIVATEAPP, OnActivateApp)
    MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
    MESSAGE_HANDLER(WM_INITMENU, OnInitMenu)
    MESSAGE_HANDLER(WM_VIDEO_INITIALIZE, OnVideoInitialize)
    MESSAGE_HANDLER(WM_VIDEO_SIZE, OnVideoSize)
    MESSAGE_HANDLER(WM_DPICHANGED, OnVideoDpiChanged)
    MESSAGE_HANDLER(WM_APP + 8, OnMainLoopExited)
    COMMAND_RANGE_HANDLER(IDC_EXIT, IDC_STOP, OnCommand)
    CHAIN_MSG_MAP(WTL::CFrameWindowImpl<CMainFrame>)
    END_MSG_MAP()
  private:
    std::wstring m_commandPath;
    std::filesystem::path m_configPath;
    rxpce_config::Settings m_config;
    rxpce_config::RuntimeOverrides m_overrides;
    bool m_configWritable = true;
    rxpce_host::UniqueHandle m_thread;
    unsigned m_generation = 0;
    bool m_stopping = false;
    bool m_preparing = false, m_closeAfterPrepare = false;
    std::atomic_bool m_cancelPrepare{false};
    struct WorkerStart {
        CMainFrame* frame;
        unsigned generation;
    };
    rxpce_video::Shortcuts m_videoShortcuts;
    HMENU m_menu = nullptr;
    BOOL m_profileLoaded = FALSE;
    BOOL LoadProfile();
    void SaveProfile();
    BOOL StartMainLoop();
    void StopMainLoop();
    void StopEmulation();
    void RefreshCursor();
    void ShowFailure(const char* detail, const char* title);
    void ApplySettings();
    BOOL LoadPceFile(const std::filesystem::path&);
    BOOL LoadMediaPath(const std::wstring&);
    static unsigned __stdcall InvokeMainLoop(void*);
    void OnExit();
    void OnZoom1();
    void OnFullScreen();
    void OnOpen();
    void OnSettings();
    void CaptureRuntime();
    LRESULT OnCommand(WORD, WORD, HWND, BOOL&);
    LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnActivateApp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSetCursor(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnInitMenu(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnVideoInitialize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnVideoSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnVideoDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMainLoopExited(UINT, WPARAM, LPARAM, BOOL&);
};
