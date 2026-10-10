// SPDX-License-Identifier: MIT
// Per-cockpit APG-73 candidate for DCS 2.9.30.28738.
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <atomic>
#include "RadarTune.h"
#include "NativeTarget.h"
#include "NativeRanking.h"
#include "RankingHook.h"
#include "SupportHook.h"
#include "MissileProfile.h"
#include "SupportDiag.h"
#include "PayloadAlias.h"
#include "PayloadAccessHook.h"
#include "SaRangeHook.h"
#include <tlhelp32.h>
#include <array>

struct lua_State;
using LuaFunction = int(*)(lua_State*);
static void (*pushNumber)(lua_State*, double);
static void (*pushFunction)(lua_State*, LuaFunction, int);
static int (*toBoolean)(lua_State*, int);
static void* (*toUserdata)(lua_State*, int);
static void (*pushString)(lua_State*, const char*);
static void (*pushNil)(lua_State*);
static double (*modelTime)();          // World.dll wTime::GetModelTime, the clock native MSI aging uses
static f23radar::SupportDiagTable supportDiag;
static std::atomic<void*> rankingOwner{nullptr};
static std::atomic<int> rankingResult{0};
static f23radar::RankingHook rankingHook;
static std::atomic<f23radar::RankFunction> nativeRank{nullptr};
static unsigned generation = 0;
static f23radar::Tune tune;
static bool supported = false;
static int (*holderCoalition)();
static int (*objectCoalition)(const void*);
static void** registryAddress;
static f23radar::SlotHook supportHook;
static f23radar::TrackingFunction nativeTrackingFunction = nullptr;
static bool supportInstalled = false;
static std::atomic<void*> supportRadar{nullptr};
static std::atomic<void*> supportMc{nullptr};
static std::atomic<unsigned long long> supportGrants{0};
static std::atomic<unsigned> supportLast{0};
static f23radar::ProfilePatch profilePatches[f23radar::profilePatchCapacity];
static int profileCount = 0;
static int profileStatus = 0;    // 1 active; 0 not yet found; negative refused
static int profileAttempts = 0;
static bool weaponsSupported = false;
// Independent weapons: payload alias for the Hornet, project data for Hornet lookups.
using PayloadTypeFunction = const void*(*)(void*, int);
using SetMissileDataFunction = void(*)(void*, const void*);
using DescriptorByTypeFunction = void*(*)(const void*);
using SidewinderNewFunction = void*(*)(void*, const void*);
static f23radar::AliasPair aliasPairs[2] = {{0, f23radar::stockAim120B}, {0, f23radar::stockAim9X}};
static void* (*currentPayload)() = nullptr;   // CockpitBase c_payload, the unpatched export
static f23radar::PayloadAccessHook payloadExportHook;
static f23radar::SlotHook stationTypeHook, containerTypeHook, missileDataHook, descriptorHook, sidewinderHook,
    payloadHook;
static PayloadTypeFunction stockStationType = nullptr, stockContainerType = nullptr;
static SetMissileDataFunction stockSetMissileData = nullptr;
static DescriptorByTypeFunction stockDescriptorByType = nullptr;
static SidewinderNewFunction stockSidewinderNew = nullptr;
static f23radar::WsType aliasStation[64], aliasContainer[64], projectMissileData, projectDescriptor, projectSeeker;
struct LaunchVector { float x, y, z; };
using LaunchZoneFunction = void(*)(void*, float, float, float, float,
    const LaunchVector&, const LaunchVector&, const LaunchVector&, float&, float&, float&);
static LaunchZoneFunction stockLaunchZone = nullptr;
static f23radar::SlotHook launchZoneHook;
static int launchZoneStatus = 0;
static void** globalInfoAddress = nullptr;
static SRWLOCK launchLogLock = SRWLOCK_INIT;
static void* (*getWideDevice)(void*, unsigned) = nullptr;
static char launchLine[2048]{};
static double launchLogTime = -1e9;
// 1 active; 2 waiting for FA18C.dll; 3 FA18C imports patched, payload vtable not seen yet; 0 not attempted;
// negative refused (see early_install).
static std::atomic<int> aliasStatus{0};

