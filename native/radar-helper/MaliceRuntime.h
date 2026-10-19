// SPDX-License-Identifier: MIT
#pragma once
#include "MaliceEnergy.h"
#include "MaliceLoftHook.h"
#include "MaliceRecovery.h"
#include <unordered_map>
#include <deque>
#include <iterator>

// Included after Radar.cpp's checked memory and symbol helpers.
namespace malice_energy {
using Simulate = double(*)(void*, double);
using Switch = void(*)(void*, int);
static f23radar::SlotHook guidanceHook, engineHook, wcseHook;
static f23radar::MaliceLoftHook loftHook;
static Simulate nativeGuidance{}, nativeEngine{}, nativeLoft{}, nativeWcse{};
// Support hold (Stage 26): 0 off, 1 active, -47 no MALICE datalink identity, -48 unknown bytes, -49 install failed.
// A hold failure never changes the energy profile.
static int holdStatus = 0;
static Switch nativeSwitch{};
static f23radar::EnergyIdentity identities[2];
static int identityCount = 0, status = 0;
static bool active = false, nativeSupported = false;
static uintptr_t weaponBase = 0;
static f23radar::EnergyActions actions;
static unsigned long long stockPasses = 0, threadMismatches = 0;
static DWORD timerApplyThread = 0, timerRestoreThread = 0, physicsThread = 0;
static bool restored = true;
static SRWLOCK lock = SRWLOCK_INIT;
struct Guard { Guard() { AcquireSRWLockExclusive(&lock); } ~Guard() { ReleaseSRWLockExclusive(&lock); } };
static std::unordered_map<void*, f23radar::EnergyState> states;
static std::unordered_map<void*, f23radar::SupportHoldRecord> holdRecords;   // Key: live datalink block.
static std::deque<std::string> messages;
static f23radar::SlotHook chaffHook;
static f23radar::AlignedEntryHook terminalHook;
using Candidate = int(*)(void*, void*);
static Candidate nativeCandidate{};
static f23radar::GuidanceStep nativeTerminal{}, nativeInertial{}, nativeSeekerControl{};
static const void* (*chaffObjectType)(const void*){};
static f23radar::RecoveryIdentity recoveryIdentities[2];
// Independent status: -50/-53 identity, -51/-54 bytes or runtime contract, -52/-55 install.
static int chaffStatus = 0, recoveryStatus = 0;
static unsigned long long chaffRejected = 0, recoveryFrames = 0;
struct RecoveryRecord { double age = -1, lossStart = -1; bool running = false; };
static std::unordered_map<void*, RecoveryRecord> recoveryRecords;
static RecoveryRecord& recoveryRecord(void* block) {
    auto& record = recoveryRecords[field<void*>(block, 0x10)];
    const double age = field<double>(block, 0x170);
    if (age < record.age) record = {};
    record.age = age;
    if (field<unsigned char>(block, 0x167)) record.lossStart = -1;
    else if (record.lossStart < 0) record.lossStart = age;
    return record;
}

static bool hashBytes(const void* bytes, size_t size, const char* expected) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{}; unsigned char digest[32]{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    if (ok) ok = BCryptHashData(hash, const_cast<PUCHAR>(static_cast<const unsigned char*>(bytes)),
                               static_cast<ULONG>(size), 0) >= 0;
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    std::string text;
    for (auto byte : digest) { text += "0123456789abcdef"[byte>>4]; text += "0123456789abcdef"[byte&15]; }
    return ok && text == expected;
}

struct LuaApi {
    int (*top)(lua_State*){};
    void (*setTop)(lua_State*, int){};
    void (*get)(lua_State*, int, const char*){};
    void (*set)(lua_State*, int, const char*){};
    int (*type)(lua_State*, int){};
    void (*copy)(lua_State*, int){};
    int (*dump)(lua_State*, int(*)(lua_State*, const void*, size_t, void*), void*){};
    int (*load)(lua_State*, const char*, size_t, const char*){};
    int (*ref)(lua_State*, int){};
    void (*unref)(lua_State*, int, int){};
    void (*rawGet)(lua_State*, int, int){};
    int (*equal)(lua_State*, int, int){};
    bool bind() {
        auto module = GetModuleHandleW(L"lua.dll");
        top = symbol<decltype(top)>(module, "lua_gettop");
        setTop = symbol<decltype(setTop)>(module, "lua_settop");
        get = symbol<decltype(get)>(module, "lua_getfield");
        set = symbol<decltype(set)>(module, "lua_setfield");
        type = symbol<decltype(type)>(module, "lua_type");
        copy = symbol<decltype(copy)>(module, "lua_pushvalue");
        dump = symbol<decltype(dump)>(module, "lua_dump");
        load = symbol<decltype(load)>(module, "luaL_loadbuffer");
        ref = symbol<decltype(ref)>(module, "luaL_ref");
        unref = symbol<decltype(unref)>(module, "luaL_unref");
        rawGet = symbol<decltype(rawGet)>(module, "lua_rawgeti");
        equal = symbol<decltype(equal)>(module, "lua_rawequal");
        return top && setTop && get && set && type && copy && dump && load && ref && unref && rawGet && equal;
    }
};
static LuaApi lua;
struct TimerProfile {
    void* descriptor{};
    lua_State* state{};
    int original = -2, replacement = -2;
    bool applied = false;
    static int writer(lua_State*, const void* bytes, size_t length, void* output) {
        static_cast<std::string*>(output)->append(static_cast<const char*>(bytes), length); return 0;
    }
    bool valid() const {
        return accessible(descriptor, 0xae8) && field<lua_State*>(descriptor, 0xae0) == state
            && accessible(state, 32);
    }
    bool prepare(void* desc) {
        descriptor = desc; state = field<lua_State*>(desc, 0xae0);
        if (!valid()) return false;
        const int top = lua.top(state);
        lua.get(state, -10002, "run");
        std::string bytes;
        bool ok = lua.type(state, -1) == 6 && !lua.dump(state, writer, &bytes)
            && f23radar::delayController(bytes, hashBytes);
        if (ok) {
            lua.copy(state, -1); original = lua.ref(state, -10000);
            ok = !lua.load(state, bytes.data(), bytes.size(), "F23B_MALICE_controller");
            if (ok) replacement = lua.ref(state, -10000);
        }
        lua.setTop(state, top);
        return ok;
    }
    bool apply() {
        if (!valid() || original < 0 || replacement < 0) return false;
        const int top = lua.top(state);
        lua.get(state, -10002, "run"); lua.rawGet(state, -10000, original);
        const bool ok = lua.equal(state, -1, -2) != 0;
        lua.setTop(state, top);
        if (ok) {
            lua.rawGet(state, -10000, replacement); lua.set(state, -10002, "run"); applied = true;
            timerApplyThread = GetCurrentThreadId();
        }
        return ok;
    }
    bool restore() {
        if (!state) return true;
        timerRestoreThread = GetCurrentThreadId();
        // Refuse a Lua VM write from a different observed weapon-physics thread.
        if (physicsThread && timerRestoreThread != physicsThread) return false;
        if (!valid()) return false;
        const int top = lua.top(state);
        bool ok = true;
        if (applied) {
            lua.get(state, -10002, "run"); lua.rawGet(state, -10000, replacement);
            ok = lua.equal(state, -1, -2) != 0;
            lua.setTop(state, top);
            if (ok) { lua.rawGet(state, -10000, original); lua.set(state, -10002, "run"); }
        }
        if (ok) {
            if (original >= 0) lua.unref(state, -10000, original);
            if (replacement >= 0) lua.unref(state, -10000, replacement);
            *this = {};
        }
        return ok;
    }
};
static TimerProfile timers[2];
static int timerCount = 0;

// Call with the lock held. Unmatched descriptors receive the original function without state writes.
static const f23radar::EnergyIdentity* identify(void* block, bool engine) {
    if (!active || !accessible(block, 0x18)) return nullptr;
    auto desc = field<void*>(block, 8);
    for (int i = 0; i < identityCount; ++i)
        if (desc == (engine ? identities[i].march : identities[i].guidance)) return &identities[i];
    return nullptr;
}
// Call with the lock held. Only a MALICE datalink descriptor matches; every other missile passes through.
static bool identifyWcse(void* block) {
    if (!active || holdStatus != 1 || !accessible(block, 0x298)) return false;
    auto desc = field<void*>(block, 8);
    for (int i = 0; i < identityCount; ++i)
        if (identities[i].wcse && desc == identities[i].wcse) return true;
    return false;
}
// Called under the mutex, from the actual native weapon callbacks.
static void observePhysicsThread() {
    const auto thread = GetCurrentThreadId();
    if (!physicsThread) physicsThread = thread;
    if (thread != timerApplyThread || thread != physicsThread) {
        ++threadMismatches;
        if (threadMismatches == 1)
            messages.emplace_back("ERROR MALICE energy: controller profile and weapon physics use different threads");
    }
}
static f23radar::EnergyState& stateFor(void* guidance, double age) {
    auto system = field<void*>(guidance, 0x10);
    auto& value = states[system];
    if (value.lastAge < 0 || age+1e-8 < value.lastAge) {
        value = {};
    }
    const double range = field<double>(guidance, 0x178);
    if (!value.loftPlanned && value.initialize(age, range,
        field<double>(guidance, 0x1b0)+field<double>(guidance, 0x228))) {
        char line[256];
        std::snprintf(line, sizeof(line), "MALICE loft plan: missile_id=%llx launch_range=%.3f gain=%.6f end_range=%.3f",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(system)), range, value.loftGain, value.loftEnd);
        messages.emplace_back(line);
    }
    value.lastAge = age;
    return value;
}
static double loft(void* block, double time) {
    const double original = nativeLoft(block, time);
    Guard guard;
    if (!identify(block, false) || !accessible(block, 0x230, true)) return original;
    observePhysicsThread();
    auto& value = stateFor(block, field<double>(block, 0x170));
    const double range = field<double>(block, 0x180), altitude = field<double>(block, 0x1b0);
    const double speed = field<double>(block, 0x190);
    const double result = value.loft(original, range, altitude, field<double>(block, 0x170), speed);
    if (!value.loftStopped && !value.ceilingReported && value.coastApex(altitude, speed) >= f23radar::loftCeiling) {
        value.ceilingReported = true; char line[256];
        std::snprintf(line, sizeof(line), "MALICE loft ceiling: missile_id=%llx time=%.3f range=%.3f altitude=%.3f speed=%.1f climb_sine=%.4f",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(field<void*>(block, 0x10))),
            field<double>(block, 0x170), range, altitude, speed, value.climbSine);
        messages.emplace_back(line);
    }
    if (value.loftStopped) {
        const unsigned char disabled = 0; std::memcpy(static_cast<char*>(block)+0x162, &disabled, 1);
        if (!value.loftReported) {
            value.loftReported = true; char line[256];
            std::snprintf(line, sizeof(line), "MALICE loft end: missile_id=%llx time=%.6f range=%.3f altitude=%.3f",
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(field<void*>(block, 0x10))),
                field<double>(block, 0x170), range, field<double>(block, 0x1b0));
            messages.emplace_back(line);
        }
    }
    return result;
}
static double guidance(void* block, double time) {
    int before = -1;
    { Guard guard; if (identify(block, false) && accessible(block, 0x230)) {
        observePhysicsThread(); before = field<int>(block, 0x158);
      }
      else ++stockPasses; }
    const double result = nativeGuidance(block, time);
    if (before >= 0) {
        Guard guard;
        if (identify(block, false) && accessible(block, 0x230)) {
            auto& value = stateFor(block, time);
            value.observe(before, field<int>(block, 0x158), time);
            value.track(time, field<double>(block, 0x180));
            if (recoveryStatus == 1) recoveryRecord(block);
        }
    }
    return result;
}
static double engine(void* block, double time) {
    bool fired = false; double seekerAge = -1, range = 0; void* system = nullptr;
    {
        Guard guard;
        if (auto id = identify(block, true)) {
            observePhysicsThread();
            system = field<void*>(block, 0x10);
            auto it = states.find(system);
            if (it != states.end() && !it->second.fired && it->second.seekerAge >= 0) {
                auto boost = f23radar::sibling(block, *id, id->boost, weaponBase+f23radar::engineVtable, accessible);
                auto gp = f23radar::sibling(block, *id, id->guidance, weaponBase+f23radar::guidanceVtable, accessible);
                if (boost && gp && accessible(gp, 0x230) && accessible(block, 0x138)) {
                    auto& value = it->second;
                    const double believed = field<double>(gp, 0x180), speed = field<double>(gp, 0x190);
                    fired = value.ignite(field<int>(boost, 0x100), field<int>(block, 0x100),
                        field<double>(block, 0x108), field<double>(block, 0x130), believed, speed);
                    if (value.withheld && !value.withheldReported) {
                        value.withheldReported = true; char line[256];
                        std::snprintf(line, sizeof(line), "MALICE pulse withheld: missile_id=%llx range=%.3f closing=%.1f speed=%.1f time_to_go=%.2f limit=%.1f",
                            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(system)), believed, value.closing, speed,
                            value.timeToGo(believed, speed), f23radar::minimumPulseTime);
                        messages.emplace_back(line);
                    }
                    if (fired) {
                        seekerAge = value.seekerAge; range = field<double>(gp, 0x180);
                        // Classify the actual action target immediately before the single native switch call.
                        actions.record(field<void*>(block, 8) == id->march);
                        nativeSwitch(block, 1);
                    }
                } else if (status == 1 && !it->second.withheld) {
                    // A held pulse is checked again until the missile ends; only the first decision reports a fault.
                    status = -45; messages.emplace_back("ERROR MALICE energy: sibling identity check failed; no ignition");
                }
            }
        }
    }
    const double result = nativeEngine(block, time);
    if (fired) {
        Guard guard; char line[256];
        std::snprintf(line, sizeof(line), "MALICE pulse: missile_id=%llx time=%.6f seeker_time=%.6f range=%.3f fuel_kg=77.39472752 duration=17.55",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(system)), field<double>(block, 0x108), seekerAge, range);
        messages.emplace_back(line);
    }
    return result;
}

