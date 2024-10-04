#include "stdafx.h"
#include "helper.hpp"

#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <safetyhook.hpp>

HMODULE baseModule = GetModuleHandle(NULL);
HMODULE thisModule; // Fix DLL

// Version
std::string sFixName = "OPPW4Fix";
std::string sFixVer = "0.0.4";
std::string sLogFile = sFixName + ".log";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::filesystem::path sExePath;
std::string sExeName;
std::filesystem::path sThisModulePath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";
std::pair DesktopDimensions = { 0,0 };

// Ini variables
bool bCustomRes;
int iCustomResX = 1280;
int iCustomResY = 720;
bool bFixFOV;
bool bFixAspect;
bool bFixHUD;
bool bSkipIntro;
int iFramerateCap;
float fGameplayFOVMulti;
int iShadowResolution;
bool bRenderTextureRes;

// Aspect ratio + HUD stuff
float fPi = (float)3.141592653;
float fAspectRatio;
float fNativeAspect = (float)16 / 9;
float fAspectMultiplier;
float fHUDWidth;
float fHUDHeight;
float fHUDWidthOffset;
float fHUDHeightOffset;

// Variables
int iCurrentResX;
int iCurrentResY;
float fCurrentFrametime = 0.0166666f;
bool bIsMoviePlaying = false;

void CalculateAspectRatio(bool bLog)
{
    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD variables
    fHUDWidth = iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2;
    fHUDHeightOffset = 0;
    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2;
    }

    if (bLog) {
        // Log details about current resolution
        spdlog::info("----------");
        spdlog::info("Current Resolution: Resolution: {}x{}", iCurrentResX, iCurrentResY);
        spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
        spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
        spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
        spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        spdlog::info("----------");
    }   
}

// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size) {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open()) {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (std::filesystem::exists(_filename) && std::filesystem::file_size(_filename) >= _max_size) {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file() {
        if (std::filesystem::exists(_filename)) {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

void Logging()
{
    // Get this module path
    WCHAR thisModulePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(thisModule, thisModulePath, MAX_PATH);
    sThisModulePath = thisModulePath;
    sThisModulePath = sThisModulePath.remove_filename();

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    // spdlog initialisation
    {
        try {
            // Create 10MB truncated logger
            logger = logger = std::make_shared<spdlog::logger>(sLogFile, std::make_shared<size_limited_sink<std::mutex>>(sThisModulePath.string() + sLogFile, 10 * 1024 * 1024));
            spdlog::set_default_logger(logger);

            spdlog::flush_on(spdlog::level::debug);
            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", sFixName.c_str(), sFixVer.c_str());
            spdlog::info("----------");
            spdlog::info("Log file: {}", sThisModulePath.string() + sLogFile);
            spdlog::info("----------");

            // Log module details
            spdlog::info("Module Name: {0:s}", sExeName.c_str());
            spdlog::info("Module Path: {0:s}", sExePath.string());
            spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
            spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(baseModule));
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(baseModule, 1);
        }
    }
}

