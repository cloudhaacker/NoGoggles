// NoGoggles - a pretend VR headset for SteamVR.
//
// SteamVR loads this driver and sees a headset. Instead of sending pictures to
// lenses, the headset is a normal window on your monitor, showing one clean,
// un-warped view. Where the "head" looks is steered with the mouse, the keyboard
// or a gamepad stick.
//
// Built without the Visual C++ runtime on purpose (see build.sh), so it only
// needs kernel32 and user32, which every Windows PC has.

#include "openvr_driver.h"
#include "winmini.h"

// ---------------------------------------------------------------------------
// Tiny runtime pieces we provide ourselves (no CRT)
// ---------------------------------------------------------------------------
extern "C" {
int _fltused = 0;
int __cdecl _purecall() { return 0; }

__attribute__((no_builtin)) void* __cdecl memset(void* d, int c, size_t n)
{
    volatile unsigned char* p = (volatile unsigned char*)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}
__attribute__((no_builtin)) void* __cdecl memcpy(void* d, const void* s, size_t n)
{
    volatile unsigned char* o = (volatile unsigned char*)d;
    const unsigned char* i = (const unsigned char*)s;
    while (n--) *o++ = *i++;
    return d;
}
__attribute__((no_builtin)) int __cdecl strcmp(const char* a, const char* b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}
int __stdcall _DllMainCRTStartup(void*, unsigned long, void*) { return 1; }
}

inline void* operator new(size_t, void* p) noexcept { return p; }

static bool StrIEq(const char* a, const char* b)
{
    for (;; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z') x -= 32;
        if (y >= 'a' && y <= 'z') y -= 32;
        if (x != y) return false;
        if (!x) return true;
    }
}

// ---------------------------------------------------------------------------
// Math
// ---------------------------------------------------------------------------
// Our own "play space" ID. The installer writes a matching room setup so
// SteamVR doesn't treat the headset as uncalibrated.
static const uint64_t kUniverseId = 4206942069ULL;

static const double kPi = 3.14159265358979323846;