// Datalink update of every wWCSE_AMRAAM missile. For a MALICE it ties the launcher's support answer
// (supported_tracking, called inside the native update) to this missile. No lock is held during the
// native call, because the support answer runs on the same thread.
static double wcse(void* block, double time) {
    f23radar::MaliceSupportQuery query;
    bool malice = false;
    {
        Guard guard;
        if (modelTime && identifyWcse(block)) {
            malice = true;
            auto& record = holdRecords[block];
            record.refresh(field<unsigned>(block, 0x290), field<unsigned>(block, 0x294), modelTime());
            query.supported = record.supported;
        }
    }
    if (malice) maliceSupportQuery = &query;
    const double result = nativeWcse(block, time);
    maliceSupportQuery = nullptr;
    if (malice && query.asked) {
        Guard guard;
        const double now = modelTime();
        auto it = holdRecords.find(block);
        if (it != holdRecords.end()) {
            auto& record = it->second;
            const int change = record.apply(query, now);
            if (change) {
                char line[256];
                const auto id = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(field<void*>(block, 0x10)));
                if (change == 1)
                    std::snprintf(line, sizeof(line), "MALICE support hold: missile_id=%llx target=%u model_time=%.3f",
                        id, record.target, now);
                else
                    std::snprintf(line, sizeof(line), "MALICE support hold end: missile_id=%llx target=%u model_time=%.3f held_s=%.1f reason=%s",
                        id, record.target, now, now-record.holdStart,
                        query.granted ? "track" : "refused");
                messages.emplace_back(line);
            }
        }
        // Records of ended missiles: the engine stops simulating their blocks.
        if (holdRecords.size() > 64)
            for (auto i = holdRecords.begin(); i != holdRecords.end();)
                i = now - i->second.lastSeen > 30 ? holdRecords.erase(i) : std::next(i);
    }
    return result;
}