void Configuration()
{
    // Initialise config
    std::ifstream iniFile(sThisModulePath.string() + sConfigFile);
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVer.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sThisModulePath.string().c_str() << std::endl;
        FreeLibraryAndExitThread(baseModule, 1);
    }
    else {
        spdlog::info("Config file: {}", sThisModulePath.string() + sConfigFile);
        ini.parse(iniFile);
    }

    // Parse config
    ini.strip_trailing_comments();
    spdlog::info("----------");

    inipp::get_value(ini.sections["Custom Resolution"], "Enabled", bCustomRes);
    inipp::get_value(ini.sections["Custom Resolution"], "Width", iCustomResX);
    inipp::get_value(ini.sections["Custom Resolution"], "Height", iCustomResY);
    spdlog::info("Config Parse: bCustomRes: {}", bCustomRes);
    spdlog::info("Config Parse: iCustomResX: {}", iCustomResX);
    spdlog::info("Config Parse: iCustomResY: {}", iCustomResY);

    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFixFOV);
    spdlog::info("Config Parse: bFixFOV: {}", bFixFOV);

    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bFixAspect);
    spdlog::info("Config Parse: bFixAspect: {}", bFixAspect);

    inipp::get_value(ini.sections["Fix HUD"], "Enabled", bFixHUD);
    spdlog::info("Config Parse: bFixHUD: {}", bFixHUD);

    inipp::get_value(ini.sections["Skip Intro"], "Enabled", bSkipIntro);
    spdlog::info("Config Parse: bSkipIntro: {}", bSkipIntro);

    inipp::get_value(ini.sections["Gameplay FOV"], "Multiplier", fGameplayFOVMulti);
    if ((float)fGameplayFOVMulti < 0.10f || (float)fGameplayFOVMulti > 3.00f) {
        fGameplayFOVMulti = std::clamp((float)fGameplayFOVMulti, 0.10f, 3.00f);
        spdlog::warn("Config Parse: fGameplayFOVMulti value invalid, clamped to {}", fGameplayFOVMulti);
    }
    spdlog::info("Config Parse: fGameplayFOVMulti: {}", fGameplayFOVMulti);

    inipp::get_value(ini.sections["Render Texture Resolution"], "Enabled", bRenderTextureRes);
    spdlog::info("Config Parse: bRenderTextureRes: {}", bRenderTextureRes);

    inipp::get_value(ini.sections["Framerate Cap"], "Framerate", iFramerateCap);
    if (iFramerateCap < 10 || iFramerateCap > 500) {
        iFramerateCap = std::clamp(iFramerateCap, 10, 500);
        spdlog::warn("Config Parse: iFramerateCap value invalid, clamped to {}", iFramerateCap);
    }
    spdlog::info("Config Parse: iFramerateCap: {}", iFramerateCap);

    inipp::get_value(ini.sections["Shadow Quality"], "Resolution", iShadowResolution);
    if (iShadowResolution < 64 || iShadowResolution > 16384) {
        iShadowResolution = std::clamp(iShadowResolution, 64, 16384);
        spdlog::warn("Config Parse: iShadowResolution value invalid, clamped to {}", iShadowResolution);
    }
    spdlog::info("Config Parse: iShadowResolution: {}", iShadowResolution);

    spdlog::info("----------");

    // Grab desktop resolution
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();
    if (iCustomResX == 0 && iCustomResY == 0) {
        iCustomResX = DesktopDimensions.first;
        iCustomResY = DesktopDimensions.second;
        spdlog::info("Config Parse: Using desktop resolution of {}x{} as custom resolution.", iCustomResX, iCustomResY);
    }

    // Calculate aspect ratio
    iCurrentResX = iCustomResX;
    iCurrentResY = iCustomResY;
    CalculateAspectRatio(true);
}