static bool file_matches(const wchar_t* path, const char* expected) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    unsigned char buffer[65536], digest[32]{};
    DWORD size{};
    while (ok) {
        if (!ReadFile(file, buffer, sizeof(buffer), &size, nullptr)) { ok = false; break; }
        if (!size) break;
        ok = BCryptHashData(hash, buffer, size, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    std::string hex;
    for (auto byte : digest) { hex += "0123456789abcdef"[byte >> 4]; hex += "0123456789abcdef"[byte & 15]; }
    return ok && hex == expected;
}
static bool matches(HMODULE module, const char* expected) {
    wchar_t path[32768]{};
    if (!module || !GetModuleFileNameW(module, path, 32768)) return false;
    return file_matches(path, expected);
}
static bool accessible(const void* p, size_t size, bool write = false) {
    const auto first = reinterpret_cast<uintptr_t>(p);
    if (!first || first + size < first) return false;
    auto cursor = first;
    while (cursor < first + size) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info))
            || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        if (write && !(info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
        auto next = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}
template<class T> static T field(const void* p, size_t offset) {
    T out; std::memcpy(&out, static_cast<const char*>(p) + offset, sizeof(out)); return out;
}
template<class T> static T symbol(HMODULE module, const char* name) {
    auto address=GetProcAddress(module,name); T out{};
    static_assert(sizeof(out)==sizeof(address)); std::memcpy(&out,&address,sizeof(out)); return out;
}
template<class T> static T method(const void* object, size_t offset) {
    if (!accessible(object,8)) return nullptr;
    auto table=field<void*>(object,0);
    if (!accessible(table,offset+8)) return nullptr;
    auto address=field<void*>(table,offset);
    if (!accessible(address,1)) return nullptr;
    T out; std::memcpy(&out,&address,sizeof(out));return out;
}
static int object_side(unsigned id) {
    if (!holderCoalition || !objectCoalition || !accessible(registryAddress,8)) return -1;
    if (!id) return 0;
    auto registry=field<void*>(registryAddress,0);
    // RegisterManager::instance and its lookup are the SceneObject constructor path.
    auto lookup=method<void*(*)(void*,unsigned)>(registry,0x10);
    if (!lookup) return -1;
    auto registered=lookup(registry,id);
    auto object=moving_object(registered,[](const void* p,size_t n){ return accessible(p,n); });
    if (!object) return -1;
    const int own=holderCoalition(), other=objectCoalition(object);
    if (own<1 || own>2 || other<0 || other>2) return -1;
    // Project rule: skip same-coalition and neutral targets. This uses the
    // object's simulation coalition, not a fabricated native IFF result.
    return other==own || other==0 ? 1 : 2;
}
static int target_side(void* responder, unsigned& id) {
    auto target=method<unsigned(*)(void*)>(responder,0x10);
    if (!target) return -1;
    id=target(responder);
    return object_side(id);
}
// Use the same aircraft-frame handoff as the working independent IR controller.
// The native Hornet seeker still decides whether it has an infrared lock.
static bool target_angles(void* context, unsigned id, double& az, double& el) {
    if (!id || !accessible(context, 0x30)) return false;
    auto objects = GetModuleHandleW(L"edObjects.dll");
    auto construct = symbol<void*(*)(void*, unsigned)>(objects, "??0SceneObject@@QEAA@I@Z");
    auto destroy = symbol<void(*)(void*)>(objects, "??1SceneObject@@UEAA@XZ");
    auto valid = symbol<bool(*)(const void*)>(objects, "??BSceneObject@@QEBA_NXZ");
    auto position = symbol<double*(*)(const void*, double*)>(objects,
        "?getObjectPosition@SceneObject@@UEBA?AVMatrixd@osg@@XZ");
    if (!construct || !destroy || !valid || !position) return false;
    alignas(16) unsigned char object[64]{};
    construct(object, id);
    if (!valid(object)) { destroy(object); return false; }
    double matrix[16]{};
    position(object, matrix);
    destroy(object);
    auto planeLink = field<char*>(context, 0x28);
    if (reinterpret_cast<uintptr_t>(planeLink) < 0x100) return false;
    auto plane = planeLink - 0x100;
    auto frame = method<const float*(*)(void*)>(plane, 0x148);
    if (!frame) return false;
    auto axes = frame(plane);
    if (!accessible(axes, 16 * sizeof(float))) return false;
    double delta[3] = {matrix[12] - axes[12], matrix[13] - axes[13],
                       matrix[14] - axes[14]};
    double local[3]{};
    for (int row = 0; row < 3; ++row)
        for (int axis = 0; axis < 3; ++axis)
            local[row] += axes[4 * row + axis] * delta[axis];
    az = -std::atan2(local[2], local[0]);
    el = std::atan2(local[1], std::hypot(local[0], local[2]));
    return std::isfinite(az) && std::isfinite(el);
}
// Primary-cockpit reader. It never changes radar tuning or ranking ownership.
static int primary_target(lua_State* L) {
    unsigned id = 0;
    int side = -1;
    double az = 0, el = 0;
    bool valid = false;
    auto contexts = GetProcAddress(GetModuleHandleW(L"CockpitBase.dll"),
        "?contexts_ptr@ccCockpitContext@cockpit@@0PAPEAV12@A");
    auto module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"FA18C.dll"));
    if (module && accessible(reinterpret_cast<void*>(contexts), sizeof(void*))) {
        auto context = field<void*>(reinterpret_cast<void*>(contexts), 0);
        if (accessible(context, 0x108)) {
            auto responder = field<uintptr_t>(context, 0x100);
            auto radar = reinterpret_cast<void*>(responder >= 0xb8 ? responder - 0xb8 : 0);
            if (accessible(radar, f23radar::radarSize)
                && field<uintptr_t>(radar, 0) == module + 0x6359e8
                && field<void*>(radar, 0x18) == context) {
                side = target_side(reinterpret_cast<void*>(responder), id);
                if (side == 2) valid = target_angles(context, id, az, el);
            }
        }
    }
    pushNumber(L, id);
    pushNumber(L, side);
    pushNumber(L, valid ? az : 0);
    pushNumber(L, valid ? el : 0);
    pushNumber(L, valid ? 1 : 0);
    return 5;
}
// Native answer first. The extension applies only to the F-23B radar, only behind the
// native power/operate/inhibit gates, and only while the target is an MSI trackfile.
// Diagnostics record each F-23B decision read-only; they do not change the answer.
static bool supported_tracking(void* responder, unsigned target, unsigned type) {
    const bool native = nativeTrackingFunction(responder, target, type);
    if (!target) return native;
    auto radar = supportRadar.load();
    if (!radar || responder != static_cast<char*>(radar) + f23radar::responderOffset) return native;
    auto mc = supportMc.load();
    const char* record = f23radar::msiRecord(mc, radar, target, accessible);
    auto note = [&](int decision) {
        if (modelTime) supportDiag.note(target, decision, f23radar::trackSnapshot(record), modelTime());
    };
    if (native) { note(f23radar::NativeSupport); return true; }
    if (!f23radar::nativeGatesOpen(responder, accessible)) { note(f23radar::GatesClosed); return false; }
    if (!record) {
        note(f23radar::linkedToRadar(mc, radar, accessible) ? f23radar::NoTrackfile : f23radar::NotLinked);
        return false;
    }
    supportGrants.fetch_add(1);
    supportLast.store(target);
    note(f23radar::ExtensionSupport);
    return true;
}
// Project weapons are independent types registered by the F-23B Core module. Classify each
// missile type from its exact database resource name. The native resource map
// includes both legacy Block II and new-scheme MALICE descriptors.
struct NativeStringView { const char* data; size_t length; };
// MSVC returns this 16-byte value through an explicit first result pointer.
static NativeStringView* (*weaponResourceName)(NativeStringView*, const void*) = nullptr;
static char classifiedName[96] = "none";
static uint16_t classifiedType[64];
static unsigned char classifiedKind[64];
static int classifiedCount = 0;
static SRWLOCK classifyLock = SRWLOCK_INIT;
struct ExclusiveLock {
    SRWLOCK* lock;
    explicit ExclusiveLock(SRWLOCK* value) : lock(value) { AcquireSRWLockExclusive(lock); }
    ~ExclusiveLock() { ReleaseSRWLockExclusive(lock); }
};
static int classify(const f23radar::WsType& type) {
    if (!f23radar::airToAirMissile(type) || type.level4 == f23radar::stockAim120B
        || type.level4 == f23radar::stockAim9X || type.level4 == 106) return 0;
    ExclusiveLock guard(&classifyLock);
    for (int i = 0; i < classifiedCount; ++i)
        if (classifiedType[i] == type.level4) return classifiedKind[i];
    int kind = 0;
    NativeStringView name{};
    if (weaponResourceName) weaponResourceName(&name, &type);
    if (name.length > 0 && name.length < sizeof(classifiedName) && accessible(name.data, name.length)) {
        kind = f23radar::projectWeaponKind(name.data, name.length);
        std::memcpy(classifiedName, name.data, name.length);
        classifiedName[name.length] = 0;
    }
    if (classifiedCount < 64) {
        classifiedType[classifiedCount] = type.level4;
        classifiedKind[classifiedCount++] = static_cast<unsigned char>(kind);
    }
    if (kind && !aliasPairs[kind - 1].project) aliasPairs[kind - 1].project = type.level4;
    return kind;
}
// Which module asks. Only FA18C code sees the alias; other callers get the real project types.
static std::atomic<uintptr_t> hornetBase{0}, hornetSize{0}, cockpitBase{0}, cockpitSize{0};
enum { CallerHornet, CallerCockpit, CallerOther };
static int caller_kind(uintptr_t address) {
    if (f23radar::insideImage(address, hornetBase.load(), hornetSize.load())) return CallerHornet;
    if (f23radar::insideImage(address, cockpitBase.load(), cockpitSize.load())) return CallerCockpit;
    return CallerOther;
}
// Read-only alias diagnostics: calls and aliased answers per caller kind, and the last queried type.
static std::atomic<unsigned> aliasCalls[3], aliasHits[3];
static std::atomic<unsigned> aliasLastType{0};
static const void* alias_type(void* self, int station, const void* type, f23radar::WsType* buffer, int kind) {
    aliasCalls[kind].fetch_add(1);
    if (!type || !currentPayload || self != currentPayload()
        || !accessible(type, sizeof(f23radar::WsType))) return type;
    f23radar::WsType value; std::memcpy(&value, type, sizeof(value));
    aliasLastType.store((unsigned(value.level1) << 28) | (unsigned(value.level2) << 24)
        | (unsigned(value.level3) << 16) | value.level4);
    classify(value);
    if (kind != CallerHornet || station < 0 || station >= 64) return type;
    const auto stock = f23radar::stockFor(value, aliasPairs, 2);
    if (!stock) return type;
    aliasHits[kind].fetch_add(1);
    buffer[station] = f23radar::withLevel4(type, stock);
    return &buffer[station];
}
// Hornet view of the F-23B payload: project missiles report their stock Hornet type.
__attribute__((noinline)) static const void* alias_station_type(void* self, int station) {
    const int kind = caller_kind(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    return alias_type(self, station, stockStationType(self, station), aliasStation, kind);
}
__attribute__((noinline)) static const void* alias_container_type(void* self, int station) {
    const int kind = caller_kind(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    return alias_type(self, station, stockContainerType(self, station), aliasContainer, kind);
}
// Which project missiles the current payload carries. It mirrors the Hornet's own station loop
// (FA18C RVA 0x31b600): stations 0 .. count-2, skipping an empty station.
static void scan_payload(bool (&found)[2]) {
    found[0] = found[1] = false;
    auto payload = currentPayload ? currentPayload() : nullptr;
    if (!payload || !stockStationType || !stockContainerType) return;
    auto stationCount = method<int(*)(void*)>(payload, f23radar::payloadStationCountSlot);
    auto weaponCount = method<int(*)(void*, int)>(payload, f23radar::payloadWeaponCountSlot);
    if (!stationCount || !weaponCount) return;
    const int stations = stationCount(payload);
    for (int station = 0; station < stations - 1 && station < 64; ++station) {
        if (weaponCount(payload, station) <= 0) continue;
        for (auto read : {stockStationType, stockContainerType}) {
            const void* type = read(payload, station);
            if (!type || !accessible(type, sizeof(f23radar::WsType))) continue;
            f23radar::WsType value; std::memcpy(&value, type, sizeof(value));
            classify(value);
            for (int i = 0; i < 2; ++i)
                if (aliasPairs[i].project && f23radar::airToAirMissile(value) && value.level4 == aliasPairs[i].project)
                    found[i] = true;
        }
    }
}
// Hornet data lookups: an aliased stock type reads the project's own weapon data, but only while the
// current payload carries that project missile. A stock Hornet flown later keeps its own data.
static const void* project_type(const void* type, f23radar::WsType& buffer) {
    if (!type || !accessible(type, sizeof(f23radar::WsType))) return type;
    f23radar::WsType value; std::memcpy(&value, type, sizeof(value));
    if (!f23radar::airToAirMissile(value)) return type;
    const int index = value.level4 == f23radar::stockAim120B ? 0 : value.level4 == f23radar::stockAim9X ? 1 : -1;
    if (index < 0) return type;
    bool found[2];
    scan_payload(found);
    if (!found[index]) return type;
    buffer = f23radar::withLevel4(type, aliasPairs[index].project);
    return &buffer;
}
static bool selected_weapon(void* mc, f23radar::WsType& result, int& station) {
    auto payload = currentPayload ? currentPayload() : nullptr;
    if (!payload || !stockStationType || !stockContainerType || !getWideDevice
        || !accessible(mc, 0x20)) return false;
    auto context = field<void*>(mc, 0x18);
    if (!accessible(context, 0x40)) return false;
    // devices.lua registers F18::avArmamentControl_AYK22_F18 as SMS device 23.
    auto sms = getWideDevice(context, 23);
    station = f23radar::amraamStation(sms, context, hornetBase.load(), accessible);
    if (station < 0) return false;
    auto count = method<int(*)(void*)>(payload, f23radar::payloadStationCountSlot);
    if (!count || station >= count(payload) - 1) return false;
    for (auto read : {stockContainerType, stockStationType}) {
        const void* raw = read(payload, station);
        if (!accessible(raw, sizeof(result))) continue;
        const auto value = field<f23radar::WsType>(raw, 0);
        if (!f23radar::airToAirMissile(value)) continue;
        result = value;
        return true;
    }
    return false;
}
static void alias_set_missile_data(void* self, const void* type) {
    stockSetMissileData(self, project_type(type, projectMissileData));
}
static void* alias_descriptor_by_type(const void* type) {
    return stockDescriptorByType(project_type(type, projectDescriptor));
}
static void* alias_sidewinder_new(void* self, const void* type) {
    return stockSidewinderNew(self, project_type(type, projectSeeker));
}
static void* rocket_constant(uint16_t level4) {
    if (!level4 || !accessible(globalInfoAddress, sizeof(void*))) return nullptr;
    auto info = field<void*>(globalInfoAddress, 0);
    auto get = method<void*(*)(void*, const f23radar::WsType*)>(info, 0x30);
    const f23radar::WsType type{4,4,7,0,level4,0};
    return get ? get(info, &type) : nullptr;
}
static void format_constant(char* output, size_t size, void* value) {
    if (!accessible(value, 0x278)) { std::snprintf(output, size, "unreadable"); return; }
    size_t used = 0;
    for (size_t offset = 0x10c; offset <= 0x274; offset = offset == 0x10c ? 0x248 : offset + 4) {
        const int wrote = std::snprintf(output + used, size - used, "%s%zx=%.9g",
            used ? "," : "", offset, double(field<float>(value, offset)));
        if (wrote < 0 || static_cast<size_t>(wrote) >= size - used) break;
        used += static_cast<size_t>(wrote);
    }
}
static void alias_launch_zone(void* input, float altitude, float a3, float a4, float deltaAltitude,
    const LaunchVector& own, const LaunchVector& target, const LaunchVector& relative,
    float& maximum, float& noEscape, float& aero) {
    auto radar = supportRadar.load();
    const auto output = reinterpret_cast<uintptr_t>(&maximum);
    auto mc = reinterpret_cast<void*>(output >= 0x3fc0 ? output - 0x3fc0 : 0);
    const bool owner = reinterpret_cast<uintptr_t>(&noEscape) == output - 8
        && reinterpret_cast<uintptr_t>(&aero) == output - 4
        && f23radar::linkedToRadar(mc, radar, accessible);
    f23radar::WsType selected{};
    int station = -1;
    const bool selectedKnown = owner && selected_weapon(mc, selected, station);
    void* stock = owner ? rocket_constant(f23radar::stockAim120B) : nullptr;
    void* project = owner ? rocket_constant(aliasPairs[0].project) : nullptr;
    void* chosen = f23radar::launchZoneConstant(input, stock, project, selected,
        aliasPairs[0].project, owner && selectedKnown && accessible(project, 0x278));
    stockLaunchZone(chosen, altitude, a3, a4, deltaAltitude, own, target, relative, maximum, noEscape, aero);
    if (!owner || !modelTime) return;
    const double now = modelTime();
    ExclusiveLock guard(&launchLogLock);
    if (now >= launchLogTime && now - launchLogTime < 2.0) return;
    launchLogTime = now;
    char stockFields[480]{}, projectFields[480]{};
    format_constant(stockFields, sizeof(stockFields), stock);
    format_constant(projectFields, sizeof(projectFields), project);
    std::snprintf(launchLine, sizeof(launchLine),
        "time=%.3f status=%d station=%d selected=%u substituted=%d input=%p stock=%p malice=%p "
        "stock_fields=[%s] malice_fields=[%s] altitude=%.3f delta_alt=%.3f "
        "own=[%.3f,%.3f,%.3f] target=[%.3f,%.3f,%.3f] relative=[%.3f,%.3f,%.3f] outputs=[%.3f,%.3f,%.3f]",
        now, launchZoneStatus, station, unsigned(selected.level4), chosen != input,
        input, stock, project, stockFields, projectFields, double(altitude), double(deltaAltitude),
        double(own.x), double(own.y), double(own.z), double(target.x), double(target.y), double(target.z),
        double(relative.x), double(relative.y), double(relative.z), double(maximum), double(noEscape), double(aero));
}
// SA scale limit: 0 not attempted, 1 extended to 640 NM, -50 unknown FA18C bytes, -51 write refused.
static int saRangeStatus = 0;
// Returns 1 when the requested limit is in place, -50 for unknown bytes, -51 if the write failed.
static int sa_range_apply(bool extended) {
    auto base = reinterpret_cast<unsigned char*>(GetModuleHandleW(L"FA18C.dll"));
    if (!base || !accessible(base + f23radar::saRangeStockConstant, 8)
        || !accessible(base + f23radar::saRangeExtendedConstant, 8)) return -50;
    for (const auto& p : f23radar::saRangePatches)
        if (!accessible(base + p.rva, p.valueOffset + p.valueSize)) return -50;
    const int state = f23radar::saRangeState(base);
    if (state == (extended ? 2 : 1)) return 1;
    if (state != (extended ? 1 : 2)) return -50;
    // Pause other threads: two of the values are not aligned, so the writes are not atomic.
    std::array<HANDLE, 2048> handles{};
    size_t count = 0, suspended = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return -51;
    THREADENTRY32 item{}; item.dwSize = sizeof(item);
    bool ok = Thread32First(snapshot, &item) != FALSE;
    const auto process = GetCurrentProcessId(), self = GetCurrentThreadId();
    if (ok) do {
        if (item.th32OwnerProcessID != process || item.th32ThreadID == self) continue;
        if (count == handles.size()) { ok = false; break; }
        HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, item.th32ThreadID);
        if (!thread) { ok = false; break; }
        handles[count++] = thread;
    } while (Thread32Next(snapshot, &item));
    CloseHandle(snapshot);
    if (ok) for (; suspended < count; ++suspended)
        if (SuspendThread(handles[suspended]) == DWORD(-1)) { ok = false; break; }
    if (ok) for (size_t i = 0; i < suspended; ++i) {
        CONTEXT context{}; context.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(handles[i], &context)) { ok = false; break; }
        for (const auto& p : f23radar::saRangePatches) {
            const auto start = reinterpret_cast<uintptr_t>(base) + p.rva;
            if (context.Rip > start && context.Rip < start + p.valueOffset + p.valueSize) ok = false;
        }
    }
    std::array<DWORD, 3> protect{};
    size_t opened = 0;
    for (; ok && opened < protect.size(); ++opened) {
        const auto& p = f23radar::saRangePatches[opened];
        ok = VirtualProtect(base + p.rva, p.valueOffset + p.valueSize, PAGE_EXECUTE_READWRITE, &protect[opened]) != FALSE;
        if (!ok) break;
    }
    if (ok) f23radar::saRangeWrite(base, extended);
    for (size_t i = 0; i < opened; ++i) {
        const auto& p = f23radar::saRangePatches[i];
        DWORD unused = 0;
        ok = VirtualProtect(base + p.rva, p.valueOffset + p.valueSize, protect[i], &unused) != FALSE && ok;
        ok = FlushInstructionCache(GetCurrentProcess(), base + p.rva, p.valueOffset + p.valueSize) != FALSE && ok;
    }
    while (suspended) ResumeThread(handles[--suspended]);
    for (size_t i = 0; i < count; ++i) CloseHandle(handles[i]);
    return ok && f23radar::saRangeState(base) == (extended ? 2 : 1) ? 1 : -51;
}
static void install_launch_zone() {
    auto runtime = GetModuleHandleW(L"Weapons.dll");
    const auto base = hornetBase.load();
    launchZoneStatus = -40;
    if (!base || !matches(runtime, "6e3a4cb16c0c89cb7bc266d4f275d4e8659ab8e385464bb5d377832d7a2fa614")) return;
    stockLaunchZone = symbol<LaunchZoneFunction>(runtime,
        "?DLZ@@YAXPEAURocket_Const@@MMMMAEBVVec3f@osg@@11AEAM22@Z");
    globalInfoAddress = reinterpret_cast<void**>(GetProcAddress(GetModuleHandleW(L"WorldGeneral.dll"),
        "?globalInfo@@3PEAVIwInfo@@EA"));
    getWideDevice = symbol<decltype(getWideDevice)>(GetModuleHandleW(L"CockpitBase.dll"),
        "?get_wide_device@ccCockpitContext@cockpit@@QEAAPEAVavDevice@2@I@Z");
    auto slot = reinterpret_cast<uintptr_t*>(base + f23radar::importLaunchZone);
    if (!stockLaunchZone || !globalInfoAddress || !getWideDevice || !accessible(slot, 8)) return;
    launchZoneStatus = -41;
    if (!launchZoneHook.prepare(slot, reinterpret_cast<uintptr_t>(stockLaunchZone),
            reinterpret_cast<void*>(&alias_launch_zone)) || !launchZoneHook.install()) return;
    launchZoneStatus = 1;
}
// The Hornet asks c_payload() before its first station read. The patched import answers as the
// original does, and on the first sight of a payload class it puts the alias into that class's vtable.
static SRWLOCK payloadLock = SRWLOCK_INIT;
static std::atomic<void*> payloadSeen{nullptr};
static uintptr_t* patchedTable = nullptr;
static char vtableNote[200] = "payload vtable not seen";
static void import_refused(uintptr_t rva, uintptr_t actual, uintptr_t expected) {
    HMODULE owner{};
    wchar_t path[MAX_PATH]{};
    char name[64] = "unknown";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(actual), &owner) && GetModuleFileNameW(owner, path, MAX_PATH)) {
        const wchar_t* base = std::wcsrchr(path, L'\\');
        WideCharToMultiByte(CP_UTF8, 0, base ? base + 1 : path, -1, name, sizeof(name) - 1, nullptr, nullptr);
    }
    std::snprintf(vtableNote, sizeof(vtableNote), "import +%llx refused: actual=%llx (%s+%llx) expected=%llx",
        static_cast<unsigned long long>(rva), static_cast<unsigned long long>(actual), name,
        static_cast<unsigned long long>(actual - reinterpret_cast<uintptr_t>(owner)),
        static_cast<unsigned long long>(expected));
}
static void describe_vtable(uintptr_t* table) {
    HMODULE owner{};
    wchar_t path[MAX_PATH]{};
    char name[96] = "unknown";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(table), &owner) && GetModuleFileNameW(owner, path, MAX_PATH)) {
        const wchar_t* base = std::wcsrchr(path, L'\\');
        WideCharToMultiByte(CP_UTF8, 0, base ? base + 1 : path, -1, name, sizeof(name) - 1, nullptr, nullptr);
    }
    std::snprintf(vtableNote, sizeof(vtableNote), "payload vtable %s+0x%llx", name,
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(table) - reinterpret_cast<uintptr_t>(owner)));
}
static void patch_payload(void* payload) {
    if (payload == payloadSeen.load() || !accessible(payload, 8)) return;
    ExclusiveLock guard(&payloadLock);
    payloadSeen.store(payload);
    auto table = field<uintptr_t*>(payload, 0);
    if (table == patchedTable) { if (aliasStatus.load() == 3) aliasStatus.store(1); return; }
    if (patchedTable) { std::snprintf(vtableNote, sizeof(vtableNote), "second payload class refused"); return; }
    auto stationSlot = table + f23radar::payloadStationTypeSlot / 8;
    auto containerSlot = table + f23radar::payloadContainerTypeSlot / 8;
    if (!accessible(stationSlot, 8) || !accessible(containerSlot, 8)
        || !accessible(reinterpret_cast<void*>(*stationSlot), 1)
        || !accessible(reinterpret_cast<void*>(*containerSlot), 1)) { aliasStatus.store(-22); return; }
    describe_vtable(table);
    stockStationType = reinterpret_cast<PayloadTypeFunction>(*stationSlot);
    stockContainerType = reinterpret_cast<PayloadTypeFunction>(*containerSlot);
    if (!stationTypeHook.prepare(stationSlot, *stationSlot, reinterpret_cast<void*>(&alias_station_type))
        || !containerTypeHook.prepare(containerSlot, *containerSlot, reinterpret_cast<void*>(&alias_container_type))
        || !stationTypeHook.install() || !containerTypeHook.install()) {
        stationTypeHook.restore(); containerTypeHook.restore();
        aliasStatus.store(-22);
        return;
    }
    patchedTable = table;
    aliasStatus.store(1);
}
static int ensure_hornet_imports();
__attribute__((noinline)) static void* hooked_c_payload() {
    const int kind = caller_kind(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    const bool ready = kind == CallerHornet && ensure_hornet_imports() > 0;
    void* payload = currentPayload();
    if (ready && payload) patch_payload(payload);
    return payload;
}
static size_t image_size(uintptr_t base) {
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!accessible(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (!accessible(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->OptionalHeader.SizeOfImage;
}
// Patch the four FA18C imports that matter: the payload accessor and the three weapon-data lookups.
// Codes: 3 installed; -20 no descriptor accessors; -23 import slot refused; -24 install failed.
static int install_hornet_imports(uintptr_t base) {
    if (!base || !currentPayload || !stockSetMissileData || !stockDescriptorByType || !stockSidewinderNew) return -20;
    auto slot = [&](uintptr_t rva) { return reinterpret_cast<uintptr_t*>(base + rva); };
    auto address = [](auto function) { return reinterpret_cast<uintptr_t>(function); };
    for (auto rva : {f23radar::importCurrentPayload, f23radar::importSetMissileData,
                     f23radar::importDescriptorByType, f23radar::importSidewinderNew})
        if (!accessible(slot(rva), 8)) return -23;
    const bool viaExport = *slot(f23radar::importCurrentPayload)
        == reinterpret_cast<uintptr_t>(payloadExportHook.address());
    for (auto pair : {std::pair<uintptr_t, uintptr_t>{f23radar::importCurrentPayload, address(currentPayload)},
                     {f23radar::importSetMissileData, address(stockSetMissileData)},
                     {f23radar::importDescriptorByType, address(stockDescriptorByType)},
                     {f23radar::importSidewinderNew, address(stockSidewinderNew)}}) {
        if (pair.first == f23radar::importCurrentPayload && viaExport) continue;
        if (*slot(pair.first) != pair.second) {
            import_refused(pair.first, *slot(pair.first), pair.second);
            return -23;
        }
    }
    if ((!viaExport && !payloadHook.prepare(slot(f23radar::importCurrentPayload), address(currentPayload),
            reinterpret_cast<void*>(&hooked_c_payload)))
        || !missileDataHook.prepare(slot(f23radar::importSetMissileData), address(stockSetMissileData),
            reinterpret_cast<void*>(&alias_set_missile_data))
        || !descriptorHook.prepare(slot(f23radar::importDescriptorByType), address(stockDescriptorByType),
            reinterpret_cast<void*>(&alias_descriptor_by_type))
        || !sidewinderHook.prepare(slot(f23radar::importSidewinderNew), address(stockSidewinderNew),
            reinterpret_cast<void*>(&alias_sidewinder_new))) return -23;
    // Project data first, then the payload accessor, so no aliased type reaches a stock lookup.
    if (!(missileDataHook.install() && descriptorHook.install() && sidewinderHook.install()
        && (viaExport || payloadHook.install()))) {
        payloadHook.restore(); missileDataHook.restore(); descriptorHook.restore(); sidewinderHook.restore();
        return -24;
    }
    return 3;
}
// The first native payload call occurs after import binding and before station
// enumeration. Install synchronously there, never from a loader callback.
static SRWLOCK importsLock = SRWLOCK_INIT;
static int ensure_hornet_imports() {
    int status = aliasStatus.load();
    if (status != 2) return status;
    ExclusiveLock guard(&importsLock);
    status = aliasStatus.load();
    if (status != 2) return status;
    const auto base = hornetBase.load();
    if (!base) return 0;
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(base), &pinned)) status = -26;
    else status = install_hornet_imports(base);
    aliasStatus.store(status);
    return status;
}
// FA18C is not loaded until its cockpit is created. A load notification runs before any FA18C code.
struct LdrString { USHORT length, maximum; PWSTR buffer; };
struct LdrNotification { ULONG flags; const LdrString* fullName; const LdrString* baseName; PVOID base; ULONG size; };
using LdrCallback = void(CALLBACK*)(ULONG, const LdrNotification*, PVOID);
using LdrRegister = LONG(NTAPI*)(ULONG, LdrCallback, PVOID, PVOID*);
static std::wstring hornetPath;      // verified FA18C.dll path, normalized; written before registration only
static const wchar_t* hornetPathData = nullptr;
static size_t hornetPathLength = 0;
static void* ldrCookie = nullptr;
static void CALLBACK hornet_notification(ULONG reason, const LdrNotification* data, PVOID) {
    if ((reason != 1 && reason != 2) || !data || !data->fullName || !data->fullName->buffer) return;
    if (!f23radar::loaderPathEquals(data->fullName->buffer, data->fullName->length / sizeof(wchar_t),
            hornetPathData, hornetPathLength)) return;
    if (reason == 1) {
        hornetSize.store(data->size);
        hornetBase.store(reinterpret_cast<uintptr_t>(data->base));
    } else {
        hornetBase.store(0); hornetSize.store(0);
        aliasStatus.store(2);
    }
}
static std::wstring expected_hornet_path() {
    wchar_t executable[2 * MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, executable, 2 * MAX_PATH)) return L"";
    std::wstring root(executable);
    for (int level = 0; level < 2; ++level) {      // ...\bin\DCS.exe -> ...\DCS World
        const auto cut = root.find_last_of(L"\\/");
        if (cut == std::wstring::npos) return L"";
        root.resize(cut);
    }
    return root + L"\\Mods\\aircraft\\FA-18C\\bin\\FA18C.dll";
}
// Codes: 1/2/3 as aliasStatus; -20 native accessors missing; -25 unsupported native build;
// -26 load notification unavailable.
static int early_install() {
    if (aliasStatus.load() != 0) return aliasStatus.load();
    auto cockpit = GetModuleHandleW(L"CockpitBase.dll"), weapons = GetModuleHandleW(L"WeaponsBase.dll");
    auto blocks = GetModuleHandleW(L"WeaponBlocks.dll"), weaponRuntime = GetModuleHandleW(L"Weapons.dll");
    if (!cockpit || !weapons || !blocks || !weaponRuntime) return -20;
    if (!matches(cockpit, "163d55167503da8dd0ea9eb459e17206fc3dc8d6b4805b5b13bbe58341beb794")
        || !matches(weapons, "a14d00c70d3cba0a3b129df9627280f2a5b01cb60d281484da4468c921d759df")
        || !matches(blocks, "e1be3d2bf0d8f3c2dd93925f386b162b96f1db7318e9b319b54a61b66072b83b")
        || !matches(weaponRuntime, "6e3a4cb16c0c89cb7bc266d4f275d4e8659ab8e385464bb5d377832d7a2fa614")) return -25;
    const auto path = expected_hornet_path();
    if (path.empty() || !file_matches(path.c_str(), "fa6e89b5e193b7faf11a9de16269bb9314472532cf447254cbf38e1e9b375f31"))
        return -25;
    weaponResourceName = symbol<decltype(weaponResourceName)>(weaponRuntime,
        "?weaponGetUniqueResourceName@@YA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@AEBVwsType@@@Z");
    currentPayload = symbol<void*(*)()>(cockpit, "?c_payload@cockpit@@YAPEAVIwHumanPayload@@XZ");
    stockSetMissileData = symbol<SetMissileDataFunction>(cockpit, "?setMissileData@MissileSight@cockpit@@QEAAXAEBVwsType@@@Z");
    stockDescriptorByType = symbol<DescriptorByTypeFunction>(weapons,
        "?wGetAmmunitionDescriptorByType@@YAPEAVwAmmunitionDescriptor@@VwsType@@@Z");
    stockSidewinderNew = symbol<SidewinderNewFunction>(cockpit, "??0eqSidewinderNew@cockpit@@QEAA@AEBVwsType@@@Z");
    if (!weaponResourceName || !currentPayload || !stockSetMissileData || !stockDescriptorByType
        || !stockSidewinderNew) return -20;
    auto ntdll = GetModuleHandleW(L"ntdll.dll");
    auto registerNotification = symbol<LdrRegister>(ntdll, "LdrRegisterDllNotification");
    HMODULE pinned{};
    if (!registerNotification
        || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&early_install), &pinned)) return -26;
    cockpitBase.store(reinterpret_cast<uintptr_t>(cockpit));
    cockpitSize.store(image_size(reinterpret_cast<uintptr_t>(cockpit)));
    hornetPath = f23radar::normalizePath(path.c_str(), path.size());
    hornetPathData = hornetPath.data(); hornetPathLength = hornetPath.size();
    aliasStatus.store(2);
    if (registerNotification(0, &hornet_notification, nullptr, &ldrCookie) < 0) { aliasStatus.store(-26); return -26; }
    // Future FA18C imports bind to this relay. The original accessor remains
    // callable and all other modules receive its unchanged result.
    if (!payloadExportHook.prepare(cockpit, "?c_payload@cockpit@@YAPEAVIwHumanPayload@@XZ",
            reinterpret_cast<void*>(currentPayload), reinterpret_cast<void*>(&hooked_c_payload))
        || !payloadExportHook.install()) { aliasStatus.store(-27); return -27; }
    // Patch now when the Hornet library is already loaded, for example after a stock Hornet flight.
    if (auto loaded = GetModuleHandleW(L"FA18C.dll")) {
        wchar_t loadedPath[32768]{};
        if (GetModuleFileNameW(loaded, loadedPath, 32768)
            && f23radar::normalizePath(loadedPath, std::wcslen(loadedPath)) == hornetPath)
        {
            hornetSize.store(image_size(reinterpret_cast<uintptr_t>(loaded)));
            hornetBase.store(reinterpret_cast<uintptr_t>(loaded));
            ensure_hornet_imports();
        }
        else aliasStatus.store(-25);
    }
    return aliasStatus.load();
}
#include "MaliceRuntime.h"