// The vtable points to the unadjusted handler. Its +8 field is the actual radar sensor.
static int candidate(void* handler, void* object) {
    bool isMalice = false;
    {
        Guard guard;
        if (active && chaffStatus == 1 && accessible(handler, 16)) {
            auto sensor = field<void*>(handler, 8);
            if (accessible(sensor, 16)) {
                auto descriptor = field<void*>(sensor, 8);
                for (int i = 0; i < identityCount; ++i)
                    if (descriptor && descriptor == recoveryIdentities[i].sensor) isMalice = true;
            }
        }
    }
    if (isMalice) {
        auto getObject = method<void*(*)(void*)>(object, 0x30);
        auto scene = getObject ? getObject(object) : nullptr;
        auto getMoving = method<void*(*)(void*)>(scene, 0x38);
        auto moving = getMoving ? getMoving(scene) : nullptr;
        auto getCategory = method<unsigned(*)(void*)>(moving, 0x190);
        const auto category = getCategory ? getCategory(moving) : 0;
        const auto type = category == 0x100 && chaffObjectType ? chaffObjectType(moving) : nullptr;
        if (accessible(type, sizeof(f23radar::WsType))
            && f23radar::rejectMaliceChaff(isMalice, category, field<uint16_t>(type, 4))) {
            Guard guard; ++chaffRejected;
            return 0;  // Continue enumeration without scoring or consuming a candidate slot.
        }
    }
    return nativeCandidate(handler, object);
}