void Resolution()
{
    uint8_t* CurrentResolutionScanResult = Memory::PatternScan(baseModule, "89 ?? ?? 89 ?? ?? 48 ?? ?? ?? 89 ?? ?? 89 ?? ?? 48 ?? ?? ?? 74 ?? FF ?? ?? ?? ?? ??");
    if (CurrentResolutionScanResult) {
        spdlog::info("Current Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CurrentResolutionScanResult - (uintptr_t)baseModule);
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx) {
                // Log resolution
                int iResX = (int)ctx.rsi;
                int iResY = (int)ctx.rdi;

                if (iResX != iCurrentResX || iResY != iCurrentResY) {
                    iCurrentResX = iResX;
                    iCurrentResY = iResY;
                    CalculateAspectRatio(true);
                }
            });
    }
    else if (!CurrentResolutionScanResult) {
        spdlog::error("Current Resolution: Pattern scan failed.");
    }

    // Add custom resolution
    if (bCustomRes) {
        // These patterns will always be valid but may not be in memory yet. 
        // So we'll attempt to scan for it for 30 seconds and give the game time to load it.
        uint8_t* ResolutionList1ScanResult = nullptr;
        for (int attempts = 0; attempts < 300; ++attempts) {
            if (ResolutionList1ScanResult = Memory::PatternScan(baseModule, "00 05 00 00 D0 02 00 00 56 05 00 00"))
                break; // Exit loop
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep and check again
        }
        uint8_t* ResolutionList2ScanResult = Memory::PatternScan(baseModule, "00 05 D0 02 56 05 00 03 40 06");
        if (ResolutionList1ScanResult && ResolutionList2ScanResult) {
            spdlog::info("Custom Resolution: List 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionList1ScanResult - (uintptr_t)baseModule);
            spdlog::info("Custom Resolution: List 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionList2ScanResult - (uintptr_t)baseModule);

            // Replace 1280x720 with new resolution
            Memory::Write((uintptr_t)ResolutionList1ScanResult, iCustomResX);
            Memory::Write((uintptr_t)ResolutionList1ScanResult + 0x4, iCustomResY);
            Memory::Write((uintptr_t)ResolutionList2ScanResult, (short)iCustomResX);
            Memory::Write((uintptr_t)ResolutionList2ScanResult + 0x2, (short)iCustomResY);
            spdlog::info("Custom Resolution: List: Replaced 1280x720 with {}x{}", iCustomResX, iCustomResY);
        }
        else if (!ResolutionList1ScanResult || !ResolutionList2ScanResult) {
            spdlog::error("Custom Resolution: Pattern scan(s) failed.");
        }

        // Spoof GetSystemMetrics results so our custom resolution is always valid
        uint8_t* SystemMetricsScanResult = Memory::PatternScan(baseModule, "89 ?? ?? ?? ?? ?? FF ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? 48 89 ?? ?? ?? ?? ?? ?? ?? ?? 48 89 ?? ?? 89 ?? ??");
        uint8_t* ResCheckScanResult = Memory::PatternScan(baseModule, "74 ?? 3B ?? ?? 77 ?? 3B ?? 0F ?? ?? ?? ?? ?? FF ?? 48 ?? ?? ??");
        if (SystemMetricsScanResult && ResCheckScanResult) {
            spdlog::info("Custom Resolution: GetSystemMetrics: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)SystemMetricsScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ReportedWidthMidHook{};
            ReportedWidthMidHook = safetyhook::create_mid(SystemMetricsScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.rax = iCustomResX;
                });

            static SafetyHookMid ReportedHeightMidHook{};
            ReportedHeightMidHook = safetyhook::create_mid(SystemMetricsScanResult + 0xC,
                [](SafetyHookContext& ctx) {
                    ctx.rax = iCustomResY;
                });

            // Allow internal resolution that is higher than the output
            spdlog::info("Custom Resolution: GetSystemMetrics: ResCheck: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResCheckScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)ResCheckScanResult, "\xE9\x89\x00\x00\x00", 5);
            spdlog::info("Custom Resolution: GetSystemMetrics: ResCheck: Patched instruction.");
        }
        else if (!SystemMetricsScanResult || !ResCheckScanResult) {
            spdlog::error("Custom Resolution: GetSystemMetrics: Pattern scan(s) failed.");
        }
    }
}

void SkipIntro()
{
    if (bSkipIntro) {
        // Opening State
        uint8_t* OpeningStateScanResult = Memory::PatternScan(baseModule, "48 ?? ?? 83 ?? 0E 0F 87 ?? ?? ?? ?? 48 ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ??");
        if (OpeningStateScanResult) {
            spdlog::info("Intro Skip: Opening State: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)OpeningStateScanResult - (uintptr_t)baseModule);
            static SafetyHookMid OpeningStateMidHook{};
            OpeningStateMidHook = safetyhook::create_mid(OpeningStateScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rax == 0x04)
                        ctx.rax = 0x0E;
                });
        }
        else if (!OpeningStateScanResult) {
            spdlog::error("Intro Skip: Opening State: Pattern scan failed.");
        }
    } 
}

void AspectFOV()
{
    if (bFixAspect) {
        // Markers + Enemy Culling Aspect Ratio
        uint8_t* CullingMarkersAspectScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? ?? 48 ?? ?? 89 ?? ?? ?? ?? ?? 66 ?? ?? ?? ?? ?? ?? 00 01");
        if (CullingMarkersAspectScanResult) {
            spdlog::info("Aspect Ratio: Markers/Culling: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CullingMarkersAspectScanResult - (uintptr_t)baseModule);
            static SafetyHookMid CullingMarkersAspectMidHook{};
            CullingMarkersAspectMidHook = safetyhook::create_mid(CullingMarkersAspectScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rcx + 0x1B0) {
                        if (fAspectRatio != fNativeAspect)
                            *reinterpret_cast<float*>(ctx.rcx + 0x1B0) = fAspectRatio;
                    }                
                });
        }
        else if (!CullingMarkersAspectScanResult) {
            spdlog::error("Aspect Ratio: Markers/Culling: Pattern scan failed.");
        }
    }

    if (fGameplayFOVMulti != 1.00f) {
        // Gameplay FOV
        uint8_t* GameplayFOVScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? 78 ?? ?? ?? 0F ?? ?? F3 0F ?? ?? E8 ?? ?? ?? ?? 85 ?? 75 ?? F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ??");
        if (GameplayFOVScanResult) {
            spdlog::info("FOV: Gameplay: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameplayFOVScanResult - (uintptr_t)baseModule);
            static SafetyHookMid GameplayFOVMidHook{};
            GameplayFOVMidHook = safetyhook::create_mid(GameplayFOVScanResult + 0x8,
                [](SafetyHookContext& ctx) {
                    ctx.xmm4.f32[0] *= fGameplayFOVMulti;
                });
        }
        else if (!GameplayFOVScanResult) {
            spdlog::error("FOV: Gameplay: Pattern scan failed.");
        }
    }

    if (bFixFOV) {
        // Cutscene FOV
        uint8_t* CutsceneFOVScanResult = Memory::PatternScan(baseModule, "00 0F 84 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? 0F ?? ?? ?? ?? 0F ?? ?? ?? ?? 0F ?? ??");
        if (CutsceneFOVScanResult) {
            spdlog::info("FOV: Cutscene: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CutsceneFOVScanResult - (uintptr_t)baseModule);
            static SafetyHookMid CutsceneFOVMidHook{};
            CutsceneFOVMidHook = safetyhook::create_mid(CutsceneFOVScanResult + 0xF,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = fNativeAspect;
                });
        }
        else if (!CutsceneFOVScanResult) {
            spdlog::error("FOV: Cutscene: Pattern scan failed.");
        }
    }
}

void HUD()
{
    if (bFixHUD) {
        // HUD Size
        uint8_t* HUDSizeScanResult = Memory::PatternScan(baseModule, "45 ?? ?? 75 ?? 0F 28 ?? ?? ?? ?? ?? 0F ?? ?? F2 0F ?? ?? ?? ?? ?? ?? 33 ??");
        if (HUDSizeScanResult) {
            spdlog::info("HUD: Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDSizeScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDSizeMidHook{};
            HUDSizeMidHook = safetyhook::create_mid(HUDSizeScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect) {
                        ctx.xmm9.f32[0] *= 1920.00f;
                        ctx.xmm9.f32[0] /= 1080.00f * fAspectRatio;
                    }
                    else if (fAspectRatio < fNativeAspect) {
                        ctx.xmm7.f32[0] *= 1080.00f;
                        ctx.xmm7.f32[0] /= 1920.00f / fAspectRatio;
                    }
                });
        }
        else if (!HUDSizeScanResult) {
            spdlog::error("HUD: Size: Pattern scan failed.");
        }

        // Minimap Position
        uint8_t* MinimapPositionScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? 0F ?? ?? 76 ?? F3 0F ?? ?? EB ?? F3 0F ?? ?? f3 0F ?? ?? 89 ?? ?? ?? 45 ?? ??");
        if (MinimapPositionScanResult) {
            spdlog::info("HUD: Minimap Position: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MinimapPositionScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MinimapPositionWidthMidHook{};
            MinimapPositionWidthMidHook = safetyhook::create_mid(MinimapPositionScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = 1080.00f * fAspectRatio;
                });

            static SafetyHookMid MinimapPositionHeightMidHook{};
            MinimapPositionHeightMidHook = safetyhook::create_mid(MinimapPositionScanResult + 0x2C,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = 1920.00f / fAspectRatio;
                });
        }
        else if (!MinimapPositionScanResult) {
            spdlog::error("HUD: Minimap Position: Pattern scan failed.");
        }

        // Key Guides
        uint8_t* KeyGuide1ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? E8 ?? ?? ?? ??");
        uint8_t* KeyGuide2ScanResult = Memory::PatternScan(baseModule, "0F ?? ?? 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ??");
        uint8_t* KeyGuide3ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? 48 8B ?? ?? ??");
        if (KeyGuide1ScanResult && KeyGuide2ScanResult && KeyGuide3ScanResult) {
            spdlog::info("HUD: Key Guide: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)KeyGuide1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid KeyGuide1MidHook{};
            KeyGuide1MidHook = safetyhook::create_mid(KeyGuide1ScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = fHUDWidth;
                });

            spdlog::info("HUD: Key Guide: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)KeyGuide2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid KeyGuide2MidHook{};
            KeyGuide2MidHook = safetyhook::create_mid(KeyGuide2ScanResult + 0x6,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = fHUDWidth;
                });

            spdlog::info("HUD: Key Guide: 3: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)KeyGuide3ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid KeyGuide3MidHook{};
            KeyGuide3MidHook = safetyhook::create_mid(KeyGuide3ScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = fHUDWidth;
                });
        }
        else if (!KeyGuide1ScanResult || !KeyGuide2ScanResult || !KeyGuide3ScanResult) {
            spdlog::error("HUD: Key Guide: Pattern scan(s) failed.");
        }

        // Button Height
        uint8_t* ButtonHeight1ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F 28 ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 44 ?? ?? ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ??");
        uint8_t* ButtonHeight2ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F 28 ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 44 ?? ?? ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? 44 ?? ?? ?? ?? ?? ??");
        if (ButtonHeight1ScanResult && ButtonHeight2ScanResult) {
            spdlog::info("HUD: Button Height: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ButtonHeight1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ButtonHeight1MidHook{};
            ButtonHeight1MidHook = safetyhook::create_mid(ButtonHeight1ScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm2.f32[0] = fHUDHeight;
                });

            spdlog::info("HUD: Button Height: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ButtonHeight2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ButtonHeight2MidHook{};
            ButtonHeight2MidHook = safetyhook::create_mid(ButtonHeight2ScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm2.f32[0] = fHUDHeight;
                });
        }
        else if (!ButtonHeight1ScanResult || !ButtonHeight2ScanResult) {
            spdlog::error("HUD: Button Height: Pattern scan(s) failed.");
        }

        // Menu Selections
        uint8_t* MenuSelectionsScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? 83 ?? ?? 7C ?? F3 0F ?? ?? ?? ?? ?? ?? EB ??");
        if (MenuSelectionsScanResult) {
            spdlog::info("HUD: Menu Selections: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MenuSelectionsScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MenuSelectionsMidHook{};
            MenuSelectionsMidHook = safetyhook::create_mid(MenuSelectionsScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm1.f32[0] = fHUDWidth;
                });
        }
        else if (!MenuSelectionsScanResult) {
            spdlog::error("HUD: Menu Selections: Pattern scan failed.");
        }

        // Minimap Icons
        uint8_t* MinimapIconsScanResult = Memory::PatternScan(baseModule, "F3 41 ?? ?? ?? ?? F3 41 ?? ?? ?? ?? F3 44 ?? ?? ?? F3 44 ?? ?? ?? 0F ?? ?? ?? 0F 83 ?? ?? ?? ??");
        if (MinimapIconsScanResult) {
            spdlog::info("HUD: Minimap Icons: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MinimapIconsScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MinimapIconsMidHook{};
            MinimapIconsMidHook = safetyhook::create_mid(MinimapIconsScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect) {
                        ctx.xmm1.f32[0] *= 1920.00f;
                        ctx.xmm1.f32[0] /= 1080.00f * fAspectRatio;
                    }
                    else if (fAspectRatio < fNativeAspect) {
                        ctx.xmm0.f32[0] *= 1080.00f;
                        ctx.xmm0.f32[0] /= 1920.00f / fAspectRatio;
                    }
                });
        }
        else if (!MinimapIconsScanResult) {
            spdlog::error("HUD: Minimap Icons: Pattern scan failed.");
        }

        // Gameplay HUD
        uint8_t* GameplayHUDScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? 66 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ??");
        if (GameplayHUDScanResult) {
            spdlog::info("HUD: Gameplay HUD: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameplayHUDScanResult - (uintptr_t)baseModule);
            static SafetyHookMid GameplayHUDWidthMidHook{};
            GameplayHUDWidthMidHook = safetyhook::create_mid(GameplayHUDScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm1.f32[0] = fHUDWidth;
                });
        }
        else if (!GameplayHUDScanResult) {
            spdlog::error("HUD: Gameplay HUD: Pattern scan failed.");
        }

        // Get movie state
        uint8_t* MovieStateScanResult = Memory::PatternScan(baseModule, "4C ?? ?? 83 ?? 16 0F 87 ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ??");
        if (MovieStateScanResult) {
            spdlog::info("HUD: Movie State: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MovieStateScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MovieStateMidHook{};
            MovieStateMidHook = safetyhook::create_mid(MovieStateScanResult,
                [](SafetyHookContext& ctx) {
                    // Is movie playing/paused
                    if ((int)ctx.rax == 0x0B || (int)ctx.rax == 0x0C || (int)ctx.rax == 0x0D || (int)ctx.rax == 0x0F || (int)ctx.rax == 0x10) {
                        bIsMoviePlaying = true;
                    }
                    else {
                        bIsMoviePlaying = false;
                    }
                });
        }
        else if (!MovieStateScanResult) {
            spdlog::error("HUD: Movie State: Pattern scan failed.");
        }

        // Fades + Movies
        uint8_t* FadesScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? 00 89 ?? ?? 49 ?? ?? ?? 48 ?? ?? FF ?? ?? ?? ?? 00");
        if (FadesScanResult) {
            spdlog::info("HUD: Fades: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FadesScanResult - (uintptr_t)baseModule);
            static SafetyHookMid FadesMidHook{};
            FadesMidHook = safetyhook::create_mid(FadesScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (ctx.rax + 0xF0) {
                        // Check for fade to black (2689x1793)
                        if (*reinterpret_cast<short*>(ctx.rax + 0xF0) == (short)2689 && *reinterpret_cast<short*>(ctx.rax + 0xF2) == (short)1793) {
                            if (fAspectRatio > fNativeAspect) {
                                *reinterpret_cast<short*>(ctx.rax + 0xF0) = static_cast<int>(1793 * fAspectRatio); // Set new width
                            }
                            else if (fAspectRatio < fNativeAspect) {
                                *reinterpret_cast<short*>(ctx.rax + 0xF2) = static_cast<int>(2689 / fAspectRatio); // Set new height
                            }
                        }

                        // Fix movies
                        char* sElementName = (char*)ctx.rax + 0x280;
                        if (strcmp(sElementName, "ktglkids_scl_capture_plane_full_rgba8") == 0) {
                            if (bIsMoviePlaying) {
                                if (fAspectRatio > fNativeAspect) {
                                    *reinterpret_cast<short*>(ctx.rax + 0xF0) = static_cast<short>(std::round(fHUDWidth));
                                }
                                else if (fAspectRatio < fNativeAspect) {
                                    *reinterpret_cast<short*>(ctx.rax + 0xF2) = static_cast<short>(std::round(fHUDHeight));
                                }
                            }
                            else if (!bIsMoviePlaying) {
                                if (fAspectRatio > fNativeAspect) {
                                    *reinterpret_cast<short*>(ctx.rax + 0xF0) = (short)iCurrentResX;
                                }
                                else if (fAspectRatio < fNativeAspect) {
                                    *reinterpret_cast<short*>(ctx.rax + 0xF2) = (short)iCurrentResY;
                                }
                            }
                        }
                    }
                });
        }
        else if (!FadesScanResult) {
            spdlog::error("HUD: Fades: Pattern scan failed.");
        }

        // Screen size
        uint8_t* ScreenSizeScanResult = Memory::PatternScan(baseModule, "41 ?? ?? ?? 80 ?? ?? ?? 00 41 ?? 01 00 00 00 F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ??");
        if (ScreenSizeScanResult) {
            spdlog::info("HUD: Screen Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ScreenSizeScanResult - (uintptr_t)baseModule);

            static SafetyHookMid ScreenSizeMidHook{};
            ScreenSizeMidHook = safetyhook::create_mid(ScreenSizeScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.r8 + 0x60) {
                        if (*reinterpret_cast<short*>(ctx.r8 + 0x60) == (short)1920 && *reinterpret_cast<short*>(ctx.r8 + 0x62) == (short)1080) {
                            if (fAspectRatio > fNativeAspect) {
                                *reinterpret_cast<short*>(ctx.r8 + 0x60) = static_cast<short>(1080.00f * fAspectRatio);
                            }
                            else if (fAspectRatio < fNativeAspect) {
                                *reinterpret_cast<short*>(ctx.r8 + 0x62) = static_cast<short>(1920.00f / fAspectRatio);
                            }
                        }
                    }
                });
        }
        else if (!ScreenSizeScanResult) {
            spdlog::error("HUD: Screen Size: Pattern scan failed.");
        }

        // Growth Map
        uint8_t* GrowthMapScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? 66 ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? 66 0F ?? ?? ?? ?? ?? ??");
        if (GrowthMapScanResult) {
            spdlog::info("HUD: Growth Map: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GrowthMapScanResult - (uintptr_t)baseModule);
            static SafetyHookMid GrowthMapWidthMidHook{};
            GrowthMapWidthMidHook = safetyhook::create_mid(GrowthMapScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDWidth;
                });

            static SafetyHookMid GrowthMapHeightMidHook{};
            GrowthMapHeightMidHook = safetyhook::create_mid(GrowthMapScanResult + 0x32,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDHeight;
                });
        }
        else if (!GrowthMapScanResult) {
            spdlog::error("HUD: Growth Map: Pattern scan failed.");
        }

        // Soul Map
        uint8_t* SoulMapScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? 66 ?? ?? ?? ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? 66 0F ?? ?? ?? ?? ?? ??");
        if (SoulMapScanResult) {
            spdlog::info("HUD: Soul Map: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)SoulMapScanResult - (uintptr_t)baseModule);
            static SafetyHookMid SoulMapWidthMidHook{};
            SoulMapWidthMidHook = safetyhook::create_mid(SoulMapScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDWidth;
                });

            static SafetyHookMid SoulMapHeightMidHook{};
            SoulMapHeightMidHook = safetyhook::create_mid(SoulMapScanResult + 0x32,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDHeight;
                });
        }
        else if (!SoulMapScanResult) {
            spdlog::error("HUD: Soul Map: Pattern scan failed.");
        }

        // Mission Select
        uint8_t* MissionSelect1ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? F3 41 ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? ?? F3 0F ?? ?? ?? ?? 0F 28 ?? ?? ??");
        uint8_t* MissionSelect2ScanResult = Memory::PatternScan(baseModule, "0F ?? ?? F3 41 ?? ?? ?? 66 0F ?? ?? 41 0F ?? ?? ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ??");
        if (MissionSelect1ScanResult && MissionSelect2ScanResult) {
            spdlog::info("HUD: Mission Select: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MissionSelect1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MissionSelect1SizeMidHook{};
            MissionSelect1SizeMidHook = safetyhook::create_mid(MissionSelect1ScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = fHUDWidth;
                });

            static SafetyHookMid MissionSelect1OffsetMidHook{};
            MissionSelect1OffsetMidHook = safetyhook::create_mid(MissionSelect1ScanResult - 0x1E,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] += ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                });

            spdlog::info("HUD: Mission Select: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MissionSelect2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MissionSelect2SizeMidHook{};
            MissionSelect2SizeMidHook = safetyhook::create_mid(MissionSelect2ScanResult + 0x3,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm2.f32[0] = fHUDWidth;
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm3.f32[0] = fHUDHeight;
                });

            static SafetyHookMid MissionSelect2OffsetWidthMidHook{};
            MissionSelect2OffsetWidthMidHook = safetyhook::create_mid(MissionSelect2ScanResult + 0x23,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] += ((1080.00f * fAspectRatio) - 1920.00f) / 2.00f;
                });

            static SafetyHookMid MissionSelect2OffsetHeightMidHook{};
            MissionSelect2OffsetHeightMidHook = safetyhook::create_mid(MissionSelect2ScanResult + 0x14,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] += ((1920.00f / fAspectRatio) - 1080.00f) / 2.00f;
                });
        }
        else if (!MissionSelect1ScanResult || !MissionSelect2ScanResult) {
            spdlog::error("HUD: Mission Select: Pattern scan(s) failed.");
        }
    }
}