// MALICE long-flight profile: find the independent MALICE by name in the weapon database,
// prove identity by its 600 s sensor lifetime, refuse if the stock AIM-120C shares the INS
// descriptor or sensor, then widen MALICE's INS limits and narrow its seeker FOV.
static void try_profile() {
    if (!weaponsSupported || profileStatus == 1 || profileStatus < -1 || profileAttempts > 600) return;
    ++profileAttempts;
    auto lookup = symbol<void*(*)(const void*)>(GetModuleHandleW(L"WeaponsBase.dll"),
        "?wGetAmmunitionDescriptorByType@@YAPEAVwAmmunitionDescriptor@@VwsType@@@Z");
    if (!lookup) { profileStatus = -6; return; }
    auto access = [](const void* p, size_t n, bool w) { return accessible(p, n, w); };
    // wsType key (Weapons.dll 0x125111..0x125188): level1-3 bytes at +0..+2, level4 uint16 at +4.
    const unsigned char keyStock[8] = {4,4,7,0,106,0,0,0};
    {
        // The independent MALICE type is known once the Hornet has asked about an F-23B station.
        if (!aliasPairs[0].project) { --profileAttempts; return; }
        const f23radar::WsType keyMalice{4, 4, 7, 0, aliasPairs[0].project, 0};
        void* malice = lookup(&keyMalice);
        if (!malice || !f23radar::schemeBlock(malice, 0xd8, "sensor", access)) { profileStatus = -1; return; }
        void* stock = lookup(keyStock);
        if (!stock) stock = f23radar::findAmmunition(reinterpret_cast<const void*>(lookup), 106, access);
        int count = 0;
        const auto weaponBlocks = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"WeaponBlocks.dll"));
        const int plan = f23radar::planMaliceProfile(malice, stock, weaponBlocks, profilePatches, count, access);
        if (plan == 1) {
            malice_energy::start(malice, stock);
            profileCount = count; f23radar::applyProfile(profilePatches, count, true);
        }
        profileStatus = plan;
        return;
    }
    profileStatus = -1;
}
static void rank_tracks(void* mc) {
    // Remember the mission computer that feeds the F-23B radar; the MSI vector lives there.
    if (auto radar = supportRadar.load(); radar && f23radar::linkedToRadar(mc, radar, accessible))
        supportMc.store(mc);
    rankingResult.store(f23radar::rankWithoutFriendlies(mc, rankingOwner.load(), nativeRank.load(),
        accessible, object_side));
}
static int close_owner(lua_State* L) {
    auto token = static_cast<unsigned*>(toUserdata(L, 1));
    if (token && *token == generation) {
        rankingOwner.store(nullptr);
        supportRadar.store(nullptr);
        supportMc.store(nullptr);
        malice_energy::stop();
        launchZoneHook.restore();
        launchZoneStatus = 0;
        if (saRangeStatus == 1 && sa_range_apply(false) == 1) saRangeStatus = 0;
        rankingHook.restore(); // A failed removal leaves only an inert, pinned passthrough.
        supportHook.restore(); // A remaining slot passes through: no F-23B radar is registered.
        if (profileStatus == 1) f23radar::applyProfile(profilePatches, profileCount, false);
        profileStatus = 0; profileAttempts = 0; profileCount = 0;
        // The independent-weapon connection stays: it belongs to FA18C.dll, not to this cockpit's script state.
    }
    return 0;
}
static int update(lua_State* L) {
    int status = -1;
    unsigned targetID=0;
    int side=-1;
    double cueAz=0, cueEl=0;
    bool cueValid=false;
    void* owner = nullptr;
    const auto token = static_cast<unsigned*>(toUserdata(L, -10003)); // Lua 5.1 upvalue 1.
    if (!token || *token != generation) { pushNumber(L, -4); return 1; }
    if (supported) {
        auto module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"FA18C.dll"));
        auto contexts = GetProcAddress(GetModuleHandleW(L"CockpitBase.dll"),
            "?contexts_ptr@ccCockpitContext@cockpit@@0PAPEAV12@A");
        status = 0; // Cockpit not initialized yet.
        if (module && accessible(reinterpret_cast<void*>(contexts), sizeof(void*))) {
            auto context = field<void*>(reinterpret_cast<void*>(contexts), 0);
            if (accessible(context, 0x108)) {
                auto responder = field<uintptr_t>(context, 0x100);
                auto radar = reinterpret_cast<void*>(responder >= 0xb8 ? responder - 0xb8 : 0);
                if (accessible(radar, f23radar::radarSize, true)
                    && field<uintptr_t>(radar, 0) == module + 0x6359e8
                    && field<void*>(radar, 0x18) == context) {
                    if (supportRadar.exchange(radar) != radar) supportMc.store(nullptr);
                    side=target_side(reinterpret_cast<void*>(responder),targetID);
                    if (side == 2 && toBoolean(L, 2))
                        cueValid=target_angles(context,targetID,cueAz,cueEl);
                    auto model = field<void*>(radar, 0x46f0);
                    auto mode = field<void*>(radar, 0x4cc0);
                    status = -3;
                    if (accessible(model, f23radar::modelSize, true) && accessible(mode, 16)
                        && field<void*>(mode, 8) == radar) {
                        auto vtable = field<uintptr_t>(mode, 0);
                        bool search = vtable == module + 0x636578 || vtable == module + 0x636848;
                        status = tune.update(radar, model, search);
                        if (search && status >= 0 && toBoolean(L, 1)) owner = radar;
                    }
                }
            }
        }
    }
    rankingOwner.store(owner);
    pushNumber(L, status);
    pushNumber(L, targetID);
    pushNumber(L, side);
    pushNumber(L, rankingResult.load());
    pushNumber(L, cueValid ? cueAz : 0);
    pushNumber(L, cueValid ? cueEl : 0);
    pushNumber(L, cueValid ? 1 : 0);
    pushNumber(L, supportInstalled ? 1 : 0);
    pushNumber(L, static_cast<double>(supportGrants.load()));
    pushNumber(L, supportLast.load());
    try_profile();
    pushNumber(L, profileStatus);
    char line[256];
    if (modelTime && supportDiag.nextLine(modelTime(), line, sizeof(line))) pushString(L, line);
    else pushNil(L);
    pushNumber(L, aliasStatus.load());
    pushNumber(L, aliasPairs[0].project);
    pushNumber(L, aliasPairs[1].project);
    char aliasLine[540];
    std::snprintf(aliasLine, sizeof(aliasLine),
        "status=%d fa18=%u/%u cockpit=%u/%u other=%u/%u last_type=%08x classified=%d hornet=%llx; %s; resource=%s",
        aliasStatus.load(), aliasCalls[CallerHornet].load(), aliasHits[CallerHornet].load(),
        aliasCalls[CallerCockpit].load(), aliasHits[CallerCockpit].load(),
        aliasCalls[CallerOther].load(), aliasHits[CallerOther].load(), aliasLastType.load(), classifiedCount,
        static_cast<unsigned long long>(hornetBase.load()), vtableNote, classifiedName);
    pushString(L, aliasLine);
    {
        ExclusiveLock guard(&launchLogLock);
        if (launchLine[0]) { pushString(L, launchLine); launchLine[0] = 0; }
        else pushNil(L);
    }
    pushNumber(L, malice_energy::status);
    std::string energyMessage;
    if (malice_energy::nextMessage(energyMessage)) pushString(L, energyMessage.c_str());
    else pushNil(L);
    pushString(L, malice_energy::diagnostics().c_str());
    pushNumber(L, saRangeStatus);
    return 21;
}
extern "C" __declspec(dllexport) int luaopen_f23b_radar(lua_State* L) {
    auto lua = GetModuleHandleW(L"lua.dll");
    auto address = GetProcAddress(lua, "lua_pushnumber");
    std::memcpy(&pushNumber, &address, sizeof(pushNumber));
    address = GetProcAddress(lua, "lua_pushcclosure");
    std::memcpy(&pushFunction, &address, sizeof(pushFunction));
    toBoolean = symbol<int(*)(lua_State*,int)>(lua, "lua_toboolean");
    toUserdata = symbol<void*(*)(lua_State*,int)>(lua, "lua_touserdata");
    pushString = symbol<void(*)(lua_State*,const char*)>(lua, "lua_pushstring");
    pushNil = symbol<void(*)(lua_State*)>(lua, "lua_pushnil");
    modelTime = symbol<double(*)()>(GetModuleHandleW(L"World.dll"), "?GetModelTime@wTime@@SANXZ");
    auto newUserdata = symbol<void*(*)(lua_State*,size_t)>(lua, "lua_newuserdata");
    auto createTable = symbol<void(*)(lua_State*,int,int)>(lua, "lua_createtable");
    auto setField = symbol<void(*)(lua_State*,int,const char*)>(lua, "lua_setfield");
    auto setMetatable = symbol<int(*)(lua_State*,int)>(lua, "lua_setmetatable");
    if (!pushNumber || !pushFunction || !toBoolean || !toUserdata || !newUserdata
        || !createTable || !setField || !setMetatable || !pushString || !pushNil) return 0;
    rankingOwner.store(nullptr);
    rankingResult.store(0);
    supportRadar.store(nullptr);
    supportMc.store(nullptr);
    ++generation;
    launchLogTime = -1e9;
    supported = matches(GetModuleHandleW(L"FA18C.dll"), "fa6e89b5e193b7faf11a9de16269bb9314472532cf447254cbf38e1e9b375f31")
        && matches(GetModuleHandleW(L"CockpitBase.dll"), "163d55167503da8dd0ea9eb459e17206fc3dc8d6b4805b5b13bbe58341beb794")
        && matches(GetModuleHandleW(L"edObjects.dll"), "dfcbde26de71f85d8b9b9b57554ad9f6781ed9bae9a2eb6b7a08e55c9199d468")
        && matches(GetModuleHandleW(L"WorldGeneral.dll"), "7e9ad48e07f574333ab32c12c92a7be9c91173c57de9dda702fe433f04d6ad27");
    holderCoalition=symbol<int(*)()>(GetModuleHandleW(L"CockpitBase.dll"),"?getHolderCoalition@HumanRadiosKeeper@cockpit@@SA?AW4wcCoalitionName@@XZ");
    objectCoalition=symbol<int(*)(const void*)>(GetModuleHandleW(L"WorldGeneral.dll"),"?Coalition@MovingObject@@QEBA?BW4wcCoalitionName@@XZ");
    registryAddress=reinterpret_cast<void**>(GetProcAddress(GetModuleHandleW(L"edObjects.dll"),"?instance@RegisterManager@@2PEAV1@EA"));
    supported=supported && holderCoalition && objectCoalition && registryAddress;
    if (supported) install_launch_zone();
    if (supported) saRangeStatus = sa_range_apply(true);
    if (supported) {
        HMODULE pinned{};
        // Native calls can outlive a Lua state; keep code and relay valid until process exit.
        supported = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&rank_tracks), &pinned)
            && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"FA18C.dll", &pinned)
            && rankingHook.prepare(reinterpret_cast<uintptr_t>(GetModuleHandleW(L"FA18C.dll")),
                reinterpret_cast<void*>(&rank_tracks));
        if (supported) {
            nativeRank = reinterpret_cast<f23radar::RankFunction>(rankingHook.trampoline());
            supported = rankingHook.install();
        }
        if (supported) {
            // Reviewed entry bytes and vtable slot for the hash-checked FA18C build only.
            static constexpr unsigned char entry[13] =
                {0x48,0x89,0x5c,0x24,0x18,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x30};
            const auto module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"FA18C.dll"));
            const auto function = module + f23radar::nativeTracking;
            auto slot = reinterpret_cast<uintptr_t*>(module + f23radar::trackingSlot);
            nativeTrackingFunction = reinterpret_cast<f23radar::TrackingFunction>(function);
            weaponsSupported = matches(GetModuleHandleW(L"WeaponsBase.dll"), "a14d00c70d3cba0a3b129df9627280f2a5b01cb60d281484da4468c921d759df")
                && matches(GetModuleHandleW(L"WeaponBlocks.dll"), "e1be3d2bf0d8f3c2dd93925f386b162b96f1db7318e9b319b54a61b66072b83b");
            malice_energy::nativeSupported = weaponsSupported
                && matches(GetModuleHandleW(L"blocksim.dll"), "5952b84cca451babd6b3e46e8d3f12d71826df3c1661f98ef53a224c4dd73466");
            try_profile();
            supportInstalled = accessible(reinterpret_cast<void*>(function), sizeof(entry))
                && std::memcmp(reinterpret_cast<void*>(function), entry, sizeof(entry)) == 0
                && accessible(slot, 8)
                && supportHook.prepare(slot, function, reinterpret_cast<void*>(&supported_tracking))
                && supportHook.install();
        }
    }
    auto token = static_cast<unsigned*>(newUserdata(L, sizeof(unsigned)));
    *token = generation;
    createTable(L, 0, 1);
    pushFunction(L, close_owner, 0);
    setField(L, -2, "__gc");
    setMetatable(L, -2);
    pushFunction(L, update, 1);
    return 1;
}
extern "C" __declspec(dllexport) int luaopen_f23b_primary(lua_State* L) {
    auto lua = GetModuleHandleW(L"lua.dll");
    pushNumber = symbol<void(*)(lua_State*, double)>(lua, "lua_pushnumber");
    pushFunction = symbol<void(*)(lua_State*, LuaFunction, int)>(lua, "lua_pushcclosure");
    if (!pushNumber || !pushFunction) return 0;
    if (!matches(GetModuleHandleW(L"FA18C.dll"), "fa6e89b5e193b7faf11a9de16269bb9314472532cf447254cbf38e1e9b375f31")
        || !matches(GetModuleHandleW(L"CockpitBase.dll"), "163d55167503da8dd0ea9eb459e17206fc3dc8d6b4805b5b13bbe58341beb794")
        || !matches(GetModuleHandleW(L"edObjects.dll"), "dfcbde26de71f85d8b9b9b57554ad9f6781ed9bae9a2eb6b7a08e55c9199d468")
        || !matches(GetModuleHandleW(L"WorldGeneral.dll"), "7e9ad48e07f574333ab32c12c92a7be9c91173c57de9dda702fe433f04d6ad27")) return 0;
    holderCoalition = symbol<int(*)()>(GetModuleHandleW(L"CockpitBase.dll"),
        "?getHolderCoalition@HumanRadiosKeeper@cockpit@@SA?AW4wcCoalitionName@@XZ");
    objectCoalition = symbol<int(*)(const void*)>(GetModuleHandleW(L"WorldGeneral.dll"),
        "?Coalition@MovingObject@@QEBA?BW4wcCoalitionName@@XZ");
    registryAddress = reinterpret_cast<void**>(GetProcAddress(GetModuleHandleW(L"edObjects.dll"),
        "?instance@RegisterManager@@2PEAV1@EA"));
    if (!holderCoalition || !objectCoalition || !registryAddress) return 0;
    pushFunction(L, primary_target, 0);
    return 1;
}
// Called by the F-23B ForceBridge before its first native callback. Returns connection status.
extern "C" __declspec(dllexport) int f23b_connect_weapons() {
    const int status = early_install();
    if (status < 0) aliasStatus.store(status);
    return status;
}

// Read-only lifecycle evidence. The test harness can inspect cleanup after a stock-cockpit transition.
static int energy_diagnostics(lua_State* L) {
    pushString(L, malice_energy::diagnostics().c_str()); return 1;
}
extern "C" __declspec(dllexport) int luaopen_f23b_energy_diag(lua_State* L) {
    auto lua = GetModuleHandleW(L"lua.dll");
    pushString = symbol<decltype(pushString)>(lua, "lua_pushstring");
    pushFunction = symbol<decltype(pushFunction)>(lua, "lua_pushcclosure");
    if (!pushString || !pushFunction) return 0;
    pushFunction(L, energy_diagnostics, 0); return 1;
}