static void terminal(void* block, double step) {
    bool useFallback = false, isMalice = false;
    double insAge = -1;
    {
        Guard guard;
        if (recoveryStatus == 1) if (auto id = identify(block, false)) {
            isMalice = true;
            const auto& sensors = recoveryIdentities[id-identities];
            auto sensor = f23radar::sibling(block, *id, sensors.sensor, weaponBase+f23radar::radarSensorVtable, accessible);
            auto ins = f23radar::sibling(block, *id, sensors.ins, weaponBase+f23radar::strapdownVtable, accessible);
            if (!sensor || !ins || !accessible(sensor, 0x348) || !accessible(ins, 0x110)
                || !accessible(block, 0x4f8, true)) {
                recoveryStatus = -54;
                messages.emplace_back("ERROR MALICE recovery: sibling/state identity failed; native guidance retained");
            } else {
                insAge = field<double>(ins, 0x108);
                useFallback = f23radar::useMaliceRecovery(true, field<int>(block, 0x158),
                    field<unsigned char>(block, 0x167) != 0, field<int>(sensor, 0x344),
                    field<unsigned>(sensor, 0x2bc), insAge);
            }
        }
    }
    if (useFallback && !f23radar::writeInertialReferences(block, step, nativeSeekerControl, accessible)) {
        Guard guard; recoveryStatus = -54; useFallback = false;
        messages.emplace_back("ERROR MALICE recovery: reference writer contract failed; native guidance retained");
    }
    if (isMalice) {
        Guard guard;
        auto& record = recoveryRecord(block);
        if (useFallback != record.running) {
            char line[256];
            const auto id = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(field<void*>(block, 0x10)));
            if (useFallback)
                std::snprintf(line, sizeof(line), "MALICE lost-lock fallback: missile_id=%llx model_time=%.3f loss_s=%.3f ins_age_s=%.3f",
                    id, modelTime ? modelTime() : record.age, record.lossStart < 0 ? 0 : record.age-record.lossStart, insAge);
            else
                std::snprintf(line, sizeof(line), "MALICE lost-lock fallback end: missile_id=%llx model_time=%.3f reason=%s ins_age_s=%.3f",
                    id, modelTime ? modelTime() : record.age,
                    field<unsigned char>(block, 0x167) ? "signal" : insAge >= f23radar::recoveryFreshSeconds ? "stale" : "mode", insAge);
            messages.emplace_back(line); record.running = useFallback;
        }
        if (useFallback) ++recoveryFrames;
    }
    // No mutex around native calls: inertial guidance calls the existing loft hook.
    if (useFallback) {
        nativeInertial(block, step);
        const unsigned char initialized = 0;
        std::memcpy(static_cast<char*>(block)+0x164, &initialized, 1);
    } else nativeTerminal(block, step);
}