static double Sin(double x)
{
    // wrap to [-pi, pi]
    while (x > kPi) x -= 2 * kPi;
    while (x < -kPi) x += 2 * kPi;
    // fold to [-pi/2, pi/2]
    if (x > kPi / 2) x = kPi - x;
    if (x < -kPi / 2) x = -kPi - x;
    double x2 = x * x, term = x, sum = x;
    for (int i = 1; i < 10; ++i) {
        term *= -x2 / ((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sum;
}
static double Cos(double x) { return Sin(x + kPi / 2); }

// tan for 0 < x < pi/2
static double Tan(double x) { return Sin(x) / Cos(x); }

static double Clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Small text helpers for logging and the settings file
// ---------------------------------------------------------------------------
struct Text {
    char buf[512];
    int len = 0;
    Text() { buf[0] = 0; }
    Text& s(const char* t)
    {
        while (*t && len < (int)sizeof(buf) - 1) buf[len++] = *t++;
        buf[len] = 0;
        return *this;
    }
    Text& i(long long v)
    {
        char tmp[24]; int n = 0; bool neg = v < 0;
        unsigned long long u = neg ? (unsigned long long)(-v) : (unsigned long long)v;
        do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
        if (neg) tmp[n++] = '-';
        while (n && len < (int)sizeof(buf) - 1) buf[len++] = tmp[--n];
        buf[len] = 0;
        return *this;
    }
    Text& f(double v)
    {
        if (v < 0) { s("-"); v = -v; }
        long long whole = (long long)v;
        i(whole); s(".");
        long long frac = (long long)((v - (double)whole) * 1000 + 0.5);
        if (frac >= 1000) frac = 999;
        if (frac < 100) s("0");
        if (frac < 10) s("0");
        return i(frac);
    }
};

static void Log(const Text& t)
{
    if (vr::VRDriverLog()) vr::VRDriverLog()->Log(t.buf);
}

static double ParseNumber(const char* p, double fallback)
{
    while (*p == ' ' || *p == '\t') ++p;
    bool neg = false;
    if (*p == '-') { neg = true; ++p; } else if (*p == '+') ++p;
    if (!((*p >= '0' && *p <= '9') || *p == '.')) return fallback;
    double v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    if (*p == '.') {
        ++p; double scale = 0.1;
        while (*p >= '0' && *p <= '9') { v += (*p++ - '0') * scale; scale *= 0.1; }
    }
    return neg ? -v : v;
}

static bool ParseBool(const char* p, bool fallback)
{
    while (*p == ' ' || *p == '\t') ++p;
    char c = *p;
    if (c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T') return true;
    if (c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F') return false;
    if ((c == 'o' || c == 'O') && (p[1] == 'n' || p[1] == 'N')) return true;
    if ((c == 'o' || c == 'O') && (p[1] == 'f' || p[1] == 'F')) return false;
    return fallback;
}

// Key names people can type in the settings file -> Windows virtual key codes.
struct KeyName { const char* name; int vk; };
static const KeyName kKeys[] = {
    {"none", 0},
    {"F1", 0x70}, {"F2", 0x71}, {"F3", 0x72}, {"F4", 0x73}, {"F5", 0x74}, {"F6", 0x75},
    {"F7", 0x76}, {"F8", 0x77}, {"F9", 0x78}, {"F10", 0x79}, {"F11", 0x7A}, {"F12", 0x7B},
    {"Home", 0x24}, {"End", 0x23}, {"Insert", 0x2D}, {"Delete", 0x2E},
    {"PageUp", 0x21}, {"PageDown", 0x22}, {"ScrollLock", 0x91}, {"Pause", 0x13},
    {"Up", 0x26}, {"Down", 0x28}, {"Left", 0x25}, {"Right", 0x27},
    {"Numpad8", 0x68}, {"Numpad2", 0x62}, {"Numpad4", 0x64}, {"Numpad6", 0x66}, {"Numpad5", 0x65},
    {"Space", 0x20}, {"Tab", 0x09}, {"Shift", 0x10}, {"Ctrl", 0x11}, {"Alt", 0x12},
    {"RightMouse", 0x02}, {"MiddleMouse", 0x04}, {"Mouse4", 0x05}, {"Mouse5", 0x06},
};

static int ParseKey(const char* p, int fallback)
{
    while (*p == ' ' || *p == '\t') ++p;
    for (const KeyName& k : kKeys)
        if (StrIEq(p, k.name)) return k.vk;
    // a single letter or digit
    if (p[0] && !p[1]) {
        char c = p[0];
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
    }
    return fallback;
}

// Gamepad button names -> XInput button bits (triggers handled separately).
static const int kPadLT = 0x10000, kPadRT = 0x20000;
struct PadName { const char* name; int bits; };
static const PadName kPadButtons[] = {
    {"none", 0}, {"A", 0x1000}, {"B", 0x2000}, {"X", 0x4000}, {"Y", 0x8000},
    {"LB", 0x0100}, {"RB", 0x0200}, {"LT", kPadLT}, {"RT", kPadRT},
    {"LS", 0x0040}, {"RS", 0x0080}, {"L3", 0x0040}, {"R3", 0x0080},
    {"Back", 0x0020}, {"Select", 0x0020}, {"Start", 0x0010},
    {"DUp", 0x0001}, {"DDown", 0x0002}, {"DLeft", 0x0004}, {"DRight", 0x0008},
};

static int ParseOnePad(const char* p, int fallback)
{
    while (*p == ' ' || *p == '\t') ++p;
    for (const PadName& b : kPadButtons)
        if (StrIEq(p, b.name)) return b.bits;
    return fallback;
}

// "RS" or a combo like "Back+Start"
static int ParsePad(const char* p, int fallback)
{
    int bits = 0;
    char part[32];
    while (*p) {
        int n = 0;
        while (*p && *p != '+' && n < 31) { if (*p != ' ') part[n++] = *p; ++p; }
        part[n] = 0;
        if (*p == '+') ++p;
        if (n) {
            int b = ParseOnePad(part, -1);
            if (b < 0) return fallback;
            bits |= b;
        }
    }
    return bits;
}

// ---------------------------------------------------------------------------
// Settings (NoGoggles.ini, next to driver.vrdrivermanifest)
// ---------------------------------------------------------------------------
struct Settings {
    // Display - read once when SteamVR starts
    int windowX = 100, windowY = 100, windowW = 1600, windowH = 900;
    bool fullscreen = false;
    double fovDegrees = 90.0;       // horizontal field of view
    double renderScale = 1.0;       // 1.0 = render at window size
    double refreshRate = 60.0;

    // Looking around - re-read every second, so you can tweak while playing
    int mouseToggleKey = 0x77;      // F8
    double mouseSensitivity = 0.15; // degrees per mouse pixel
    bool invertY = false;
    int stick = 2;                  // 0 = off, 1 = left stick, 2 = right stick
    double stickSpeed = 120.0;      // degrees per second at full tilt
    double stickDeadzone = 0.20;
    int stickHoldButton = 0;        // only look with the stick while this is held
    bool keyboardLook = true;       // arrow keys / numpad 8-4-6-2
    double keyboardSpeed = 90.0;
    int recenterKey = 0x78;         // F9
    int recenterPad = 0x0080;       // R3 (click the right stick)
    double maxPitch = 80.0;
    double headHeight = 0.7;        // metres above the floor (0.7 tested good in Rhombus of Ruin)
    bool predictTurning = true;     // send turning speed so SteamVR can predict
    int quitKey = 0x79;             // F10, hold for a second to close SteamVR

    // Crosshair - re-read every second
    bool crosshair = true;
    int crosshairSize = 10;         // pixels
    double crosshairOpacity = 0.75; // 0.1 - 1.0
    int crosshairToggleKey = 0x76;  // F7
    int crosshairTogglePad = 0x0040; // L3 (click the left stick)
    int quitPad = 0x0030;           // Back+Start, held to close SteamVR
    bool messages = true;           // little on-screen notes ("Mouse look ON")
    bool keepOnTop = true;          // NoGoggles window stays above the game's own window
    bool autoFocusGame = true;      // keep the game "active" so the gamepad reaches it
    double smoothing = 0.0;         // 0 = none, 0.9 = very smooth
};

static char g_iniPath[600];

static void IniGet(const char* section, const char* key, char* out, unsigned long size)
{
    out[0] = 0;
    GetPrivateProfileStringA(section, key, "", out, size, g_iniPath);
    // Strip trailing comments and spaces: "90 ; comment"
    for (char* c = out; *c; ++c)
        if (*c == ';' || *c == '#') { *c = 0; break; }
    int n = 0; while (out[n]) ++n;
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) out[--n] = 0;
}

static double IniNum(const char* s, const char* k, double d) { char b[64]; IniGet(s, k, b, sizeof b); return b[0] ? ParseNumber(b, d) : d; }
static bool IniBool(const char* s, const char* k, bool d) { char b[64]; IniGet(s, k, b, sizeof b); return b[0] ? ParseBool(b, d) : d; }
static int IniKey(const char* s, const char* k, int d) { char b[64]; IniGet(s, k, b, sizeof b); return b[0] ? ParseKey(b, d) : d; }
static int IniPad(const char* s, const char* k, int d) { char b[64]; IniGet(s, k, b, sizeof b); return b[0] ? ParsePad(b, d) : d; }

static void LoadDisplaySettings(Settings& s)
{
    s.fullscreen = IniBool("Display", "Fullscreen", s.fullscreen);
    s.windowX = (int)IniNum("Display", "WindowX", s.windowX);
    s.windowY = (int)IniNum("Display", "WindowY", s.windowY);
    s.windowW = (int)IniNum("Display", "WindowWidth", s.windowW);
    s.windowH = (int)IniNum("Display", "WindowHeight", s.windowH);
    s.fovDegrees = Clamp(IniNum("Display", "FieldOfView", s.fovDegrees), 30.0, 150.0);
    s.renderScale = Clamp(IniNum("Display", "RenderScale", s.renderScale), 0.25, 3.0);
    s.refreshRate = Clamp(IniNum("Display", "RefreshRate", s.refreshRate), 30.0, 240.0);

    if (s.fullscreen) {
        s.windowX = 0; s.windowY = 0;
        s.windowW = GetSystemMetrics(SM_CXSCREEN);
        s.windowH = GetSystemMetrics(SM_CYSCREEN);
    }
    if (s.windowW < 320) s.windowW = 320;
    if (s.windowH < 180) s.windowH = 180;
}

static void LoadLookSection(Settings& s, const char* look, const char* cross)
{
    s.mouseToggleKey = IniKey(look, "MouseLookToggleKey", s.mouseToggleKey);
    s.mouseSensitivity = Clamp(IniNum(look, "MouseSensitivity", s.mouseSensitivity), 0.005, 5.0);
    s.invertY = IniBool(look, "InvertY", s.invertY);

    char b[64];
    IniGet(look, "GamepadStick", b, sizeof b);
    if (b[0]) {
        if (StrIEq(b, "left")) s.stick = 1;
        else if (StrIEq(b, "right")) s.stick = 2;
        else if (StrIEq(b, "off") || StrIEq(b, "none")) s.stick = 0;
    }
    s.stickSpeed = Clamp(IniNum(look, "GamepadSpeed", s.stickSpeed), 5.0, 720.0);
    s.stickDeadzone = Clamp(IniNum(look, "GamepadDeadzone", s.stickDeadzone), 0.0, 0.9);
    s.stickHoldButton = IniPad(look, "GamepadHoldButton", s.stickHoldButton);
    s.keyboardLook = IniBool(look, "KeyboardLook", s.keyboardLook);
    s.keyboardSpeed = Clamp(IniNum(look, "KeyboardSpeed", s.keyboardSpeed), 5.0, 720.0);
    s.recenterKey = IniKey(look, "RecenterKey", s.recenterKey);
    s.recenterPad = IniPad(look, "RecenterGamepadButton", s.recenterPad);
    s.maxPitch = Clamp(IniNum(look, "MaxLookUpDown", s.maxPitch), 10.0, 89.0);
    s.headHeight = Clamp(IniNum(look, "HeadHeight", s.headHeight), -3.0, 3.0);
    s.smoothing = Clamp(IniNum(look, "Smoothing", s.smoothing), 0.0, 0.98);
    s.quitKey = IniKey(look, "QuitKey", s.quitKey);
    s.crosshairToggleKey = IniKey(cross, "ToggleKey", s.crosshairToggleKey);
    s.crosshairTogglePad = IniPad(cross, "ToggleGamepadButton", s.crosshairTogglePad);
    s.quitPad = IniPad(look, "QuitGamepadButtons", s.quitPad);
    s.messages = IniBool(look, "OnScreenMessages", s.messages);
    s.predictTurning = IniBool(look, "PredictTurning", s.predictTurning);
    s.crosshair = IniBool(cross, "Show", s.crosshair);
    s.crosshairSize = (int)Clamp(IniNum(cross, "Size", s.crosshairSize), 3, 64);
    s.crosshairOpacity = Clamp(IniNum(cross, "Opacity", s.crosshairOpacity), 0.1, 1.0);
}

// ---------------------------------------------------------------------------
// On-screen messages. Any thread can post one; the overlay thread shows it.
// ---------------------------------------------------------------------------
static char g_toastText[160];
static volatile int g_toastSeq = 0;
static volatile bool g_crosshairHidden = false;

static void Toast(const char* a, const char* b = nullptr)
{
    int n = 0;
    for (const char* p = a; *p && n < 150; ++p) g_toastText[n++] = *p;
    if (b) for (const char* p = b; *p && n < 158; ++p) g_toastText[n++] = *p;
    g_toastText[n] = 0;
    g_toastSeq = g_toastSeq + 1;
}

// ---------------------------------------------------------------------------
// Per-game settings. A section like
//     [Game:Eagle Flight]
//     Exe = EagleFlight.exe
//     GamepadSpeed = 200
// overrides [Look] / [Crosshair] values while that program is running.
// ---------------------------------------------------------------------------
static char g_profileSection[128];   // "Game:Eagle Flight", or "" for none

static bool ExeInList(const char* exe, const char* list)
{
    // list is "a.exe, b.exe"
    const char* p = list;
    while (*p) {
        while (*p == ' ' || *p == ',') ++p;
        const char* start = p;
        while (*p && *p != ',') ++p;
        const char* end = p;
        while (end > start && end[-1] == ' ') --end;
        int len = (int)(end - start);
        if (len > 0) {
            int i = 0;
            for (; i < len && exe[i]; ++i) {
                char x = exe[i], y = start[i];
                if (x >= 'a' && x <= 'z') x -= 32;
                if (y >= 'a' && y <= 'z') y -= 32;
                if (x != y) break;
            }
            if (i == len && exe[i] == 0) return true;
        }
    }
    return false;
}

// Look through running programs and pick the first [Game:...] section that matches.
static void FindRunningProfile(char* out, int outSize)
{
    out[0] = 0;
    static char names[8192];
    unsigned long got = GetPrivateProfileSectionNamesA(names, sizeof names, g_iniPath);
    if (!got) return;

    struct Prof { const char* section; char exes[256]; };
    static Prof profs[32];
    int count = 0;
    for (const char* p = names; *p && count < 32; ) {
        int len = 0; while (p[len]) ++len;
        if ((p[0] == 'G' || p[0] == 'g') && (p[1] == 'a' || p[1] == 'A') && (p[2] == 'm' || p[2] == 'M') &&
            (p[3] == 'e' || p[3] == 'E') && p[4] == ':') {
            profs[count].section = p;
            IniGet(p, "Exe", profs[count].exes, sizeof profs[count].exes);
            if (profs[count].exes[0]) ++count;
        }
        p += len + 1;
    }
    if (!count) return;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    int best = count;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof pe;
    if (Process32First(snap, &pe)) {
        do {
            for (int i = 0; i < best; ++i)
                if (ExeInList(pe.szExeFile, profs[i].exes)) { best = i; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);

    if (best < count) {
        int i = 0;
        for (const char* q = profs[best].section; *q && i < outSize - 1; ++q) out[i++] = *q;
        out[i] = 0;
    }
}

// Re-read [Look]/[Crosshair], then the running game's section on top.
// Starts from fresh defaults so values from a game's section don't linger.
static void ReloadLookSettings(Settings& cfg)
{
    Settings fresh;
    LoadLookSection(fresh, "Look", "Crosshair");
    fresh.keepOnTop = IniBool("Windows", "KeepNoGogglesOnTop", fresh.keepOnTop);
    fresh.autoFocusGame = IniBool("Windows", "AutoFocusGame", fresh.autoFocusGame);
    if (g_profileSection[0]) LoadLookSection(fresh, g_profileSection, g_profileSection);
    // keep the display part, which only applies at startup
    fresh.windowX = cfg.windowX; fresh.windowY = cfg.windowY;
    fresh.windowW = cfg.windowW; fresh.windowH = cfg.windowH;
    fresh.fullscreen = cfg.fullscreen; fresh.fovDegrees = cfg.fovDegrees;
    fresh.renderScale = cfg.renderScale; fresh.refreshRate = cfg.refreshRate;
    cfg = fresh;
}

static void CheckProfile(Settings& cfg)
{
    char now[128];
    FindRunningProfile(now, sizeof now);
    if (strcmp(now, g_profileSection) != 0) {
        int i = 0; for (; now[i]; ++i) g_profileSection[i] = now[i]; g_profileSection[i] = 0;
        ReloadLookSettings(cfg);
        if (now[0]) {
            Log(Text().s("[NoGoggles] Using settings from [").s(now).s("]"));
            if (cfg.messages) Toast("Settings: ", now + 5);
        } else {
            Log(Text().s("[NoGoggles] Back to default settings"));
            if (cfg.messages) Toast("Default settings");
        }
    }
}

// ---------------------------------------------------------------------------
// The display: a plain window, both eyes drawn to the same full-size spot
// ---------------------------------------------------------------------------
class NoGogglesDisplay : public vr::IVRDisplayComponent {
public:
    Settings* cfg = nullptr;

    void GetWindowBounds(int32_t* x, int32_t* y, uint32_t* w, uint32_t* h) override
    {
        *x = cfg->windowX; *y = cfg->windowY;
        *w = (uint32_t)cfg->windowW; *h = (uint32_t)cfg->windowH;
    }
    bool IsDisplayOnDesktop() override { return true; }
    bool IsDisplayRealDisplay() override { return false; }
    void GetRecommendedRenderTargetSize(uint32_t* w, uint32_t* h) override
    {
        *w = (uint32_t)(cfg->windowW * cfg->renderScale);
        *h = (uint32_t)(cfg->windowH * cfg->renderScale);
    }
    void GetEyeOutputViewport(vr::EVREye, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) override
    {
        // Both eyes fill the whole window. Eye separation is set to zero, so the
        // two pictures are identical and you just see one normal view.
        *x = 0; *y = 0;
        *w = (uint32_t)cfg->windowW; *h = (uint32_t)cfg->windowH;
    }
    void GetProjectionRaw(vr::EVREye, float* left, float* right, float* top, float* bottom) override
    {
        double halfW = Tan(cfg->fovDegrees * kPi / 360.0);
        double halfH = halfW * (double)cfg->windowH / (double)cfg->windowW;
        *left = (float)-halfW; *right = (float)halfW;
        *top = (float)-halfH; *bottom = (float)halfH;
    }
    vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye, float u, float v) override
    {
        vr::DistortionCoordinates_t c;
        c.rfRed[0] = u; c.rfRed[1] = v;
        c.rfGreen[0] = u; c.rfGreen[1] = v;
        c.rfBlue[0] = u; c.rfBlue[1] = v;
        return c;
    }
    bool ComputeInverseDistortion(vr::HmdVector2_t* result, vr::EVREye, uint32_t, float u, float v) override
    {
        result->v[0] = u; result->v[1] = v;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Gamepad (XInput, loaded at runtime; reading it does not take it from the game)
// ---------------------------------------------------------------------------
struct XInputGamepad { unsigned short buttons; unsigned char lt, rt; short lx, ly, rx, ry; };
struct XInputState { unsigned long packet; XInputGamepad pad; };
typedef unsigned long(__stdcall* XInputGetStateFn)(unsigned long, XInputState*);
static XInputGetStateFn g_xinputGetState = nullptr;

static void LoadXInput()
{
    const char* dlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (const char* d : dlls) {
        HMODULE m = LoadLibraryA(d);
        if (m) {
            g_xinputGetState = (XInputGetStateFn)GetProcAddress(m, "XInputGetState");
            if (g_xinputGetState) return;
        }
    }
}

static bool ReadPad(XInputGamepad& out)
{
    if (!g_xinputGetState) return false;
    for (unsigned long i = 0; i < 4; ++i) {
        XInputState st;
        if (g_xinputGetState(i, &st) == 0) { out = st.pad; return true; }
    }
    return false;
}

// every button in the combo is down
static bool PadAll(const XInputGamepad& p, int bits)
{
    if (!bits) return false;
    if ((bits & kPadLT) && p.lt <= 100) return false;
    if ((bits & kPadRT) && p.rt <= 100) return false;
    int b = bits & 0xFFFF;
    return (p.buttons & b) == b;
}

static bool PadHeld(const XInputGamepad& p, int bits)
{
    if (!bits) return false;
    if ((bits & kPadLT) && p.lt > 100) return true;
    if ((bits & kPadRT) && p.rt > 100) return true;
    return (p.buttons & (bits & 0xFFFF)) != 0;
}

static double StickAxis(short raw, double deadzone)
{
    double v = raw / 32767.0;
    if (v > 1) v = 1; if (v < -1) v = -1;
    double a = v < 0 ? -v : v;
    if (a < deadzone) return 0;
    a = (a - deadzone) / (1 - deadzone);
    a = a * a; // gentle in the middle, fast at the edge
    return v < 0 ? -a : a;
}

static bool KeyDown(int vk) { return vk && (GetAsyncKeyState(vk) & 0x8000) != 0; }

// ---------------------------------------------------------------------------
// Crosshair: a small click-through dot floating over the middle of the view.
// The game aims wherever your head points, which is always the exact centre.
// ---------------------------------------------------------------------------
static HMODULE g_module = nullptr;
static volatile int g_crossSize = 10;

static intptr_t __stdcall CrosshairProc(HWND h, unsigned int msg, uintptr_t w, intptr_t l)
{
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        int n = g_crossSize;
        int ring = n >= 8 ? 2 : 1;
        SelectObject(dc, GetStockObject(NULL_PEN));
        SelectObject(dc, GetStockObject(BLACK_BRUSH));
        Ellipse(dc, 0, 0, n + 1, n + 1);
        SelectObject(dc, GetStockObject(WHITE_BRUSH));
        Ellipse(dc, ring, ring, n + 1 - ring, n + 1 - ring);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcA(h, msg, w, l);
}

// ---------------------------------------------------------------------------
// Window juggling.
// Games only read the gamepad while their OWN window is the active one, but
// the picture you want to see is in the SteamVR (NoGoggles) window. So:
//   - the NoGoggles window is kept "always on top", so it stays visible, and
//   - the game's window is kept active behind it, so the gamepad works.
// We only take focus back from SteamVR's own windows (or at game start),
// never from other programs you switch to.
// ---------------------------------------------------------------------------
static bool NameIs(const char* path, const char* name)
{
    const char* base = path;
    for (const char* p = path; *p; ++p) if (*p == '\\' || *p == '/') base = p + 1;
    return StrIEq(base, name);
}

static bool ProcessName(unsigned long pid, char* out, unsigned long size)
{
    out[0] = 0;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
    if (!h) return false;
    unsigned long n = size;
    BOOL ok = QueryFullProcessImageNameA(h, 0, out, &n);
    CloseHandle(h);
    return ok != 0;
}

static bool IsSteamVRProcess(const char* path)
{
    const char* base = path;
    for (const char* p = path; *p; ++p) if (*p == '\\' || *p == '/') base = p + 1;
    char c0 = base[0], c1 = base[1];
    if ((c0 == 'v' || c0 == 'V') && (c1 == 'r' || c1 == 'R')) return true; // vrcompositor, vrmonitor, vrdashboard, vrwebhelper...
    return NameIs(path, "steam.exe") || NameIs(path, "steamwebhelper.exe");
}

// Does this process have SteamVR's game library loaded? That's how we spot VR games.
static bool UsesOpenVR(unsigned long pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid);
    if (!h) return false;
    static HMODULE mods[1024];
    unsigned long needed = 0;
    bool found = false;
    if (K32EnumProcessModules(h, mods, sizeof mods, &needed)) {
        unsigned long count = needed / sizeof(HMODULE);
        if (count > 1024) count = 1024;
        char name[128];
        for (unsigned long i = 0; i < count && !found; ++i)
            if (K32GetModuleBaseNameA(h, mods[i], name, sizeof name) && StrIEq(name, "openvr_api.dll")) found = true;
    }
    CloseHandle(h);
    return found;
}

struct PidInfo { unsigned long pid; signed char kind; }; // kind: 0 other, 1 steamvr, 2 vr game
static PidInfo g_pidCache[64];
static int g_pidNext = 0;

static int PidKind(unsigned long pid)
{
    for (const PidInfo& p : g_pidCache) if (p.pid == pid && pid) return p.kind;
    char path[600];
    int kind = 0;
    if (ProcessName(pid, path, sizeof path)) {
        if (IsSteamVRProcess(path)) kind = 1;
        else if (UsesOpenVR(pid)) kind = 2;
    }
    g_pidCache[g_pidNext].pid = pid;
    g_pidCache[g_pidNext].kind = (signed char)kind;
    g_pidNext = (g_pidNext + 1) % 64;
    return kind;
}

static unsigned long g_ourPid = 0;

struct WindowScan {
    HWND compositor = nullptr; long compositorArea = 0;
    HWND game = nullptr; long gameArea = 0;
};

static BOOL __stdcall ScanWindow(HWND h, intptr_t param)
{
    WindowScan* scan = (WindowScan*)param;
    if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return 1;
    if (GetWindowLongPtrA(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return 1;
    unsigned long pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (!pid || pid == g_ourPid) return 1;
    RECT r;
    if (!GetWindowRect(h, &r)) return 1;
    long area = (r.right - r.left) * (r.bottom - r.top);
    if (area < 200 * 200) return 1;
    int kind = PidKind(pid);
    if (kind == 1) {
        char path[600];
        if (ProcessName(pid, path, sizeof path) && NameIs(path, "vrcompositor.exe") && area > scan->compositorArea) {
            scan->compositor = h; scan->compositorArea = area;
        }
    } else if (kind == 2 && area > scan->gameArea) {
        scan->game = h; scan->gameArea = area;
    }
    return 1;
}

static void FocusWindow(HWND target)
{
    HWND fg = GetForegroundWindow();
    unsigned long fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    unsigned long me = GetCurrentThreadId();
    if (fgThread && fgThread != me) AttachThreadInput(me, fgThread, 1);
    SetForegroundWindow(target);
    if (fgThread && fgThread != me) AttachThreadInput(me, fgThread, 0);
    if (GetForegroundWindow() != target) {
        // Windows sometimes refuses; a tap of Alt unlocks it.
        keybd_event(0x12, 0, 0, 0);
        keybd_event(0x12, 0, KEYEVENTF_KEYUP, 0);
        SetForegroundWindow(target);
    }
}

// Is window `w` underneath `other` in the stacking order?
static bool IsBelow(HWND w, HWND other)
{
    if (!w || !other) return false;
    HWND p = w;
    for (int i = 0; i < 400; ++i) {
        p = GetWindow(p, GW_HWNDPREV);
        if (!p) return false;
        if (p == other) return true;
    }
    return false;
}

struct WindowJuggler {
    HWND compositor = nullptr;
    HWND lastGame = nullptr;
    HWND topmostDone = nullptr;

    void Tick(const Settings& cfg)
    {
        if (!g_ourPid) g_ourPid = GetCurrentProcessId();
        WindowScan scan;
        EnumWindows(&ScanWindow, (intptr_t)&scan);

        compositor = scan.compositor;
        if (scan.compositor && cfg.keepOnTop) {
            // Only when it isn't already "always on top" - re-applying it all the
            // time fights other popups (Discord etc.) and makes them flicker.
            if (!(GetWindowLongPtrA(scan.compositor, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
                SetWindowPos(scan.compositor, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                Log(Text().s("[NoGoggles] NoGoggles window set to stay on top"));
            }
            topmostDone = scan.compositor;
        } else if (scan.compositor && !cfg.keepOnTop && topmostDone) {
            SetWindowPos(scan.compositor, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            topmostDone = nullptr;
        }

        if (!cfg.autoFocusGame || !scan.game) { lastGame = scan.game; return; }

        HWND fg = GetForegroundWindow();
        bool newGame = scan.game != lastGame;
        lastGame = scan.game;
        if (fg == scan.game) return;

        bool stolenBySteamVR = false;
        if (fg) {
            unsigned long pid = 0;
            GetWindowThreadProcessId(fg, &pid);
            stolenBySteamVR = PidKind(pid) == 1;
        }
        if (newGame || stolenBySteamVR) {
            FocusWindow(scan.game);
            if (newGame) Log(Text().s("[NoGoggles] Found the game window and made it active"));
        }
    }
};

// The message box: dark rounded panel with white text, near the top of the view.
static int g_toastFontPx = 32;

static intptr_t __stdcall ToastProc(HWND h, unsigned int msg, uintptr_t w, intptr_t l)
{
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r = {0, 0, 4000, 400};
        HGDIOBJ bg = CreateSolidBrush(0x00201A18);
        FillRect(dc, &r, bg);
        DeleteObject(bg);
        HGDIOBJ font = CreateFontA(g_toastFontPx, 0, 0, 0, 600, 0, 0, 0, 1, 0, 0, 4, 0, "Segoe UI");
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, 1);
        SetTextColor(dc, 0x00FFFFFF);
        int len = 0; while (g_toastText[len]) ++len;
        TextOutA(dc, g_toastFontPx, g_toastFontPx / 3, g_toastText, len);
        SelectObject(dc, old);
        DeleteObject(font);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcA(h, msg, w, l);
}

struct Crosshair {
    Settings* cfg = nullptr;
    volatile bool running = false;
    HANDLE thread = nullptr;

    void Start(Settings* c)
    {
        cfg = c; running = true;
        thread = CreateThread(nullptr, 0, &Crosshair::Entry, this, 0, nullptr);
    }
    void Stop()
    {
        running = false;
        if (thread) { WaitForSingleObject(thread, 2000); CloseHandle(thread); thread = nullptr; }
    }
    static unsigned long __stdcall Entry(void* self) { ((Crosshair*)self)->Run(); return 0; }

    void Run()
    {
        WNDCLASSEXA wc;
        memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = &CrosshairProc;
        wc.hInstance = g_module;
        wc.lpszClassName = "NoGogglesCrosshair";
        RegisterClassExA(&wc);

        WNDCLASSEXA tc;
        memset(&tc, 0, sizeof tc);
        tc.cbSize = sizeof tc;
        tc.lpfnWndProc = &ToastProc;
        tc.hInstance = g_module;
        tc.lpszClassName = "NoGogglesToast";
        RegisterClassExA(&tc);
        HWND toast = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                     "NoGogglesToast", "NoGoggles message", WS_POPUP,
                                     0, 0, 10, 10, nullptr, nullptr, g_module, nullptr);
        int seenSeq = 0; // so the "ready" message posted at startup still shows
        int toastFrames = 0;
        if (toast) SetLayeredWindowAttributes(toast, 0, 225, LWA_ALPHA);

        HWND h = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                 "NoGogglesCrosshair", "NoGoggles crosshair", WS_POPUP,
                                 0, 0, 10, 10, nullptr, nullptr, g_module, nullptr);
        if (!h) { Log(Text().s("[NoGoggles] Couldn't create the crosshair window")); if (toast) DestroyWindow(toast); return; }

        int shownSize = -1, shownAlpha = -1; bool visible = false;
        int shownX = -99999, shownY = -99999, shownN = -1;
        WindowJuggler juggler;
        int tick = 0;
        while (running) {
            MSG m;
            while (PeekMessageA(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessageA(&m);

            // --- messages
            if (toast && g_toastSeq != seenSeq) {
                seenSeq = g_toastSeq;
                g_toastFontPx = cfg->windowH / 36 < 18 ? 18 : cfg->windowH / 36;
                HDC dc = GetDC(toast);
                HGDIOBJ font = CreateFontA(g_toastFontPx, 0, 0, 0, 600, 0, 0, 0, 1, 0, 0, 4, 0, "Segoe UI");
                HGDIOBJ old = SelectObject(dc, font);
                int len = 0; while (g_toastText[len]) ++len;
                SIZE ext = {0, 0};
                GetTextExtentPoint32A(dc, g_toastText, len, &ext);
                SelectObject(dc, old);
                DeleteObject(font);
                ReleaseDC(toast, dc);
                int w = ext.cx + g_toastFontPx * 2;
                int hgt = ext.cy + (g_toastFontPx / 3) * 2;
                int x = cfg->windowX + cfg->windowW / 2 - w / 2;
                int y = cfg->windowY + cfg->windowH / 10;
                SetWindowRgn(toast, CreateRoundRectRgn(0, 0, w + 1, hgt + 1, hgt / 2, hgt / 2), 1);
                SetWindowPos(toast, HWND_TOPMOST, x, y, w, hgt, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                InvalidateRect(toast, nullptr, 1);
                toastFrames = 110; // about two seconds
            }
            if (toastFrames > 0) {
                if ((toastFrames & 7) == 0 && IsBelow(toast, juggler.compositor))
                    SetWindowPos(toast, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                if (--toastFrames == 0) ShowWindow(toast, SW_HIDE);
            }

            if (++tick >= 30) { // about twice a second
                tick = 0;
                juggler.Tick(*cfg);
                if (cfg->crosshair && !g_crosshairHidden) {
                    int n = cfg->crosshairSize;
                    int alpha = (int)(cfg->crosshairOpacity * 255);
                    if (n != shownSize) {
                        g_crossSize = n;
                        SetWindowRgn(h, CreateEllipticRgn(0, 0, n + 1, n + 1), 1);
                        shownSize = n;
                        InvalidateRect(h, nullptr, 1);
                    }
                    if (alpha != shownAlpha) { SetLayeredWindowAttributes(h, 0, (unsigned char)alpha, LWA_ALPHA); shownAlpha = alpha; }
                    int x = cfg->windowX + cfg->windowW / 2 - n / 2;
                    int y = cfg->windowY + cfg->windowH / 2 - n / 2;
                    bool moved = x != shownX || y != shownY || n != shownN || !visible;
                    bool covered = IsBelow(h, juggler.compositor);
                    if (moved || covered) {
                        // lift it above the NoGoggles view only when it's actually hidden
                        SetWindowPos(h, HWND_TOPMOST, x, y, n, n, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        shownX = x; shownY = y; shownN = n;
                    }
                    visible = true;
                } else if (visible) {
                    ShowWindow(h, SW_HIDE);
                    visible = false;
                }
            }
            Sleep(16);
        }
        DestroyWindow(h);
        UnregisterClassA("NoGogglesCrosshair", g_module);
        if (toast) DestroyWindow(toast);
        UnregisterClassA("NoGogglesToast", g_module);
    }
};

// Closing SteamVR the polite way: ask the SteamVR Status window to close,
// the same as clicking its X.
static void QuitSteamVR()
{
    Log(Text().s("[NoGoggles] Quit key held - closing SteamVR"));
    char cmd[] = "taskkill.exe /IM vrmonitor.exe";
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); memset(&pi, 0, sizeof pi);
    si.cb = sizeof si;
    if (CreateProcessA(nullptr, cmd, nullptr, nullptr, 0, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}

// ---------------------------------------------------------------------------
// The headset
// ---------------------------------------------------------------------------
class NoGogglesHmd : public vr::ITrackedDeviceServerDriver {
public:
    Settings cfg;
    NoGogglesDisplay display;
    uint32_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
    volatile bool running = false;
    HANDLE thread = nullptr;

    // look state
    double yaw = 0, pitch = 0;          // target, degrees
    double shownYaw = 0, shownPitch = 0; // smoothed
    double prevYaw = 0, prevPitch = 0;
    double yawRate = 0, pitchRate = 0;   // degrees per second, for SteamVR's prediction
    bool mouseLook = false;
    bool toggleWasDown = false, recenterWasDown = false, padRecenterWasDown = false;
    POINT anchor = {0, 0};
    Crosshair crosshair;
    bool crossWasDown = false;
    double quitHeld = 0;
    bool quitSent = false, quitWarned = false;
    vr::VRInputComponentHandle_t proximity = 0;
    double proximityTimer = 0;

    vr::EVRInitError Activate(uint32_t id) override
    {
        deviceIndex = id;
        auto c = vr::VRProperties()->TrackedDeviceToPropertyContainer(id);
        auto* p = vr::VRProperties();
        p->SetStringProperty(c, vr::Prop_ModelNumber_String, "NoGoggles");
        p->SetStringProperty(c, vr::Prop_ManufacturerName_String, "NoGoggles");
        p->SetStringProperty(c, vr::Prop_TrackingSystemName_String, "nogoggles");
        p->SetStringProperty(c, vr::Prop_RenderModelName_String, "generic_hmd");
        p->SetFloatProperty(c, vr::Prop_UserIpdMeters_Float, 0.0f);
        p->SetFloatProperty(c, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.0f);
        p->SetFloatProperty(c, vr::Prop_DisplayFrequency_Float, (float)cfg.refreshRate);
        p->SetFloatProperty(c, vr::Prop_SecondsFromVsyncToPhotons_Float, 0.011f);
        p->SetBoolProperty(c, vr::Prop_IsOnDesktop_Bool, false);
        p->SetBoolProperty(c, vr::Prop_DisplayDebugMode_Bool, true);
        // Tell SteamVR the headset is "being worn" at all times. Without this,
        // a headset that never moves is treated as sitting on a desk, and games
        // show errors like "HMD Tracking Error".
        p->SetBoolProperty(c, vr::Prop_ContainsProximitySensor_Bool, true);
        p->SetUint64Property(c, vr::Prop_CurrentUniverseId_Uint64, kUniverseId);
        p->SetBoolProperty(c, vr::Prop_HasDisplayComponent_Bool, true);
        p->SetStringProperty(c, vr::Prop_InputProfilePath_String, "{nogoggles}/input/nogoggles_profile.json");

        proximity = vr::k_ulInvalidInputComponentHandle;
        vr::VRDriverInput()->CreateBooleanComponent(c, "/proximity", &proximity);
        vr::VRDriverInput()->UpdateBooleanComponent(proximity, true, 0);

        LoadXInput();
        running = true;
        thread = CreateThread(nullptr, 0, &NoGogglesHmd::ThreadEntry, this, 0, nullptr);
        crosshair.Start(&cfg);
        if (cfg.messages) Toast("NoGoggles ready  -  F8 mouse look,  F9 recenter,  hold F10 to quit");

        Log(Text().s("[NoGoggles] v0.7 headset active. Window ").i(cfg.windowW).s("x").i(cfg.windowH)
                .s(" at ").i(cfg.windowX).s(",").i(cfg.windowY).s(", FOV ").f(cfg.fovDegrees)
                .s(xinputNote()));
        return vr::VRInitError_None;
    }

    const char* xinputNote() { return g_xinputGetState ? ", gamepad reader ready" : ", no XInput found"; }

    void Deactivate() override
    {
        running = false;
        if (thread) { WaitForSingleObject(thread, 2000); CloseHandle(thread); thread = nullptr; }
        crosshair.Stop();
        deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
    }
    void EnterStandby() override {}
    void* GetComponent(const char* name) override
    {
        if (strcmp(name, vr::IVRDisplayComponent_Version) == 0) return &display;
        return nullptr;
    }
    void DebugRequest(const char*, char* buf, uint32_t size) override { if (size) buf[0] = 0; }

    vr::DriverPose_t GetPose() override
    {
        vr::DriverPose_t pose;
        memset(&pose, 0, sizeof pose);
        pose.qWorldFromDriverRotation.w = 1;
        pose.qDriverFromHeadRotation.w = 1;

        double y = shownYaw * kPi / 180.0, x = shownPitch * kPi / 180.0;
        double cy = Cos(y / 2), sy = Sin(y / 2), cp = Cos(x / 2), sp = Sin(x / 2);
        // yaw (around up) then pitch (around right)
        pose.qRotation.w = cy * cp;
        pose.qRotation.x = cy * sp;
        pose.qRotation.y = sy * cp;
        pose.qRotation.z = -sy * sp;

        pose.vecPosition[1] = cfg.headHeight;
        pose.poseIsValid = true;
        pose.deviceIsConnected = true;
        pose.result = vr::TrackingResult_Running_OK;
        pose.shouldApplyHeadModel = false;
        pose.willDriftInYaw = false;

        if (cfg.predictTurning) {
            // world-space angular velocity (radians/sec): yaw spins around "up",
            // pitch spins around the head's current "right" direction
            double wy = yawRate * kPi / 180.0, wp = pitchRate * kPi / 180.0;
            pose.vecAngularVelocity[0] = wp * Cos(y);
            pose.vecAngularVelocity[1] = wy;
            pose.vecAngularVelocity[2] = -wp * Sin(y);
        }
        return pose;
    }

    static unsigned long __stdcall ThreadEntry(void* self)
    {
        ((NoGogglesHmd*)self)->Loop();
        return 0;
    }

    void Loop()
    {
        LARGE_INTEGER freq, last, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
        double reloadTimer = 0, profileTimer = 2.5;

        while (running) {
            QueryPerformanceCounter(&now);
            double dt = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
            last = now;
            if (dt > 0.1) dt = 0.1;

            reloadTimer += dt;
            if (reloadTimer > 1.0) { reloadTimer = 0; ReloadLookSettings(cfg); }
            profileTimer += dt;
            if (profileTimer > 3.0) { profileTimer = 0; CheckProfile(cfg); }

            UpdateLook(dt);

            proximityTimer += dt;
            if (proximityTimer > 0.5) {
                proximityTimer = 0;
                vr::VRDriverInput()->UpdateBooleanComponent(proximity, true, 0);
            }

            if (deviceIndex != vr::k_unTrackedDeviceIndexInvalid)
                vr::VRServerDriverHost()->TrackedDevicePoseUpdated(deviceIndex, GetPose(), sizeof(vr::DriverPose_t));
            Sleep(4);
        }
    }

    void UpdateLook(double dt)
    {
        // --- mouse look on/off
        bool toggleDown = KeyDown(cfg.mouseToggleKey);
        if (toggleDown && !toggleWasDown) {
            mouseLook = !mouseLook;
            if (mouseLook) {
                // park the pointer in the middle of the NoGoggles window
                anchor.x = cfg.windowX + cfg.windowW / 2;
                anchor.y = cfg.windowY + cfg.windowH / 2;
                SetCursorPos(anchor.x, anchor.y);
            }
            Log(Text().s(mouseLook ? "[NoGoggles] Mouse look ON" : "[NoGoggles] Mouse look OFF"));
            if (cfg.messages) Toast(mouseLook ? "Mouse look ON" : "Mouse look OFF");
        }
        toggleWasDown = toggleDown;

        if (mouseLook) {
            POINT p;
            if (GetCursorPos(&p)) {
                double dx = p.x - anchor.x, dy = p.y - anchor.y;
                yaw -= dx * cfg.mouseSensitivity;
                pitch += (cfg.invertY ? dy : -dy) * cfg.mouseSensitivity;
                if (dx != 0 || dy != 0) SetCursorPos(anchor.x, anchor.y);
            }
        }

        // --- gamepad stick
        XInputGamepad pad;
        bool havePad = ReadPad(pad);
        if (havePad && cfg.stick) {
            bool allowed = cfg.stickHoldButton == 0 || PadHeld(pad, cfg.stickHoldButton);
            if (allowed) {
                short sx = cfg.stick == 1 ? pad.lx : pad.rx;
                short sy = cfg.stick == 1 ? pad.ly : pad.ry;
                double ax = StickAxis(sx, cfg.stickDeadzone), ay = StickAxis(sy, cfg.stickDeadzone);
                yaw -= ax * cfg.stickSpeed * dt;
                pitch += (cfg.invertY ? -ay : ay) * cfg.stickSpeed * dt;
            }
        }

        // --- keyboard
        if (cfg.keyboardLook) {
            double k = cfg.keyboardSpeed * dt;
            if (KeyDown(0x25) || KeyDown(0x64)) yaw += k;   // left / numpad 4
            if (KeyDown(0x27) || KeyDown(0x66)) yaw -= k;   // right / numpad 6
            if (KeyDown(0x26) || KeyDown(0x68)) pitch += k; // up / numpad 8
            if (KeyDown(0x28) || KeyDown(0x62)) pitch -= k; // down / numpad 2
        }

        // --- recenter
        bool rc = KeyDown(cfg.recenterKey);
        if (rc && !recenterWasDown) { yaw = 0; pitch = 0; shownYaw = 0; shownPitch = 0; if (cfg.messages) Toast("View recentered"); }
        recenterWasDown = rc;

        // --- hold the quit key for a second to close SteamVR
        if (KeyDown(cfg.quitKey) || (havePad && PadAll(pad, cfg.quitPad))) {
            quitHeld += dt;
            if (!quitWarned && quitHeld > 0.3) { quitWarned = true; if (cfg.messages) Toast("Keep holding to close SteamVR..."); }
            if (quitHeld > 1.5 && !quitSent) { quitSent = true; if (cfg.messages) Toast("Closing SteamVR"); QuitSteamVR(); }
        } else {
            quitHeld = 0; quitSent = false; quitWarned = false;
        }
        bool prc = havePad && PadAll(pad, cfg.recenterPad);
        if (prc && !padRecenterWasDown) { yaw = 0; pitch = 0; shownYaw = 0; shownPitch = 0; if (cfg.messages) Toast("View recentered"); }

        // --- crosshair show/hide key
        bool ct = KeyDown(cfg.crosshairToggleKey) || (havePad && PadAll(pad, cfg.crosshairTogglePad));
        if (ct && !crossWasDown) {
            g_crosshairHidden = !g_crosshairHidden;
            if (cfg.messages) Toast(g_crosshairHidden ? "Crosshair hidden" : "Crosshair shown");
        }
        crossWasDown = ct;
        padRecenterWasDown = prc;

        // keep yaw tidy, limit up/down
        while (yaw > 180) { yaw -= 360; shownYaw -= 360; }
        while (yaw < -180) { yaw += 360; shownYaw += 360; }
        pitch = Clamp(pitch, -cfg.maxPitch, cfg.maxPitch);

        // --- smoothing (frame-rate independent)
        if (cfg.smoothing > 0) {
            double keep = 1.0;
            // keep = smoothing ^ (dt * 60)
            double e = dt * 60.0, base = cfg.smoothing;
            int whole = (int)e;
            for (int i = 0; i < whole; ++i) keep *= base;
            keep *= 1.0 - (1.0 - base) * (e - whole);
            shownYaw = yaw + (shownYaw - yaw) * keep;
            shownPitch = pitch + (shownPitch - pitch) * keep;
        } else {
            shownYaw = yaw; shownPitch = pitch;
        }

        // --- turning speed. SteamVR uses it to predict where the head will be
        // when the frame reaches the screen. Without it, turning shows a black
        // strip at the edge you're turning toward.
        if (dt > 0) {
            double dy = shownYaw - prevYaw, dp = shownPitch - prevPitch;
            while (dy > 180) dy -= 360;
            while (dy < -180) dy += 360;
            double ry = Clamp(dy / dt, -720, 720), rp = Clamp(dp / dt, -720, 720);
            double k = Clamp(dt * 25.0, 0.0, 1.0); // light filtering for jumpy mouse input
            yawRate += (ry - yawRate) * k;
            pitchRate += (rp - pitchRate) * k;
        }
        if (rc || prc) { yawRate = 0; pitchRate = 0; }
        prevYaw = shownYaw; prevPitch = shownPitch;
    }
};

// ---------------------------------------------------------------------------
// The driver SteamVR talks to
// ---------------------------------------------------------------------------
class NoGogglesProvider : public vr::IServerTrackedDeviceProvider {
public:
    NoGogglesHmd* hmd = nullptr;

    vr::EVRInitError Init(vr::IVRDriverContext* ctx) override
    {
        VR_INIT_SERVER_DRIVER_CONTEXT(ctx);
        hmd = new (hmdStorage) NoGogglesHmd();
        LoadDisplaySettings(hmd->cfg);
        ReloadLookSettings(hmd->cfg);
        hmd->display.cfg = &hmd->cfg;
        Log(Text().s("[NoGoggles] Settings file: ").s(g_iniPath));
        if (!vr::VRServerDriverHost()->TrackedDeviceAdded("NOGOGGLES-0001", vr::TrackedDeviceClass_HMD, hmd)) {
            Log(Text().s("[NoGoggles] SteamVR refused the headset"));
            return vr::VRInitError_Driver_Unknown;
        }
        return vr::VRInitError_None;
    }
    void Cleanup() override { VR_CLEANUP_SERVER_DRIVER_CONTEXT(); }
    const char* const* GetInterfaceVersions() override { return vr::k_InterfaceVersions; }
    void RunFrame() override
    {
        vr::VREvent_t e;
        while (vr::VRServerDriverHost()->PollNextEvent(&e, sizeof e)) {}
    }
    bool ShouldBlockStandbyMode() override { return true; } // never go to sleep mid-game
    void EnterStandby() override {}
    void LeaveStandby() override {}

    alignas(16) unsigned char hmdStorage[sizeof(NoGogglesHmd)];
};

static NoGogglesProvider g_provider;

static void FindIniPath()
{
    // This DLL lives in <driver>\bin\win64\driver_nogoggles.dll; the ini is in <driver>\.
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (const char*)&FindIniPath, &self);
    g_module = self;
    unsigned long n = GetModuleFileNameA(self, g_iniPath, sizeof(g_iniPath) - 32);
    int slashes = 0;
    while (n > 0) {
        --n;
        if (g_iniPath[n] == '\\' || g_iniPath[n] == '/') {
            if (++slashes == 3) break;
        }
    }
    const char* name = "\\NoGoggles.ini";
    int i = (int)n;
    while (*name) g_iniPath[i++] = *name++;
    g_iniPath[i] = 0;
}

extern "C" __declspec(dllexport) void* HmdDriverFactory(const char* iface, int* rc)
{
    if (strcmp(iface, vr::IServerTrackedDeviceProvider_Version) == 0) {
        FindIniPath();
        return &g_provider;
    }
    if (rc) *rc = vr::VRInitError_Init_InterfaceNotFound;
    return nullptr;
}
