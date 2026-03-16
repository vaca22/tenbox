#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct PortForward {
    uint16_t host_port;
    uint16_t guest_port;
    bool lan = false;  // true = bind 0.0.0.0 (LAN accessible), false = 127.0.0.1 (loopback only)

    // Serialize to hostfwd-style string: "tcp:<bind>:<hport>-:<gport>"
    std::string ToHostfwd() const {
        return std::string("tcp:") + (lan ? "0.0.0.0:" : "127.0.0.1:") +
               std::to_string(host_port) + "-:" + std::to_string(guest_port);
    }

    // Parse hostfwd-style string. Returns true on success.
    // Accepts: "tcp:ADDR:HP-:GP" (full) or "HP:GP" / "HP:GP:L" (legacy)
    static bool FromHostfwd(const char* s, PortForward& out) {
        out = {};
        if (!s || !*s) return false;

        // Full format: "tcp:ADDR:HP-:GP"
        if (s[0] == 't' && s[1] == 'c' && s[2] == 'p' && s[3] == ':') {
            const char* p = s + 4;
            const char* colon1 = FindChar(p, ':');
            if (!colon1) return false;
            std::string bind_addr(p, colon1);
            p = colon1 + 1;
            const char* dash = FindChar(p, '-');
            if (!dash) return false;
            unsigned hp = ParseUint(p, dash);
            if (hp == 0 || hp > 65535) return false;
            p = dash + 1;
            if (*p != ':') return false;
            p++;
            const char* end = p;
            while (*end >= '0' && *end <= '9') ++end;
            unsigned gp = ParseUint(p, end);
            if (gp == 0 || gp > 65535) return false;
            out.host_port = static_cast<uint16_t>(hp);
            out.guest_port = static_cast<uint16_t>(gp);
            out.lan = (bind_addr != "127.0.0.1");
            return true;
        }

        // Legacy format: "HP:GP" or "HP:GP:LAN_FLAG"
        const char* p = s;
        const char* c1 = FindChar(p, ':');
        if (!c1) return false;
        unsigned hp = ParseUint(p, c1);
        if (hp == 0 || hp > 65535) return false;
        p = c1 + 1;
        const char* c2 = FindChar(p, ':');
        const char* gp_end = c2 ? c2 : FindEnd(p);
        unsigned gp = ParseUint(p, gp_end);
        if (gp == 0 || gp > 65535) return false;
        unsigned lan_flag = 0;
        if (c2) {
            p = c2 + 1;
            lan_flag = ParseUint(p, FindEnd(p));
        }
        out.host_port = static_cast<uint16_t>(hp);
        out.guest_port = static_cast<uint16_t>(gp);
        out.lan = (lan_flag != 0);
        return true;
    }

private:
    static const char* FindChar(const char* s, char c) {
        for (; *s; ++s) if (*s == c) return s;
        return nullptr;
    }
    static const char* FindEnd(const char* s) {
        while (*s) ++s;
        return s;
    }
    static unsigned ParseUint(const char* begin, const char* end) {
        unsigned v = 0;
        for (const char* p = begin; p < end; ++p) {
            if (*p < '0' || *p > '9') return 0;
            unsigned next = v * 10 + static_cast<unsigned>(*p - '0');
            if (next < v) return 0;  // overflow
            v = next;
        }
        return (begin < end) ? v : 0;
    }
};

struct SharedFolder {
    std::string tag;        // virtiofs mount tag (e.g., "share")
    std::string host_path;  // host directory path
    bool readonly = false;
};

enum class VmPowerState : uint8_t {
    kStopped = 0,
    kStarting = 1,
    kRunning = 2,
    kStopping = 3,
    kCrashed = 4,
};

struct VmSpec {
    std::string name;
    std::string vm_id;       // UUID derived from directory name
    std::string vm_dir;      // absolute path to this VM's directory
    std::string kernel_path; // absolute at runtime, relative in vm.json
    std::string initrd_path;
    std::string disk_path;
    std::string cmdline;
    uint64_t memory_mb = 4096;
    uint32_t cpu_count = 4;
    bool nat_enabled = true;
    bool debug_mode = false;
    bool dpi_scaled = false;  // true = apply DPI scaling (lower VM res, larger text), false = 1:1 physical pixels
    std::vector<PortForward> port_forwards;
    std::vector<SharedFolder> shared_folders;
    int64_t creation_time = 0;   // Unix timestamp (seconds since epoch), 0 = not set
    int64_t last_boot_time = 0;  // Unix timestamp when VM was last started
};

struct VmMutablePatch {
    std::optional<std::string> name;
    std::optional<bool> debug_mode;
    std::optional<std::vector<PortForward>> port_forwards;
    std::optional<std::vector<SharedFolder>> shared_folders;
    std::optional<uint64_t> memory_mb;
    std::optional<uint32_t> cpu_count;
    bool apply_on_next_boot = false;
};