static void startRecovery(void* malice, void* stock, int planned) {
    bool knownSensors = planned > 0, knownIns = planned > 0;
    for (int i = 0; i < planned; ++i) {
        recoveryIdentities[i] = f23radar::planRecovery(malice, stock, identities[i], accessible);
        knownSensors = knownSensors && recoveryIdentities[i].sensor;
        knownIns = knownIns && recoveryIdentities[i].ins;
    }
    const auto check = [&](uintptr_t rva, const unsigned char* bytes, size_t n) {
        const auto address = reinterpret_cast<void*>(weaponBase+rva);
        return accessible(address, n) && !std::memcmp(address, bytes, n);
    };
    const unsigned char thunk[] = {0x48,0x83,0xc1,0x08,0xe9,0x17,0xf9,0xff,0xff};
    const unsigned char callback[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x68,0x20,0x56,0x57,0x41,0x56};
    chaffObjectType = symbol<decltype(chaffObjectType)>(GetModuleHandleW(L"WorldGeneral.dll"), "?Type@MovingObject@@QEBAAEBVwsType@@XZ");
    nativeCandidate = reinterpret_cast<Candidate>(weaponBase+f23radar::chaffCallbackThunk);
    chaffStatus = !knownSensors ? -50
        : !chaffObjectType || !check(f23radar::chaffCallbackThunk, thunk, sizeof(thunk))
          || !check(0x2ede00, callback, sizeof(callback))
          || !accessible(reinterpret_cast<void*>(weaponBase+0x3965e8), 8)
          || field<uintptr_t>(reinterpret_cast<void*>(weaponBase), 0x3965e8) != reinterpret_cast<uintptr_t>(chaffObjectType)
          || !chaffHook.prepare(reinterpret_cast<uintptr_t*>(weaponBase+f23radar::chaffCallbackSlot),
              weaponBase+f23radar::chaffCallbackThunk, reinterpret_cast<void*>(&candidate)) ? -51
        : !chaffHook.install() ? -52 : 1;
    const unsigned char terminalEntry[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55};
    const unsigned char inertialEntry[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x57,0x48,0x81,0xec,0xb0,0,0,0};
    const unsigned char controlEntry[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x70,0x18,0x57};
    nativeInertial = reinterpret_cast<f23radar::GuidanceStep>(weaponBase+f23radar::inertialGuidance);
    nativeSeekerControl = reinterpret_cast<f23radar::GuidanceStep>(weaponBase+f23radar::seekerControl);
    recoveryStatus = !knownSensors || !knownIns ? -53
        : !check(f23radar::terminalGuidance, terminalEntry, sizeof(terminalEntry))
          || !check(f23radar::inertialGuidance, inertialEntry, sizeof(inertialEntry))
          || !check(f23radar::seekerControl, controlEntry, sizeof(controlEntry))
          || !terminalHook.prepare(weaponBase+f23radar::terminalGuidance, reinterpret_cast<void*>(&terminal),
              terminalEntry, sizeof(terminalEntry)) ? -54 : 1;
    nativeTerminal = reinterpret_cast<f23radar::GuidanceStep>(terminalHook.trampoline());
    if (recoveryStatus == 1 && !terminalHook.install()) recoveryStatus = -55;
    for (const auto item : {std::pair<const char*, int>{"chaff filter", chaffStatus}, {"recovery", recoveryStatus}})
        if (item.second != 1) {
            char line[128];
            std::snprintf(line, sizeof(line), "ERROR MALICE %s unavailable (code %d); other features retained", item.first, item.second);
            messages.emplace_back(line);
        }
}