void Framerate()
{
    if (iFramerateCap != 60) {
        // Framerate Cap
        uint8_t* FramerateCapScanResult = Memory::PatternScan(baseModule, "B8 3C 00 00 00 83 ?? 02 0F ?? ?? 8D ?? ?? 85 ?? 74 ?? 85 ??");
        if (FramerateCapScanResult)
        {
            spdlog::info("Framerate Cap: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FramerateCapScanResult - (uintptr_t)baseModule);
            Memory::Write((uintptr_t)FramerateCapScanResult + 0x1, iFramerateCap);
            spdlog::info("Framerate Cap: Patched instruction.");
        }
        else if (!FramerateCapScanResult)
        {
            spdlog::error("Framerate Cap: Pattern scan failed.");
        }
    }
}

void Misc()
{
    if (iShadowResolution != 0)
    {
        // Shadow Quality
        // Changes "high" quality shadow resolution
        uint8_t* ShadowQuality1ScanResult = Memory::PatternScan(baseModule, "00 10 00 00 00 10 00 00 4E 00 00 00 00 04 00 00");
        uint8_t* ShadowQuality2ScanResult = Memory::PatternScan(baseModule, "BA 00 10 00 00 44 ?? ?? EB ?? BA 00 08 00 00");
        if (ShadowQuality1ScanResult && ShadowQuality2ScanResult)
        {
            spdlog::info("Shadow Quality: Address 1 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowQuality1ScanResult - (uintptr_t)baseModule);
            spdlog::info("Shadow Quality: Address 2 is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowQuality2ScanResult - (uintptr_t)baseModule);

            Memory::Write((uintptr_t)ShadowQuality1ScanResult, iShadowResolution);
            Memory::Write((uintptr_t)ShadowQuality1ScanResult + 0x4, iShadowResolution);
            Memory::Write((uintptr_t)ShadowQuality2ScanResult + 0x1, iShadowResolution);
        }
        else if (!ShadowQuality1ScanResult || !ShadowQuality2ScanResult)
        {
            spdlog::error("Shadow Quality: Pattern scan failed.");
        }
    }

    if (bRenderTextureRes) {
        // Render Textures
        uint8_t* RenderTextures1ScanResult = Memory::PatternScan(baseModule, "45 ?? ?? 44 ?? ?? ?? 41 0F ?? ?? 45 ?? ?? 75 ?? 44 ?? ?? ?? ?? ?? ?? EB ??");
        uint8_t* RenderTextures2ScanResult = Memory::PatternScan(baseModule, "45 ?? ?? 44 ?? ?? ?? ?? 4C ?? ?? ?? 49 ?? ?? 44 ?? ?? ?? ??");
        if (RenderTextures1ScanResult && RenderTextures2ScanResult) {
            // Set 1920x1080 render textures to native resolution
            spdlog::info("HUD: Render Textures: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)RenderTextures1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid RenderTextures1MidHook{};
            RenderTextures1MidHook = safetyhook::create_mid(RenderTextures1ScanResult,
                [](SafetyHookContext& ctx) {
                    if ((int)ctx.r10 == 1920 && (int)ctx.r11 == 1080) {
                        ctx.r10 = iCurrentResX;
                        ctx.r11 = iCurrentResY;
                    }
                });

            spdlog::info("HUD: Render Textures: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)RenderTextures2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid RenderTextures2MidHook{};
            RenderTextures2MidHook = safetyhook::create_mid(RenderTextures2ScanResult,
                [](SafetyHookContext& ctx) {
                    if ((int)ctx.r13 == 1920 && ctx.r12 == 1080) {
                        ctx.r13 = iCurrentResX;
                        ctx.rdx = iCurrentResX;
                        ctx.r12 = iCurrentResY;
                    }
                });
        }
        else if (!RenderTextures1ScanResult || !RenderTextures2ScanResult) {
            spdlog::error("HUD: Render Textures: Pattern scan(s) failed.");
        }
    }    
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    SkipIntro();
    Resolution();
    AspectFOV();
    HUD();
    Framerate();
    Misc();
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        thisModule = hModule;
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, CREATE_SUSPENDED, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_TIME_CRITICAL);
            ResumeThread(mainHandle);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}