#pragma once
#include "wtl_support.h"
#include "video.h"
#include "video_shortcuts.h"
#include "settings.h"
#include "unique_handle.h"
#include "replay_session.h"
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
    MESSAGE_HANDLER(WM_APP + 9, OnReplayFinished)
    COMMAND_RANGE_HANDLER(IDC_EXIT, IDC_STOP, OnCommand)
    COMMAND_RANGE_HANDLER(IDC_INPUT_PROFILE_FIRST, IDC_INPUT_PROFILE_LAST, OnInputProfile)
    COMMAND_RANGE_HANDLER(IDC_REPLAY_RECORD, IDC_REPLAY_DISCARD, OnReplayCommand)
    COMMAND_RANGE_HANDLER(IDC_RECENT_GAME_FIRST, IDC_CLEAR_GAME_PROFILE, OnGameHistory)
    CHAIN_MSG_MAP(WTL::CFrameWindowImpl<CMainFrame>)
    END_MSG_MAP()
  private:
    std::wstring m_commandPath;
    std::filesystem::path m_configPath;
    rxpce_config::Settings m_config;
    rxpce_config::RuntimeOverrides m_overrides;
    bool m_configWritable = true;
    rxpce_host::UniqueHandle m_thread;
    std::unique_ptr<rxpce_replay::Session> m_replay;
    std::optional<rxpce_replay::Recording> m_pendingRecording;
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
    BOOL StartMainLoop(std::shared_ptr<const rxpce_replay::Recording> playback = {});
    void StopMainLoop();
    void StopEmulation(bool offer_save = true);
    void RefreshCursor();
    void ShowFailure(const char* detail, const char* title);
    void ApplySettings(const std::filesystem::path& game = {});
    BOOL LoadPceFile(const std::filesystem::path&,
                     std::shared_ptr<const rxpce_replay::Recording> playback = {});
    BOOL LoadMediaPath(const std::wstring&,
                       std::shared_ptr<const rxpce_replay::Recording> playback = {});
    static unsigned __stdcall InvokeMainLoop(void*);
    void OnExit();
    void OnZoom1();
    void OnFullScreen();
    void OnOpen();
    void UpdateRecentGamesMenu();
    LRESULT OnGameHistory(WORD, WORD, HWND, BOOL&);
    std::wstring ChooseMediaPath();
    std::wstring ChooseReplayPath(bool save);
    bool SavePendingReplay();
    bool CollectFinishedReplay();
    bool ReplayInProgress() const;
    void PlayReplay();
    LRESULT OnReplayCommand(WORD, WORD, HWND, BOOL&);
    LRESULT OnReplayFinished(UINT, WPARAM, LPARAM, BOOL&);
    void OnSettings();
    void CaptureRuntime();
    bool CanSelectInputProfile() const;
    void UpdateInputProfileLabels();
    LRESULT OnInputProfile(WORD, WORD, HWND, BOOL&);
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