static void stop() {
    { Guard guard; active = false; }
    // Restore hooks outside the mutex: a suspended simulation thread may be waiting for that mutex.
    bool ok = !guidanceHook.installed() || guidanceHook.restore();
    ok = (!engineHook.installed() || engineHook.restore()) && ok;
    ok = (!wcseHook.installed() || wcseHook.restore()) && ok;
    ok = (!chaffHook.installed() || chaffHook.restore()) && ok;
    ok = terminalHook.restore() && ok;
    ok = loftHook.restore() && ok;
    for (int i = 0; i < timerCount; ++i) ok = timers[i].restore() && ok;
    Guard guard;
    restored = ok; status = ok ? 0 : -46;
    if (ok) {
        identityCount = 0; timerCount = 0; states.clear(); holdRecords.clear(); recoveryRecords.clear();
        holdStatus = 0; chaffStatus = 0; recoveryStatus = 0;
    }
    else messages.emplace_back("ERROR MALICE energy: cleanup check failed");
}
static int start(void* malice, void* stock) {
    if (status != 0) return status;
    if (!nativeSupported) return status = -34;
    weaponBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"WeaponBlocks.dll"));
    const auto bs = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"blocksim.dll"));
    int planned = 0;
    const int result = f23radar::planEnergy(malice, stock, weaponBase, bs, identities, planned, accessible);
    if (result != 1) return status = result;
    if (!lua.bind()) return status = -35;
    // Prepare both controller functions before any native behavior changes.
    for (int i = 0; i < planned; ++i) {
        auto desc = identities[i].controller;
        auto state = field<lua_State*>(desc, 0xae0);
        for (size_t offset : {size_t(0xd8), size_t(0xcc8)}) {
            auto other = f23radar::schemeBlock(stock, offset, "controller", accessible);
            if (accessible(other, 0xae8) && field<lua_State*>(other, 0xae0) == state) { stop(); return status = -33; }
        }
        bool duplicate = false;
        for (int k = 0; k < timerCount; ++k) if (timers[k].state == state) duplicate = true;
        if (!duplicate) {
            auto& timer = timers[timerCount++];
            if (!timer.prepare(desc)) { stop(); return status = -36; }
        }
    }
    const unsigned char gpEntry[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55};
    const unsigned char motorEntry[] = {0x40,0x53,0x48,0x83,0xec,0x30,0x0f,0x29,0x74,0x24,0x20};
    const unsigned char switchEntry[] = {0xe9,0x2b,0x90,0xff,0xff};
    const auto check = [&](uintptr_t offset, const unsigned char* bytes, size_t n) {
        return accessible(reinterpret_cast<void*>(weaponBase+offset), n)
            && !std::memcmp(reinterpret_cast<void*>(weaponBase+offset), bytes, n);
    };
    nativeGuidance = reinterpret_cast<Simulate>(weaponBase+f23radar::guidanceSimulate);
    nativeEngine = reinterpret_cast<Simulate>(weaponBase+f23radar::engineSimulate);
    nativeSwitch = reinterpret_cast<Switch>(weaponBase+f23radar::engineSwitch);
    if (!check(f23radar::guidanceSimulate, gpEntry, sizeof(gpEntry))
        || !check(f23radar::engineSimulate, motorEntry, sizeof(motorEntry))
        || !check(f23radar::engineSwitch, switchEntry, sizeof(switchEntry))
        || !guidanceHook.prepare(reinterpret_cast<uintptr_t*>(weaponBase+f23radar::guidanceVtable+0x18),
            weaponBase+f23radar::guidanceSimulate, reinterpret_cast<void*>(&guidance))
        || !engineHook.prepare(reinterpret_cast<uintptr_t*>(weaponBase+f23radar::engineVtable+0x18),
            weaponBase+f23radar::engineSimulate, reinterpret_cast<void*>(&engine))
        || !loftHook.prepare(weaponBase, reinterpret_cast<void*>(&loft))) { stop(); return status = -37; }
    nativeLoft = reinterpret_cast<Simulate>(loftHook.trampoline());
    if (!guidanceHook.install() || !engineHook.install() || !loftHook.install()) { stop(); return status = -38; }
    for (int i = 0; i < timerCount; ++i) if (!timers[i].apply()) { stop(); return status = -39; }
    // Support hold: optional. A failure here refuses every hold and keeps the energy profile active.
    const unsigned char wcseEntry[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x70,0x20,0x55};
    bool datalinkKnown = false;
    for (int i = 0; i < planned; ++i) datalinkKnown = datalinkKnown || identities[i].wcse;
    nativeWcse = reinterpret_cast<Simulate>(weaponBase+f23radar::wcseSimulate);
    holdStatus = !datalinkKnown ? -47
        : !check(f23radar::wcseSimulate, wcseEntry, sizeof(wcseEntry))
          || !wcseHook.prepare(reinterpret_cast<uintptr_t*>(weaponBase+f23radar::wcseVtable+0x18),
              weaponBase+f23radar::wcseSimulate, reinterpret_cast<void*>(&wcse)) ? -48
        : !wcseHook.install() ? -49 : 1;
    startRecovery(malice, stock, planned);
    Guard guard;
    identityCount = planned; states.clear(); holdRecords.clear(); actions = {}; stockPasses = 0;
    recoveryRecords.clear(); chaffRejected = 0; recoveryFrames = 0;
    physicsThread = 0; timerRestoreThread = 0; threadMismatches = 0;
    active = true; restored = false; status = 1;
    if (holdStatus != 1) {
        char line[128];
        std::snprintf(line, sizeof(line), "ERROR MALICE support hold unavailable (code %d); energy profile unchanged", holdStatus);
        messages.emplace_back(line);
    }
    return status;
}
static std::string diagnostics() {
    Guard guard; char line[800];
    std::snprintf(line, sizeof(line), "status=%d active=%d pulse_actions=%llu stock_actions=%llu stock_passes=%llu guidance_hook=%d engine_hook=%d restored=%d timer_apply_thread=%lu timer_restore_thread=%lu physics_thread=%lu thread_mismatches=%llu hold_status=%d datalink_hook=%d holds=%llu chaff_status=%d chaff_hook=%d chaff_rejected=%llu recovery_status=%d recovery_hook=%d fallback_frames=%llu",
        status, active, actions.malice, actions.stock, stockPasses, guidanceHook.installed(), engineHook.installed(), restored,
        static_cast<unsigned long>(timerApplyThread), static_cast<unsigned long>(timerRestoreThread),
        static_cast<unsigned long>(physicsThread), threadMismatches, holdStatus, wcseHook.installed(), supportHolds.load(),
        chaffStatus, chaffHook.installed(), chaffRejected, recoveryStatus, terminalHook.installed(), recoveryFrames);
    return line;
}
static bool nextMessage(std::string& message) {
    Guard guard;
    if (messages.empty()) return false;
    message = messages.front(); messages.pop_front(); return true;
}
}
