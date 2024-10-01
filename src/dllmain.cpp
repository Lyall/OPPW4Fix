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
std::string sFixVer = "0.0.1";
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
float fFramerateCap;
float fGameplayFOVMulti;
int iShadowResolution;

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

    inipp::get_value(ini.sections["Gameplay FOV"], "Multiplier", fGameplayFOVMulti);
    if ((float)fGameplayFOVMulti < 0.10f || (float)fGameplayFOVMulti > 3.00f) {
        fGameplayFOVMulti = std::clamp((float)fGameplayFOVMulti, 0.10f, 3.00f);
        spdlog::warn("Config Parse: fGameplayFOVMulti value invalid, clamped to {}", fGameplayFOVMulti);
    }
    spdlog::info("Config Parse: fGameplayFOVMulti: {}", fGameplayFOVMulti);

    inipp::get_value(ini.sections["Framerate Cap"], "Framerate", fFramerateCap);
    if ((float)fFramerateCap < 10.00f || (float)fFramerateCap > 500.00f) {
        fFramerateCap = std::clamp((float)fFramerateCap, 10.00f, 500.00f);
        spdlog::warn("Config Parse: fFramerateCap value invalid, clamped to {}", fFramerateCap);
    }
    spdlog::info("Config Parse: fFramerateCap: {}", fFramerateCap);

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

    if (bCustomRes) {
        // Add custom resolution
        uint8_t* ResolutionList1ScanResult = Memory::PatternScan(baseModule, "00 05 00 00 D0 02 00 00 56 05 00 00");
        uint8_t* ResolutionList2ScanResult = Memory::PatternScan(baseModule, "00 05 D0 02 56 05 00 03 40 06");
        if (ResolutionList1ScanResult && ResolutionList2ScanResult) {
            spdlog::info("Custom Resolution: List 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionList1ScanResult - (uintptr_t)baseModule);
            spdlog::info("Custom Resolution: List 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionList2ScanResult - (uintptr_t)baseModule);

            // Replace 1280x720 with new resolution
            Memory::Write((uintptr_t)ResolutionList1ScanResult, iCustomResX);
            Memory::Write((uintptr_t)ResolutionList1ScanResult + 0x4, iCustomResY);
            Memory::Write((uintptr_t)ResolutionList2ScanResult, (short)iCustomResX);
            Memory::Write((uintptr_t)ResolutionList2ScanResult + 0x2, (short)iCustomResY);
            spdlog::info("Custom Resolution: List: Replaced {}x{} with {}x{}", 1280, 720, iCustomResX, iCustomResY);
        }
        else if (!ResolutionList1ScanResult || !ResolutionList2ScanResult) {
            spdlog::error("Custom Resolution: Pattern scan(s) failed.");
        }

        // Spoof GetSystemMetrics results so our custom resolution is always valid
        uint8_t* SystemMetricsScanResult = Memory::PatternScan(baseModule, "89 ?? ?? ?? ?? ?? FF ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? 48 89 ?? ?? ?? ?? ?? ?? ?? ?? 48 89 ?? ?? 89 ?? ??");
        uint8_t* ResCheckScanResult = Memory::PatternScan(baseModule, "74 ?? 3B ?? ?? 77 ?? 3B ?? 0F ?? ?? ?? ?? ?? FF ?? 48 ?? ?? ??");
        if (SystemMetricsScanResult) {
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
            Memory::PatchBytes((uintptr_t)ResCheckScanResult, "\x75", 1);
            spdlog::info("Custom Resolution: GetSystemMetrics: ResCheck: Patched instruction.");
        }
        else if (!SystemMetricsScanResult ) {
            spdlog::error("Custom Resolution: GetSystemMetrics: Pattern scan failed.");
        }
    }
}

void AspectFOV()
{
    if (bFixAspect) {
        
    }

    if (bFixFOV) {
       
    }

    if (fGameplayFOVMulti != 1.00f) {
        
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

        // Key Guide
        uint8_t* KeyGuideScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? E8 ?? ?? ?? ??");
        if (KeyGuideScanResult) {
            spdlog::info("HUD: Key Guide: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)KeyGuideScanResult - (uintptr_t)baseModule);
            static SafetyHookMid KeyGuideMidHook{};
            KeyGuideMidHook = safetyhook::create_mid(KeyGuideScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = fHUDWidth;
                });
        }
        else if (!KeyGuideScanResult) {
            spdlog::error("HUD: Key Guide: Pattern scan failed.");
        }
    }   
}

void Framerate()
{
    if (fFramerateCap != 60.00f) {
        
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    Resolution();
    AspectFOV();
    HUD();
    Framerate();
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
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
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