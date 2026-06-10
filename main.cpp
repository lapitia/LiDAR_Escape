//to-do: rewrite all comments
// SFML for window creation, input, and 2D rendering (HUD)
// OpenGL and GLU for 3D rendering
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/OpenGL.hpp>
#include <GL/glu.h>

#include <vector>
#include <string>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <map>

// basic 3D vector structure and utility functions
struct Vec3 {
    float x{};
    float y{};
    float z{};
};

static Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vec3 operator*(const Vec3& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}

static float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float lengthVec(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

static Vec3 normalize(const Vec3& v) {
    float len = lengthVec(v);
    if (len < 0.0001f) return {0.f, 0.f, 0.f};
    return {v.x / len, v.y / len, v.z / len};
}

static float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

// Axis-Aligned Bounding Box (AABB) structure used for world geometry (walls, floor, ceiling)
struct AABB {
    Vec3 min; // minimum corner (x,y,z)
    Vec3 max; // maximum corner
    std::string type; // "wall", "floor", "ceiling"
    bool active = true; // can be used to temporarily disable a box
};

struct WorldBox {
    AABB box;
    int id = 0;
};

struct Spike {
    Vec3 pos;
    bool active = true;
    float revealTimer = 0.f;
    float height = 0.72f;
    float baseRadius = 0.30f;
};

struct Switch {
    Vec3 pos;
    bool activated = false;
    float revealTimer = 0.f;
};

enum class DoorRequirement {
    SwitchOnly,
    PanelOnly,
    SwitchAndPanel
};

struct Paper {
    Vec3 pos;
    std::string symbol;
    int position = -1; // which position in the code this digit belongs to (1-indexed)
    bool collected = false;
    float revealTimer = 0.f;
};

struct Panel {
    Vec3 pos;
    std::string code;
    float revealTimer = 0.f;
};

struct BatteryPickup {
    Vec3 pos;
    bool collected = false;
    float revealTimer = 0.f;
    float chargeAmount = 30.f;
};

struct Stalker {
    Vec3 pos;
    Vec3 initialPos;
    bool active = true;
    bool alerted = false;
    float revealTimer = 0.f;
    float alertVisualTimer = 0.f;
    float speedPatrol = 0.0f;
    float speedChase = 2.35f;
    float killRadius = 0.48f;
};

struct Door {
    Vec3 pos;
    bool open = false;
    float revealTimer = 0.f;
    float width = 1.6f;
    float height = 2.1f;
    float thickness = 0.22f;
    char axis = 'x';
    DoorRequirement requirement = DoorRequirement::SwitchOnly;
    std::string code;
    bool panelUnlocked = false;
    int panelIndex = -1;
};

// MapLoader: handles loading a map from a text file and provides a fallback (hard‑coded map if the file is missing)
class MapLoader {
public:
    std::vector<WorldBox> boxes;
    std::vector<Spike> spikes;
    std::vector<Switch> switches;
    std::vector<Door> doors;
    std::vector<Panel> panels;
    std::vector<Paper> papers;
    std::vector<BatteryPickup> batteries;
    std::vector<Stalker> stalkers;
    Vec3 spawn{0.f, 1.55f, 7.f}; // default spawn point (eye height ~1.55)

    // helper to create a WorldBox
    void addBox(int id, const std::string& type, Vec3 min, Vec3 max) {
        WorldBox wb;
        wb.id = id;
        wb.box.type = type;
        wb.box.min = min;
        wb.box.max = max;
        wb.box.active = true;
        boxes.push_back(wb);
    }

    void addSpike(const Vec3& pos) {
        Spike s;
        s.pos = pos;
        s.active = true;
        s.revealTimer = 0.f;
        spikes.push_back(s);
    }

    void addSwitch(const Vec3& pos) {
        Switch s;
        s.pos = pos;
        s.activated = false;
        s.revealTimer = 0.f;
        switches.push_back(s);
    }

    void addPanel(const Vec3& pos, const std::string& code = "") {
        Panel p;
        p.pos = pos;
        p.code = code;
        p.revealTimer = 0.f;
        panels.push_back(p);
    }

    void addPaper(const Vec3& pos, const std::string& symbol, int position = -1) {
        Paper p;
        p.pos = pos;
        p.symbol = symbol;
        p.position = position;
        p.collected = false;
        p.revealTimer = 0.f;
        papers.push_back(p);
    }

    void addBattery(const Vec3& pos, float chargeAmount = 30.f) {
        BatteryPickup b;
        b.pos = pos;
        b.collected = false;
        b.revealTimer = 0.f;
        b.chargeAmount = chargeAmount;
        batteries.push_back(b);
    }

    void addStalker(const Vec3& pos) {
        Stalker s;
        s.pos = pos;
        s.initialPos = pos;
        s.active = true;
        s.alerted = false;
        s.revealTimer = 0.f;
        s.alertVisualTimer = 0.f;
        stalkers.push_back(s);
    }

    void addDoor(const Vec3& pos,
                 char axis = 'x',
                 DoorRequirement requirement = DoorRequirement::SwitchOnly,
                 const std::string& code = "",
                 int panelIndex = -1) {
        Door d;
        d.pos = pos;
        d.open = false;
        d.revealTimer = 0.f;
        d.axis = (axis == 'z' || axis == 'Z') ? 'z' : 'x';
        d.requirement = requirement;
        d.code = code;
        d.panelUnlocked = false;
        d.panelIndex = panelIndex;
        doors.push_back(d);
    }

    // build a simple test map when no map file exists
    void loadFallbackMap() {
        boxes.clear();
        spikes.clear();
        switches.clear();
        doors.clear();
        panels.clear();
        papers.clear();
        batteries.clear();
        stalkers.clear();
        int id = 0;
        addBox(id++, "floor",   {-8.f, -0.5f, -18.f}, {8.f, 0.f, 10.f});
        addBox(id++, "ceiling", {-8.f, 2.15f, -18.f}, {8.f, 2.45f, 10.f});
        addBox(id++, "wall", {-8.f, 0.f, -18.f}, {-7.5f, 2.15f, 10.f});
        addBox(id++, "wall", {7.5f, 0.f, -18.f}, {8.f, 2.15f, 10.f});
        addBox(id++, "wall", {-8.f, 0.f, -18.f}, {8.f, 2.15f, -17.5f});
        addBox(id++, "wall", {-8.f, 0.f, 9.5f}, {8.f, 2.15f, 10.f});
        addBox(id++, "wall", {-2.2f, 0.f, -14.f}, {-1.6f, 2.15f, 4.f});
        addBox(id++, "wall", {1.6f, 0.f, -14.f}, {2.2f, 2.15f, 4.f});
        addBox(id++, "wall", {-1.1f, 0.f, -6.0f}, {0.9f, 2.15f, -5.4f});

        addSpike({0.f, 0.05f, -10.f});
        addSwitch({-5.f, 0.05f, 6.f});

        addPaper({-6.0f, 0.05f, 4.0f}, "5", 1);
        addPaper({-4.0f, 0.05f, -1.0f}, "8", 2);
        addPaper({4.0f, 0.05f, -7.5f}, "7", 3);
        addPaper({6.0f, 0.05f, -13.0f}, "2", 4);

        addBattery({-6.3f, 0.05f, 1.8f}, 25.f);
        addBattery({5.2f, 0.05f, -10.8f}, 30.f);
        addStalker({4.8f, 0.05f, 2.5f});

        addPanel({-1.2f, 0.05f, -15.2f}, "5872");
        addDoor({0.f, 0.05f, -16.2f}, 'x', DoorRequirement::SwitchAndPanel, "5872", 0);

        spawn = {0.f, 1.55f, 7.f};
    }

    // load map from a text file
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        boxes.clear();
        spikes.clear();
        switches.clear();
        doors.clear();
        panels.clear();
        papers.clear();
        batteries.clear();
        stalkers.clear();

        // First pass: collect all panel codes to map digits to positions
        std::map<std::string, std::map<char, int>> panelCodePositions;
        std::vector<std::pair<Panel, int>> tempPanels; // store panels with their indices

        std::string line;
        int id = 0;

        // First pass: read all lines to collect panel codes first
        std::vector<std::string> allLines;
        std::ifstream file2(filename);
        while (std::getline(file2, line)) {
            allLines.push_back(line);
        }
        file2.close();

        // First, find all panels and build position mapping
        for (const auto& l : allLines) {
            if (l.empty() || l[0] == '#') continue;
            std::istringstream iss(l);
            std::string token;
            iss >> token;

            if (token == "panel") {
                Panel p;
                float x, y, z;
                iss >> x >> y >> z;
                p.pos = {x, y, z};
                std::string code;
                if (iss >> code) {
                    p.code = code;
                }
                // Map each digit to its position (1-indexed)
                for (size_t i = 0; i < p.code.length(); ++i) {
                    panelCodePositions[p.code][p.code[i]] = (int)i + 1;
                }
            }
        }

        // Second pass: actually load everything
        for (const auto& l : allLines) {
            if (l.empty() || l[0] == '#') continue;

            std::istringstream iss(l);
            std::string token;
            iss >> token;

            if (token == "spawn") {
                // spawn line: "spawn x y z"
                iss >> spawn.x >> spawn.y >> spawn.z;
            } else if (token == "spike") {
                Spike s;
                iss >> s.pos.x >> s.pos.y >> s.pos.z;
                s.active = true;
                s.revealTimer = 0.f;
                spikes.push_back(s);
            } else if (token == "switch") {
                Switch s;
                iss >> s.pos.x >> s.pos.y >> s.pos.z;
                s.activated = false;
                s.revealTimer = 0.f;
                switches.push_back(s);
            } else if (token == "paper") {
                Paper p;
                iss >> p.pos.x >> p.pos.y >> p.pos.z >> p.symbol;
                // Try to read position if provided
                int position = -1;
                if (iss >> position) {
                    p.position = position;
                } else {
                    // No position provided, try to determine from panel codes
                    p.position = -1;
                    for (const auto& panelPair : panels) {
                        for (size_t i = 0; i < panelPair.code.length(); ++i) {
                            if (panelPair.code[i] == p.symbol[0]) {
                                p.position = (int)i + 1;
                                break;
                            }
                        }
                        if (p.position != -1) break;
                    }
                }
                p.collected = false;
                p.revealTimer = 0.f;
                papers.push_back(p);
            } else if (token == "panel") {
                Panel p;
                iss >> p.pos.x >> p.pos.y >> p.pos.z;
                if (!(iss >> p.code)) p.code.clear();
                p.revealTimer = 0.f;
                panels.push_back(p);
            } else if (token == "battery") {
                BatteryPickup b;
                iss >> b.pos.x >> b.pos.y >> b.pos.z;
                if (!(iss >> b.chargeAmount)) b.chargeAmount = 30.f;
                b.collected = false;
                b.revealTimer = 0.f;
                batteries.push_back(b);
            } else if (token == "stalker") {
                Stalker s;
                iss >> s.pos.x >> s.pos.y >> s.pos.z;
                s.initialPos = s.pos;
                s.active = true;
                s.alerted = false;
                s.revealTimer = 0.f;
                s.alertVisualTimer = 0.f;
                stalkers.push_back(s);
            } else if (token == "door") {
                Door d;
                std::string axisToken;
                std::string reqToken;

                iss >> d.pos.x >> d.pos.y >> d.pos.z;
                if (iss >> axisToken) {
                    d.axis = (axisToken == "z" || axisToken == "Z") ? 'z' : 'x';
                } else {
                    d.axis = 'x';
                }

                d.requirement = DoorRequirement::SwitchOnly;
                d.code.clear();
                d.panelUnlocked = false;
                d.panelIndex = -1;

                if (iss >> reqToken) {
                    if (reqToken == "panel") {
                        d.requirement = DoorRequirement::PanelOnly;
                    } else if (reqToken == "both") {
                        d.requirement = DoorRequirement::SwitchAndPanel;
                    } else {
                        d.requirement = DoorRequirement::SwitchOnly;
                    }
                }

                if (d.requirement != DoorRequirement::SwitchOnly) {
                    iss >> d.code;
                    int maybePanelIndex = -1;
                    if (iss >> maybePanelIndex) {
                        d.panelIndex = maybePanelIndex;
                    }
                }

                d.open = false;
                d.revealTimer = 0.f;
                doors.push_back(d);
            } else {
                // if not spawn, it's a box: "type minX minY minZ maxX maxY maxZ"
                WorldBox wb;
                wb.id = id++;
                wb.box.type = token;
                wb.box.active = true;
                iss >> wb.box.min.x >> wb.box.min.y >> wb.box.min.z
                    >> wb.box.max.x >> wb.box.max.y >> wb.box.max.z;
                boxes.push_back(wb);
            }
        }

        // Second pass for papers that still don't have positions
        for (auto& paper : papers) {
            if (paper.position == -1) {
                for (const auto& panel : panels) {
                    for (size_t i = 0; i < panel.code.length(); ++i) {
                        if (panel.code[i] == paper.symbol[0]) {
                            paper.position = (int)i + 1;
                            break;
                        }
                    }
                    if (paper.position != -1) break;
                }
            }
        }

        return true;
    }
};

// camera stores position, orientation and movement parameters
// updateFront() recalculates the forward vector from orientation
struct Camera {
    Vec3 pos{0.f, 1.55f, 7.f}; // eye position
    Vec3 front{0.f, 0.f, -1.f}; // direction the camera is looking
    float yaw = -1.5707963f; // horizontal angle (‑π/2 radians = looking along -Z)
    float pitch = 0.f; // vertical angle (looking straight ahead)
    float fovY = 90.f; // vertical field of view in degrees
    float moveSpeed = 2.8f; // movement speed (units per second)
    float mouseSens = 0.0022f; // mouse sensitivity (radians per pixel)

    // recompute front vector
    void updateFront() {
        front.x = std::cos(yaw) * std::cos(pitch);
        front.y = std::sin(pitch);
        front.z = std::sin(yaw) * std::cos(pitch);
        front = normalize(front);
    }
};

// collision detection functions
// treating the player as a sphere
// The world is made of AABBs, so collision is checked via sphere-AABB overlap
static bool sphereAABB(const Vec3& center, float radius, const AABB& box) {
    // find the closest point on the box to the sphere center
    float cx = std::max(box.min.x, std::min(center.x, box.max.x));
    float cy = std::max(box.min.y, std::min(center.y, box.max.y));
    float cz = std::max(box.min.z, std::min(center.z, box.max.z));

    float dx = center.x - cx;
    float dy = center.y - cy;
    float dz = center.z - cz;

    return dx * dx + dy * dy + dz * dz < radius * radius;
}

// floor and ceiling are not solid, just visible
static bool isSolid(const AABB& box) {
    return box.type != "floor" && box.type != "ceiling";
}

// check if player collides with any solid box
static bool collidesAt(const Vec3& pos, float radius, const std::vector<WorldBox>& boxes) {
    // check against every active box
    for (const auto& wb : boxes) {
        if (!wb.box.active) continue;
        if (!isSolid(wb.box)) continue;
        if (sphereAABB(pos, radius, wb.box)) return true;
    }
    return false;
}

static bool pointInsideSpikeVolume(const Vec3& p, const Spike& spike) {
    const float baseY = spike.pos.y - 0.03f;
    const float topY = baseY + spike.height;
    if (p.y < baseY || p.y > topY) return false;

    float t = (p.y - baseY) / std::max(0.0001f, spike.height);
    float radiusAtY = spike.baseRadius * (1.f - t);

    float dx = p.x - spike.pos.x;
    float dz = p.z - spike.pos.z;
    return dx * dx + dz * dz <= radiusAtY * radiusAtY;
}

static bool collidesWithSpike(const Vec3& pos, float radius, const Spike& spike) {
    const float baseY = spike.pos.y - 0.03f;
    const float topY = baseY + spike.height;

    const float footY = pos.y - 1.25f;
    const float shinY = pos.y - 1.00f;
    const float kneeY = pos.y - 0.78f;

    const Vec3 samples[] = {
        {pos.x, footY, pos.z},
        {pos.x, shinY, pos.z},
        {pos.x, kneeY, pos.z}
    };

    for (const Vec3& sample : samples) {
        if (pointInsideSpikeVolume(sample, spike)) {
            return true;
        }
    }

    float verticalMin = footY - radius;
    float verticalMax = kneeY + radius;
    if (verticalMax < baseY || verticalMin > topY) {
        return false;
    }

    float dx = pos.x - spike.pos.x;
    float dz = pos.z - spike.pos.z;
    float distSq = dx * dx + dz * dz;
    float hitRadius = spike.baseRadius + radius * 0.85f;

    return distSq <= hitRadius * hitRadius;
}

static bool collidesWithAnySpike(const Vec3& pos, float radius, const std::vector<Spike>& spikes) {
    for (const auto& spike : spikes) {
        if (!spike.active) continue;
        if (collidesWithSpike(pos, radius, spike)) return true;
    }
    return false;
}

static void updateSpikeReveal(std::vector<Spike>& spikes, float dt) {
    for (auto& spike : spikes) {
        if (spike.revealTimer > 0.f) {
            spike.revealTimer = std::max(0.f, spike.revealTimer - dt);
        }
    }
}

static void updateSwitchReveal(std::vector<Switch>& switches, float dt) {
    for (auto& sw : switches) {
        if (sw.revealTimer > 0.f) {
            sw.revealTimer = std::max(0.f, sw.revealTimer - dt);
        }
    }
}

static void updateDoorReveal(std::vector<Door>& doors, float dt) {
    for (auto& door : doors) {
        if (door.revealTimer > 0.f) {
            door.revealTimer = std::max(0.f, door.revealTimer - dt);
        }
    }
}

static void updatePanelReveal(std::vector<Panel>& panels, float dt) {
    for (auto& panel : panels) {
        if (panel.revealTimer > 0.f) {
            panel.revealTimer = std::max(0.f, panel.revealTimer - dt);
        }
    }
}

static void updatePaperReveal(std::vector<Paper>& papers, float dt) {
    for (auto& paper : papers) {
        if (paper.revealTimer > 0.f) {
            paper.revealTimer = std::max(0.f, paper.revealTimer - dt);
        }
    }
}

static void updateBatteryReveal(std::vector<BatteryPickup>& batteries, float dt) {
    for (auto& battery : batteries) {
        if (battery.revealTimer > 0.f) {
            battery.revealTimer = std::max(0.f, battery.revealTimer - dt);
        }
    }
}

static void updateStalkerReveal(std::vector<Stalker>& stalkers, float dt) {
    for (auto& stalker : stalkers) {
        if (stalker.revealTimer > 0.f) {
            stalker.revealTimer = std::max(0.f, stalker.revealTimer - dt);
        }
        if (stalker.alertVisualTimer > 0.f) {
            stalker.alertVisualTimer = std::max(0.f, stalker.alertVisualTimer - dt);
        }
        if (stalker.revealTimer <= 0.f) {
            stalker.alerted = false;
            stalker.alertVisualTimer = 0.f;
        }
    }
}

static bool pointNearSwitch(const Vec3& p, const Switch& sw) {
    float dx = p.x - sw.pos.x;
    float dy = p.y - (sw.pos.y + 0.35f);
    float dz = p.z - sw.pos.z;
    return dx * dx + dy * dy + dz * dz <= 0.22f * 0.22f;
}

static bool pointNearPanel(const Vec3& p, const Panel& panel) {
    float dx = p.x - panel.pos.x;
    float dy = p.y - (panel.pos.y + 0.65f);
    float dz = p.z - panel.pos.z;
    return dx * dx + dy * dy + dz * dz <= 0.30f * 0.30f;
}

static bool pointNearPaper(const Vec3& p, const Paper& paper) {
    float dx = p.x - paper.pos.x;
    float dy = p.y - (paper.pos.y + 0.08f);
    float dz = p.z - paper.pos.z;
    return dx * dx + dy * dy + dz * dz <= 0.28f * 0.28f;
}

static bool pointNearBattery(const Vec3& p, const BatteryPickup& battery) {
    float dx = p.x - battery.pos.x;
    float dy = p.y - (battery.pos.y + 0.18f);
    float dz = p.z - battery.pos.z;
    return dx * dx + dy * dy + dz * dz <= 0.30f * 0.30f;
}

static bool pointNearStalker(const Vec3& p, const Stalker& stalker) {
    float dx = p.x - stalker.pos.x;
    float dy = p.y - (stalker.pos.y + 0.95f);
    float dz = p.z - stalker.pos.z;
    return dx * dx + dy * dy + dz * dz <= 0.55f * 0.55f;
}

static void getDoorHalfExtents(const Door& door, float& halfX, float& halfZ) {
    if (door.axis == 'z') {
        halfX = door.thickness * 0.5f + 0.08f;
        halfZ = door.width * 0.5f;
    } else {
        halfX = door.width * 0.5f;
        halfZ = door.thickness * 0.5f + 0.08f;
    }
}

static bool pointNearDoorReveal(const Vec3& p, const Door& door) {
    float halfX, halfZ;
    getDoorHalfExtents(door, halfX, halfZ);
    float minY = door.pos.y;
    float maxY = door.pos.y + door.height;
    return p.x >= door.pos.x - halfX && p.x <= door.pos.x + halfX &&
           p.z >= door.pos.z - halfZ && p.z <= door.pos.z + halfZ &&
           p.y >= minY && p.y <= maxY;
}

static bool isNearSwitchInteraction(const Vec3& pos, const Switch& sw) {
    float dx = pos.x - sw.pos.x;
    float dz = pos.z - sw.pos.z;
    float distSq = dx * dx + dz * dz;
    return distSq <= 1.35f * 1.35f;
}

static bool isNearPanelInteraction(const Vec3& pos, const Panel& panel) {
    float dx = pos.x - panel.pos.x;
    float dz = pos.z - panel.pos.z;
    float distSq = dx * dx + dz * dz;
    return distSq <= 1.4f * 1.4f;
}

static bool isNearPaperPickup(const Vec3& pos, const Paper& paper) {
    float dx = pos.x - paper.pos.x;
    float dz = pos.z - paper.pos.z;
    float distSq = dx * dx + dz * dz;
    return distSq <= 1.1f * 1.1f;
}

static bool isNearBatteryPickup(const Vec3& pos, const BatteryPickup& battery) {
    float dx = pos.x - battery.pos.x;
    float dz = pos.z - battery.pos.z;
    float distSq = dx * dx + dz * dz;
    return distSq <= 1.1f * 1.1f;
}

static bool isNearStalkerAlert(const Vec3& pos, const Stalker& stalker, float radius) {
    float dx = pos.x - stalker.pos.x;
    float dz = pos.z - stalker.pos.z;
    float distSq = dx * dx + dz * dz;
    return distSq <= radius * radius;
}

static bool collidesWithStalker(const Vec3& pos, float radius, const Stalker& stalker) {
    if (!stalker.active) return false;
    float dx = pos.x - stalker.pos.x;
    float dz = pos.z - stalker.pos.z;
    float hitRadius = radius + stalker.killRadius;
    return dx * dx + dz * dz <= hitRadius * hitRadius;
}

static bool collidesWithAnyStalker(const Vec3& pos, float radius, const std::vector<Stalker>& stalkers) {
    for (const auto& stalker : stalkers) {
        if (collidesWithStalker(pos, radius, stalker)) return true;
    }
    return false;
}

static float getNearestStalkerDistance(const Vec3& pos, const std::vector<Stalker>& stalkers) {
    float best = 9999.f;
    for (const auto& stalker : stalkers) {
        if (!stalker.active) continue;
        float dx = pos.x - stalker.pos.x;
        float dz = pos.z - stalker.pos.z;
        float d = std::sqrt(dx * dx + dz * dz);
        best = std::min(best, d);
    }
    return best;
}

static bool isNearDoorInteraction(const Vec3& pos, const Door& door) {
    float dx = std::fabs(pos.x - door.pos.x);
    float dz = std::fabs(pos.z - door.pos.z);
    if (door.axis == 'z') {
        return dx <= 1.2f && dz <= door.width * 0.5f + 0.7f;
    }
    return dx <= door.width * 0.5f + 0.7f && dz <= 1.2f;
}

static bool isNearOpenDoor(const Vec3& pos, const Door& door) {
    if (!door.open) return false;
    float dx = std::fabs(pos.x - door.pos.x);
    float dz = std::fabs(pos.z - door.pos.z);
    if (door.axis == 'z') {
        return dx <= 0.8f && dz <= door.width * 0.5f;
    }
    return dx <= door.width * 0.5f && dz <= 0.8f;
}

// move the sphere from its current position by delta by breaking the movement into small steps
// for each step try moving in x, then z to allow sliding well along them
// The player's y is fixed
static void moveWithSlide(Vec3& pos, const Vec3& delta, float radius, const std::vector<WorldBox>& boxes) {
    float totalLen = lengthVec(delta);
    if (totalLen <= 0.00001f) return;
    const float maxStep = 0.08f;
    int steps = std::max(1, (int)std::ceil(totalLen / maxStep));
    Vec3 step = delta * (1.0f / (float)steps);
    // a small offset to prevent getting stuck on a surface
    const float skin = 0.001f;

    for (int i = 0; i < steps; ++i) {
        Vec3 next = pos;

        // try moving in x direction first
        if (std::abs(step.x) > 0.00001f) {
            Vec3 tryX = next;
            tryX.x += step.x;

            if (!collidesAt(tryX, radius, boxes)) {
                // no collision: accept full x movement
                next.x = tryX.x;
            } else {
                // collision: attempt to move incrementally until we hit the wall, then back off slightly (skin)
                float sign = step.x > 0.f ? 1.f : -1.f;
                float moved = 0.f;
                while (std::abs(moved) < std::abs(step.x)) {
                    float inc = std::min(0.01f, std::abs(step.x) - moved);
                    Vec3 tiny = next;
                    tiny.x += sign * inc;
                    if (collidesAt(tiny, radius, boxes)) break;
                    next.x = tiny.x;
                    moved += inc;
                }
                // slide back by skin to avoid sticking into walls
                next.x -= sign * skin;
            }
        }

        // then try moving in z direction
        if (std::abs(step.z) > 0.00001f) {
            Vec3 tryZ = next;
            tryZ.z += step.z;

            if (!collidesAt(tryZ, radius, boxes)) {
                next.z = tryZ.z;
            } else {
                float sign = step.z > 0.f ? 1.f : -1.f;
                float moved = 0.f;
                while (std::abs(moved) < std::abs(step.z)) {
                    float inc = std::min(0.01f, std::abs(step.z) - moved);
                    Vec3 tiny = next;
                    tiny.z += sign * inc;
                    if (collidesAt(tiny, radius, boxes)) break;
                    next.z = tiny.z;
                    moved += inc;
                }
                next.z -= sign * skin;
            }
        }

        // update position for this substep
        pos = next;
    }
}

// point simulation
// points are generated by casting rays into the scene and recording where they hit a solid box
struct ScanPoint {
    Vec3 pos; // world position of the point
    float lifetime; // remaining time for life of a point (seconds)
    float maxLifetime; // max lifetime (for fading)
};

class PointCloud {
public:
    bool alertedStalkerThisScan = false;
    std::vector<ScanPoint> points;
    // update lifetimes and remove expired points that decayed
    void update(float dt) {
        for (auto it = points.begin(); it != points.end();) {
            it->lifetime -= dt;
            if (it->lifetime <= 0.f) {
                it = points.erase(it);
            } else {
                ++it;
            }
        }
    }

    // add a single point with a given maximum lifetime
    void addPoint(const Vec3& pos, float maxLife) {
        ScanPoint p;
        p.pos = pos;
        p.lifetime = maxLife;
        p.maxLifetime = maxLife;
        points.push_back(p);
    }

    // perform a scan: cast a number of rays within a cone (angle in degrees) up to max distance maxDist
    // for each ray, step along until it hits a box
    // then add a point at the impact location
    void scan(const Camera& cam,
              const std::vector<WorldBox>& boxes,
              std::vector<Spike>& spikes,
              std::vector<Switch>& switches,
              std::vector<Door>& doors,
              std::vector<Panel>& panels,
              std::vector<Paper>& papers,
              std::vector<BatteryPickup>& batteries,
              std::vector<Stalker>& stalkers,
              int rays, float coneDeg, float maxDist) {
        float coneRad = coneDeg * 3.1415926f / 180.f;
        float tanCone = std::tan(coneRad * 0.5f);

        Vec3 worldUp{0.f, 1.f, 0.f};
        Vec3 right = normalize(cross(cam.front, worldUp));
        // in case front is parallel to worldUp, fallback to a different right
        if (lengthVec(right) < 0.0001f) right = {1.f, 0.f, 0.f};
        Vec3 up = normalize(cross(right, cam.front));

        const float spikeRevealLife = 1.35f;
        const float switchRevealLife = 1.7f;
        const float doorRevealLife = 1.7f;
        const float panelRevealLife = 1.7f;
        const float paperRevealLife = 1.7f;
        const float batteryRevealLife = 1.7f;
        const float stalkerRevealLife = 1.95f;
        alertedStalkerThisScan = false;

        for (int i = 0; i < rays; ++i) {
            // random sample inside a circular cone footprint
            // using sqrt(u) to prevent forming a biased ring pattern
            float u1 = (float)std::rand() / (float)RAND_MAX;
            float u2 = (float)std::rand() / (float)RAND_MAX;

            float r = std::sqrt(u1) * tanCone;
            float a = 2.f * 3.1415926f * u2;

            float offsetX = std::cos(a) * r;
            float offsetY = std::sin(a) * r;
            // direction is cam.front plus some offsets
            Vec3 dir = normalize(cam.front + right * offsetX + up * offsetY);

            // step along the ray (starting a little away from the camera)
            const float stepSize = 0.08f;
            float t = 0.18f + ((float)std::rand() / (float)RAND_MAX) * stepSize;
            float prevT = t;

            while (t <= maxDist) {
                Vec3 p = cam.pos + dir * t;

                for (auto& spike : spikes) {
                    if (!spike.active) continue;
                    if (pointInsideSpikeVolume(p, spike)) {
                        spike.revealTimer = std::max(spike.revealTimer, spikeRevealLife);
                    }
                }

                for (auto& sw : switches) {
                    if (pointNearSwitch(p, sw)) {
                        sw.revealTimer = std::max(sw.revealTimer, switchRevealLife);
                    }
                }

                for (auto& panel : panels) {
                    if (pointNearPanel(p, panel)) {
                        panel.revealTimer = std::max(panel.revealTimer, panelRevealLife);
                    }
                }

                for (auto& paper : papers) {
                    if (paper.collected) continue;
                    if (pointNearPaper(p, paper)) {
                        paper.revealTimer = std::max(paper.revealTimer, paperRevealLife);
                    }
                }

                for (auto& battery : batteries) {
                    if (battery.collected) continue;
                    if (pointNearBattery(p, battery)) {
                        battery.revealTimer = std::max(battery.revealTimer, batteryRevealLife);
                    }
                }

                for (auto& stalker : stalkers) {
                    if (!stalker.active) continue;
                    if (pointNearStalker(p, stalker)) {
                        stalker.revealTimer = std::max(stalker.revealTimer, stalkerRevealLife);

                        float dx = p.x - stalker.pos.x;
                        float dz = p.z - stalker.pos.z;
                        float hitDistSq = dx * dx + dz * dz;
                        bool strongScanHit = hitDistSq < 0.30f * 0.30f;

                        if (strongScanHit && !stalker.alerted) {
                            stalker.alerted = true;
                            stalker.alertVisualTimer = 1.15f;
                            alertedStalkerThisScan = true;
                        }
                    }
                }

                for (auto& door : doors) {
                    if (pointNearDoorReveal(p, door)) {
                        door.revealTimer = std::max(door.revealTimer, doorRevealLife);
                    }
                }

                // check against every active box
                for (const auto& wb : boxes) {
                    if (!wb.box.active) continue;

                    //found a hit
                    // refine the hit distance between the previous step and current
                    if (sphereAABB(p, 0.08f, wb.box)) {
                        //found a hit
                        // refine the hit distance between the previous step and current
                        float lo = prevT;
                        float hi = t;

                        for (int k = 0; k < 5; ++k) {
                            float mid = 0.5f * (lo + hi);
                            Vec3 mp = cam.pos + dir * mid;
                            if (sphereAABB(mp, 0.08f, wb.box)) hi = mid;
                            else lo = mid;
                        }

                        addPoint(cam.pos + dir * hi, 2.1f);
                        goto next_ray;
                    }
                }

                prevT = t;
                t += stepSize;
            }

            next_ray:;
        }
    }

    // render all points as OpenGL points with fading alpha (transparency)
    void render() const {
        glDisable(GL_LIGHTING); // points should not be lit
        glPointSize(2.8f);
        glBegin(GL_POINTS);

        for (const auto& p : points) {
            float fade = p.lifetime / p.maxLifetime;
            fade = std::pow(fade, 1.35f);
            glColor3f(fade, fade, fade); // grayscale
            glVertex3f(p.pos.x, p.pos.y, p.pos.z);
        }

        glEnd();
    }
};

// helper rendering functions
static void drawBox(const AABB& b) {
    glBegin(GL_QUADS);

    // +z face
    glNormal3f(0.f, 0.f, 1.f);
    glVertex3f(b.min.x, b.min.y, b.max.z);
    glVertex3f(b.max.x, b.min.y, b.max.z);
    glVertex3f(b.max.x, b.max.y, b.max.z);
    glVertex3f(b.min.x, b.max.y, b.max.z);

    // -z face
    glNormal3f(0.f, 0.f, -1.f);
    glVertex3f(b.max.x, b.min.y, b.min.z);
    glVertex3f(b.min.x, b.min.y, b.min.z);
    glVertex3f(b.min.x, b.max.y, b.min.z);
    glVertex3f(b.max.x, b.max.y, b.min.z);

    // -x face
    glNormal3f(-1.f, 0.f, 0.f);
    glVertex3f(b.min.x, b.min.y, b.min.z);
    glVertex3f(b.min.x, b.min.y, b.max.z);
    glVertex3f(b.min.x, b.max.y, b.max.z);
    glVertex3f(b.min.x, b.max.y, b.min.z);

    // +x face
    glNormal3f(1.f, 0.f, 0.f);
    glVertex3f(b.max.x, b.min.y, b.max.z);
    glVertex3f(b.max.x, b.min.y, b.min.z);
    glVertex3f(b.max.x, b.max.y, b.min.z);
    glVertex3f(b.max.x, b.max.y, b.max.z);

    // -y face
    glNormal3f(0.f, -1.f, 0.f);
    glVertex3f(b.min.x, b.min.y, b.min.z);
    glVertex3f(b.max.x, b.min.y, b.min.z);
    glVertex3f(b.max.x, b.min.y, b.max.z);
    glVertex3f(b.min.x, b.min.y, b.max.z);

    // +y face
    glNormal3f(0.f, 1.f, 0.f);
    glVertex3f(b.min.x, b.max.y, b.max.z);
    glVertex3f(b.max.x, b.max.y, b.max.z);
    glVertex3f(b.max.x, b.max.y, b.min.z);
    glVertex3f(b.min.x, b.max.y, b.min.z);

    glEnd();
}

static void emitConeDots(const Vec3& center, float height, float baseRadius, int rings, int radialSteps, float jitter) {
    const float baseY = center.y - 0.03f;

    for (int iy = 0; iy <= rings; ++iy) {
        float t = (float)iy / (float)rings;
        float y = baseY + t * height;
        float ringRadius = baseRadius * (1.f - t);

        int steps = std::max(6, radialSteps - iy / 2);
        for (int ia = 0; ia < steps; ++ia) {
            float a = (2.f * 3.1415926f * (float)ia) / (float)steps;
            float wobble = std::sin(a * 3.f + t * 11.f) * jitter;
            float rr = std::max(0.f, ringRadius + wobble);

            glVertex3f(
                center.x + std::cos(a) * rr,
                y,
                center.z + std::sin(a) * rr
            );
        }
    }

    for (int iy = 0; iy <= rings; ++iy) {
        float t = (float)iy / (float)rings;
        float y = baseY + t * height;
        glVertex3f(center.x, y, center.z);
    }
}

static void renderSpikes(const std::vector<Spike>& spikes) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(3.6f);
    glBegin(GL_POINTS);

    for (const auto& spike : spikes) {
        if (!spike.active) continue;
        if (spike.revealTimer <= 0.f) continue;

        float fade = clampf(spike.revealTimer / 1.35f, 0.f, 1.f);
        glColor4f(1.0f, 0.12f + 0.10f * fade, 0.12f + 0.10f * fade, 0.45f + 0.55f * fade);
        emitConeDots(spike.pos, spike.height, spike.baseRadius, 16, 22, 0.008f);

        Vec3 s1{spike.pos.x - 0.17f, spike.pos.y, spike.pos.z + 0.05f};
        Vec3 s2{spike.pos.x + 0.17f, spike.pos.y, spike.pos.z + 0.03f};
        Vec3 s3{spike.pos.x + 0.04f, spike.pos.y, spike.pos.z - 0.16f};

        glColor4f(1.0f, 0.10f, 0.10f, 0.35f + 0.45f * fade);
        emitConeDots(s1, spike.height * 0.72f, spike.baseRadius * 0.62f, 12, 18, 0.007f);
        emitConeDots(s2, spike.height * 0.68f, spike.baseRadius * 0.58f, 12, 18, 0.007f);
        emitConeDots(s3, spike.height * 0.55f, spike.baseRadius * 0.48f, 10, 16, 0.006f);
    }

    glEnd();
    glDisable(GL_BLEND);
}

static void renderSwitches(const std::vector<Switch>& switches) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.0f);
    glBegin(GL_POINTS);

    for (const auto& sw : switches) {
        if (sw.revealTimer <= 0.f && !sw.activated) continue;
        float fade = sw.activated ? 1.f : clampf(sw.revealTimer / 1.7f, 0.f, 1.f);
        if (sw.activated) glColor4f(0.15f, 1.0f, 0.32f, 0.95f);
        else glColor4f(0.18f, 0.75f, 1.0f, 0.45f + 0.5f * fade);

        for (int i = 0; i <= 14; ++i) {
            float y = sw.pos.y + 0.05f + 0.04f * (float)i;
            glVertex3f(sw.pos.x, y, sw.pos.z);
            glVertex3f(sw.pos.x + 0.035f, y, sw.pos.z + 0.01f);
            glVertex3f(sw.pos.x - 0.035f, y, sw.pos.z - 0.01f);
        }

        for (int i = 0; i < 18; ++i) {
            float a = 2.f * 3.1415926f * (float)i / 18.f;
            glVertex3f(sw.pos.x + std::cos(a) * 0.11f, sw.pos.y + 0.68f, sw.pos.z + std::sin(a) * 0.11f);
            glVertex3f(sw.pos.x + std::cos(a) * 0.08f, sw.pos.y + 0.75f, sw.pos.z + std::sin(a) * 0.08f);
        }
    }

    glEnd();
    glDisable(GL_BLEND);
}

static void renderPanels(const std::vector<Panel>& panels) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.1f);
    glBegin(GL_POINTS);

    for (const auto& panel : panels) {
        if (panel.revealTimer <= 0.f) continue;

        float fade = clampf(panel.revealTimer / 1.7f, 0.f, 1.f);
        glColor4f(1.0f, 0.82f, 0.18f, 0.45f + 0.5f * fade);

        for (int iy = 0; iy <= 18; ++iy) {
            float y = panel.pos.y + 0.15f + 0.04f * (float)iy;
            glVertex3f(panel.pos.x, y, panel.pos.z);
            glVertex3f(panel.pos.x + 0.10f, y, panel.pos.z);
            glVertex3f(panel.pos.x - 0.10f, y, panel.pos.z);
        }
    }

    glEnd();
    glDisable(GL_BLEND);
}

static void renderPapers(const std::vector<Paper>& papers) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.0f);
    glBegin(GL_POINTS);

    for (const auto& paper : papers) {
        if (paper.collected) continue;
        if (paper.revealTimer <= 0.f) continue;

        float fade = clampf(paper.revealTimer / 1.7f, 0.f, 1.f);
        glColor4f(1.0f, 0.95f, 0.55f, 0.4f + 0.55f * fade);

        for (int i = 0; i < 18; ++i) {
            float t = (float)i / 17.f;
            glVertex3f(paper.pos.x - 0.10f + 0.20f * t, paper.pos.y + 0.08f, paper.pos.z - 0.07f);
            glVertex3f(paper.pos.x - 0.10f + 0.20f * t, paper.pos.y + 0.08f, paper.pos.z + 0.07f);
        }
    }

    glEnd();
    glDisable(GL_BLEND);
}

static void renderBatteries(const std::vector<BatteryPickup>& batteries) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.0f);
    glBegin(GL_POINTS);

    for (const auto& battery : batteries) {
        if (battery.collected) continue;
        if (battery.revealTimer <= 0.f) continue;

        float fade = clampf(battery.revealTimer / 1.7f, 0.f, 1.f);

        const float bodyRadius = 0.085f;
        const float bodyHeight = 0.34f;
        const float baseY = battery.pos.y + 0.05f;
        const float topY = baseY + bodyHeight;

        // body
        for (int iy = 0; iy <= 16; ++iy) {
            float t = (float)iy / 16.f;
            float y = baseY + bodyHeight * t;

            float rCol = 0.18f + 0.10f * (1.f - t);
            float gCol = 0.90f;
            float bCol = 0.22f + 0.06f * t;
            float aCol = 0.42f + 0.50f * fade;

            if (t < 0.28f) {
                rCol *= 0.55f;
                gCol *= 0.55f;
                bCol *= 0.55f;
            }

            glColor4f(rCol, gCol, bCol, aCol);

            for (int ia = 0; ia < 24; ++ia) {
                float a = 2.f * 3.1415926f * (float)ia / 24.f;
                float wobble = std::sin(a * 2.f + t * 8.f) * 0.004f;
                float rr = bodyRadius + wobble;

                glVertex3f(
                    battery.pos.x + std::cos(a) * rr,
                    y,
                    battery.pos.z + std::sin(a) * rr
                );
            }
        }

        // cap
        glColor4f(0.95f, 0.95f, 0.98f, 0.80f + 0.15f * fade);
        for (int iy = 0; iy <= 5; ++iy) {
            float y = topY + 0.012f + 0.008f * (float)iy;
            for (int ia = 0; ia < 16; ++ia) {
                float a = 2.f * 3.1415926f * (float)ia / 16.f;
                glVertex3f(
                    battery.pos.x + std::cos(a) * 0.032f,
                    y,
                    battery.pos.z + std::sin(a) * 0.032f
                );
            }
        }
    }

    glEnd();
    glDisable(GL_BLEND);
}

static void renderStalkers(const std::vector<Stalker>& stalkers) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.6f);
    glBegin(GL_POINTS);

    for (const auto& stalker : stalkers) {
        if (!stalker.active) continue;
        if (stalker.revealTimer <= 0.f && !stalker.alerted) continue;

        float revealFade = clampf(stalker.revealTimer / 1.95f, 0.f, 1.f);
        float alertFade = stalker.alerted ? 1.f : clampf(stalker.alertVisualTimer / 1.15f, 0.f, 1.f);
        float alpha = std::max(revealFade, alertFade);
        float twitch = 0.5f + 0.5f * std::sin(stalker.pos.x * 8.2f + stalker.pos.z * 6.1f + alpha * 15.0f);

        float x = stalker.pos.x;
        float z = stalker.pos.z;
        float baseY = stalker.pos.y + 0.01f;

        float fleshAlpha = 0.18f + 0.58f * alpha;
        float boneAlpha = 0.14f + 0.50f * alpha;
        float faceAlpha = 0.26f + 0.66f * alpha;

        // pelvis and lower abdomen
        glColor4f(0.10f + 0.26f * alertFade, 0.05f + 0.12f * twitch, 0.05f + 0.10f * revealFade, fleshAlpha);
        for (int iy = 0; iy <= 8; ++iy) {
            float t = (float)iy / 8.f;
            float y = baseY + 0.28f + 0.20f * t;
            float halfW = 0.12f + 0.04f * std::sin(t * 3.1415926f);
            glVertex3f(x - halfW, y, z + 0.02f * t);
            glVertex3f(x + halfW, y, z + 0.02f * t);
            glVertex3f(x, y, z - 0.10f + 0.03f * t);
        }

        // full torso / chest mass
        glColor4f(0.12f + 0.32f * alertFade, 0.05f + 0.10f * twitch, 0.05f + 0.10f * revealFade, fleshAlpha);
        for (int iy = 0; iy <= 22; ++iy) {
            float t = (float)iy / 22.f;
            float y = baseY + 0.44f + 0.94f * t;
            float chest = std::sin(t * 3.1415926f);
            float hunch = 0.12f * chest;
            float halfW = 0.14f + 0.16f * chest;
            glVertex3f(x - halfW, y, z + hunch);
            glVertex3f(x + halfW, y, z + hunch);
            glVertex3f(x - halfW * 0.46f, y, z - 0.12f + hunch * 0.4f);
            glVertex3f(x + halfW * 0.46f, y, z + 0.12f + hunch * 0.4f);
        }

        // exposed ribs / spine hints
        glColor4f(0.48f + 0.34f * alertFade, 0.08f + 0.06f * twitch, 0.08f + 0.06f * revealFade, boneAlpha);
        for (int i = 0; i <= 10; ++i) {
            float t = (float)i / 10.f;
            float y = baseY + 0.60f + 0.58f * t;
            float rib = 0.08f + 0.09f * (1.f - std::fabs(t - 0.5f) * 1.8f);
            glVertex3f(x - rib, y, z + 0.04f);
            glVertex3f(x + rib, y, z + 0.04f);
            glVertex3f(x, y, z - 0.12f);
        }

        // long neck
        glColor4f(0.22f + 0.22f * alertFade, 0.08f + 0.08f * twitch, 0.08f + 0.08f * revealFade, boneAlpha);
        for (int i = 0; i <= 12; ++i) {
            float t = (float)i / 12.f;
            float y = baseY + 1.22f + 0.26f * t;
            glVertex3f(x, y, z + 0.10f + 0.03f * t);
            glVertex3f(x - 0.035f, y, z + 0.09f + 0.03f * t);
            glVertex3f(x + 0.035f, y, z + 0.09f + 0.03f * t);
        }

        // full skull / head volume
        glColor4f(0.22f + 0.26f * alertFade, 0.10f + 0.08f * twitch, 0.10f + 0.08f * revealFade, faceAlpha);
        for (int iy = 0; iy <= 10; ++iy) {
            float ty = (float)iy / 10.f;
            float y = baseY + 1.44f + 0.26f * ty;
            float ring = std::sin(ty * 3.1415926f);
            float halfW = 0.08f + 0.10f * ring;
            float halfD = 0.06f + 0.08f * ring;
            for (int ia = 0; ia < 14; ++ia) {
                float a = 2.f * 3.1415926f * (float)ia / 14.f;
                glVertex3f(x + std::cos(a) * halfW, y, z + 0.12f + std::sin(a) * halfD);
            }
        }

        // jaws / mouth cavity
        glColor4f(0.90f + 0.10f * alertFade, 0.05f + 0.35f * alertFade, 0.05f + 0.25f * alertFade, 0.18f + 0.76f * alpha);
        for (int i = 0; i <= 8; ++i) {
            float t = (float)i / 8.f;
            glVertex3f(x - 0.09f + 0.18f * t, baseY + 1.46f - 0.08f * std::sin(t * 3.1415926f), z + 0.24f);
            glVertex3f(x - 0.06f + 0.12f * t, baseY + 1.41f - 0.05f * std::sin(t * 3.1415926f), z + 0.27f);
        }

        // eyes
        glColor4f(1.0f, 0.02f + 0.72f * alertFade, 0.02f + 0.55f * alertFade, 0.24f + 0.76f * alpha);
        glVertex3f(x - 0.055f, baseY + 1.58f, z + 0.22f);
        glVertex3f(x + 0.055f, baseY + 1.58f, z + 0.22f);
        glVertex3f(x - 0.03f, baseY + 1.55f, z + 0.24f);
        glVertex3f(x + 0.03f, baseY + 1.55f, z + 0.24f);

        // horns / crown spikes
        glColor4f(0.55f + 0.30f * alertFade, 0.08f, 0.08f, boneAlpha);
        for (int i = 0; i <= 8; ++i) {
            float t = (float)i / 8.f;
            glVertex3f(x - 0.10f - 0.10f * t, baseY + 1.66f + 0.10f * t, z + 0.10f - 0.02f * t);
            glVertex3f(x + 0.10f + 0.10f * t, baseY + 1.66f + 0.10f * t, z + 0.10f - 0.02f * t);
        }

        // huge shoulders and upper back spikes
        glColor4f(0.40f + 0.42f * alertFade, 0.08f + 0.08f * twitch, 0.08f + 0.08f * revealFade, boneAlpha);
        for (int i = 0; i <= 10; ++i) {
            float t = (float)i / 10.f;
            float y = baseY + 1.08f + 0.18f * t;
            glVertex3f(x - 0.22f - 0.10f * t, y, z - 0.02f);
            glVertex3f(x + 0.22f + 0.10f * t, y, z - 0.02f);
            glVertex3f(x, y + 0.06f, z - 0.18f - 0.10f * t);
        }

        // full left and right arms
        glColor4f(0.14f + 0.34f * alertFade, 0.06f + 0.10f * twitch, 0.06f + 0.10f * revealFade, fleshAlpha);
        for (int i = 0; i <= 16; ++i) {
            float t = (float)i / 16.f;
            float y = baseY + 1.12f - 0.54f * t;
            glVertex3f(x - 0.20f - 0.16f * t, y, z + 0.02f * t);
            glVertex3f(x + 0.20f + 0.16f * t, y, z - 0.02f * t);
        }
        for (int i = 0; i <= 14; ++i) {
            float t = (float)i / 14.f;
            float y = baseY + 0.58f - 0.34f * t;
            glVertex3f(x - 0.36f - 0.10f * t, y, z + 0.03f + 0.03f * t);
            glVertex3f(x + 0.36f + 0.10f * t, y, z - 0.03f - 0.03f * t);
        }

        // claws / hands
        glColor4f(0.92f, 0.08f + 0.34f * alertFade, 0.08f + 0.22f * alertFade, 0.20f + 0.65f * alpha);
        for (int i = 0; i <= 5; ++i) {
            float t = (float)i / 5.f;
            glVertex3f(x - 0.48f - 0.08f * t, baseY + 0.20f - 0.05f * t, z + 0.07f + 0.03f * t);
            glVertex3f(x - 0.48f - 0.08f * t, baseY + 0.20f - 0.05f * t, z + 0.02f * t);
            glVertex3f(x + 0.48f + 0.08f * t, baseY + 0.20f - 0.05f * t, z - 0.07f - 0.03f * t);
            glVertex3f(x + 0.48f + 0.08f * t, baseY + 0.20f - 0.05f * t, z - 0.02f * t);
        }

        // full thighs
        glColor4f(0.12f + 0.28f * alertFade, 0.06f + 0.10f * twitch, 0.06f + 0.10f * revealFade, fleshAlpha);
        for (int i = 0; i <= 12; ++i) {
            float t = (float)i / 12.f;
            glVertex3f(x - 0.10f - 0.04f * t, baseY + 0.42f - 0.28f * t, z + 0.05f * t);
            glVertex3f(x + 0.10f + 0.04f * t, baseY + 0.42f - 0.28f * t, z + 0.05f * t);
        }

        // reversed knees / shins
        glColor4f(0.16f + 0.24f * alertFade, 0.08f + 0.08f * twitch, 0.08f + 0.08f * revealFade, boneAlpha);
        for (int i = 0; i <= 12; ++i) {
            float t = (float)i / 12.f;
            glVertex3f(x - 0.14f - 0.10f * t, baseY + 0.14f - 0.18f * t, z + 0.06f + 0.10f * t);
            glVertex3f(x + 0.14f + 0.10f * t, baseY + 0.14f - 0.18f * t, z + 0.06f + 0.10f * t);
        }

        // feet / talons
        glColor4f(0.92f, 0.06f + 0.26f * alertFade, 0.06f + 0.18f * alertFade, 0.16f + 0.56f * alpha);
        for (int i = 0; i <= 6; ++i) {
            float t = (float)i / 6.f;
            glVertex3f(x - 0.25f - 0.06f * t, baseY - 0.05f, z + 0.18f + 0.05f * t);
            glVertex3f(x - 0.18f - 0.06f * t, baseY - 0.05f, z + 0.14f + 0.05f * t);
            glVertex3f(x + 0.25f + 0.06f * t, baseY - 0.05f, z + 0.18f + 0.05f * t);
            glVertex3f(x + 0.18f + 0.06f * t, baseY - 0.05f, z + 0.14f + 0.05f * t);
        }

        // trailing spine / tail-like horror silhouette
        glColor4f(0.42f + 0.38f * alertFade, 0.08f, 0.08f, boneAlpha);
        for (int i = 0; i <= 16; ++i) {
            float t = (float)i / 16.f;
            glVertex3f(x, baseY + 0.92f - 0.62f * t, z - 0.18f - 0.22f * t);
        }

        // alert aura
        if (stalker.alerted) {
            glColor4f(1.0f, 0.10f, 0.10f, 0.18f + 0.66f * alertFade);
            for (int i = 0; i < 22; ++i) {
                float t = (float)i / 21.f;
                glVertex3f(x - 0.30f + 0.60f * t, baseY + 1.92f + 0.05f * std::sin(t * 18.f), z + 0.03f);
                glVertex3f(x + 0.03f * std::sin(t * 22.f), baseY + 1.90f + 0.06f * std::sin(t * 20.f), z - 0.30f + 0.60f * t);
            }
        }
    }

    glEnd();
    glDisable(GL_BLEND);
}

static void updateStalkers(std::vector<Stalker>& stalkers,
                           const Vec3& playerPos,
                           float playerRadius,
                           float dt,
                           const std::vector<WorldBox>& boxes) {
    for (auto& stalker : stalkers) {
        if (!stalker.active) continue;
        if (!stalker.alerted) continue;
        if (stalker.revealTimer <= 0.f) {
            stalker.alerted = false;
            stalker.alertVisualTimer = 0.f;
            continue;
        }

        Vec3 toPlayer{playerPos.x - stalker.pos.x, 0.f, playerPos.z - stalker.pos.z};
        float dist = lengthVec(toPlayer);
        if (dist > 8.5f) {
            stalker.alerted = false;
            stalker.alertVisualTimer = 0.f;
            continue;
        }
        if (dist <= 0.0001f) continue;

        Vec3 dir = normalize(toPlayer);
        float speed = stalker.speedChase;
        if (dist < playerRadius + stalker.killRadius + 0.05f) speed *= 1.15f;

        Vec3 nextPos = stalker.pos;
        Vec3 move = dir * (speed * dt);
        moveWithSlide(nextPos, move, 0.24f, boxes);
        nextPos.y = stalker.pos.y;

        float movedDist = lengthVec(Vec3{nextPos.x - stalker.pos.x, 0.f, nextPos.z - stalker.pos.z});
        stalker.pos = nextPos;

        if (movedDist < 0.002f && dist > 2.4f) {
            stalker.alerted = false;
            stalker.alertVisualTimer = 0.f;
        }
    }
}

static void renderParasiteOverlay(sf::RenderWindow& window, float intensity, float shakeProgress, float motionTime) {
    if (intensity <= 0.001f) return;

    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;
    float i = clampf(intensity, 0.f, 1.f);
    float progress = clampf(shakeProgress, 0.f, 1.f);
    float phase = motionTime * 180.f;
    float fade = i * i * (3.f - 2.f * i);

    sf::RectangleShape haze(sf::Vector2f(W, H));
    haze.setFillColor(sf::Color(30, 8, 8, (sf::Uint8)(18 + 46 * fade)));
    window.draw(haze);

    auto drawParasite = [&](float px, float py, float scale, bool mirrored) {
        float dir = mirrored ? -1.f : 1.f;

        sf::CircleShape outer(14.f * scale, 20);
        outer.setOrigin(14.f * scale, 14.f * scale);
        outer.setPosition(px, py);
        outer.setFillColor(sf::Color(185, 62, 62, (sf::Uint8)(120 + 95 * fade)));
        window.draw(outer);

        sf::CircleShape core(9.f * scale, 18);
        core.setOrigin(9.f * scale, 9.f * scale);
        core.setPosition(px - 2.f * dir * scale, py + 1.f * scale);
        core.setFillColor(sf::Color(222, 110, 110, (sf::Uint8)(130 + 90 * fade)));
        window.draw(core);

        sf::CircleShape eye(2.6f * scale, 14);
        eye.setOrigin(2.6f * scale, 2.6f * scale);
        eye.setPosition(px + 4.2f * dir * scale, py - 2.2f * scale);
        eye.setFillColor(sf::Color(255, 220, 220, (sf::Uint8)(140 + 80 * fade)));
        window.draw(eye);

        sf::CircleShape pupil(1.1f * scale, 12);
        pupil.setOrigin(1.1f * scale, 1.1f * scale);
        pupil.setPosition(px + 4.7f * dir * scale, py - 2.0f * scale);
        pupil.setFillColor(sf::Color(90, 22, 22, (sf::Uint8)(150 + 70 * fade)));
        window.draw(pupil);

        for (int leg = 0; leg < 4; ++leg) {
            float lt = (float)leg / 3.f;
            for (int side = -1; side <= 1; side += 2) {
                sf::RectangleShape nub(sf::Vector2f((6.f + 2.f * lt) * scale, 2.2f * scale));
                nub.setOrigin(1.2f * scale, 1.1f * scale);
                nub.setPosition(px + (lt * 6.f - 4.f) * scale, py + side * (7.f + 1.4f * lt) * scale);
                nub.setRotation(side * (22.f + 16.f * lt));
                nub.setFillColor(sf::Color(205, 92, 92, (sf::Uint8)(95 + 85 * fade)));
                window.draw(nub);
            }
        }
    };

    int sliderCount = 10;
    for (int n = 0; n < sliderCount; ++n) {
        float t = (float)n / (float)sliderCount;
        float y = 40.f + t * (H - 80.f);
        float slide = std::fmod(phase * (0.72f + t * 0.22f) + t * 210.f, W + 180.f) - 90.f;
        drawParasite(slide, y, 0.85f + 0.28f * fade, false);
    }

    int reverseCount = 8;
    for (int n = 0; n < reverseCount; ++n) {
        float t = (float)n / (float)reverseCount;
        float y = 62.f + t * (H - 124.f);
        float slide = W - std::fmod(phase * (0.78f + t * 0.18f) + t * 250.f, W + 200.f) + 100.f;
        drawParasite(slide, y, 0.80f + 0.24f * fade, true);
    }

    int crawlTop = 5;
    for (int n = 0; n < crawlTop; ++n) {
        float t = (float)n / (float)crawlTop;
        float x = 60.f + t * (W - 120.f);
        float slideY = std::fmod(phase * (0.52f + 0.06f * n) + t * 70.f, 110.f) - 34.f;
        drawParasite(x, slideY, 0.86f + 0.22f * fade, false);
    }

    sf::RectangleShape redWash(sf::Vector2f(W, H));
    redWash.setPosition(0.f, 0.f);
    redWash.setFillColor(sf::Color(120, 20, 20, (sf::Uint8)(10 + 42 * fade)));
    window.draw(redWash);

    sf::RectangleShape shakeBg(sf::Vector2f(180.f, 12.f));
    shakeBg.setPosition(W - 196.f, 16.f);
    shakeBg.setFillColor(sf::Color(28, 10, 10, (sf::Uint8)(120 + 60 * fade)));
    window.draw(shakeBg);

    sf::RectangleShape shakeFill(sf::Vector2f(180.f * progress, 12.f));
    shakeFill.setPosition(W - 196.f, 16.f);
    shakeFill.setFillColor(sf::Color(255, 160, 160, (sf::Uint8)(160 + 70 * fade)));
    window.draw(shakeFill);
}

static void renderStalkerGlitch(sf::RenderWindow& window, float intensity, bool scanAlarmFlash) {
    if (intensity <= 0.001f && !scanAlarmFlash) return;

    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;
    float i = clampf(intensity, 0.f, 1.f);
    float flash = scanAlarmFlash ? 0.45f : 0.f;

    int bandCount = 6 + (int)(i * 18.f);
    for (int b = 0; b < bandCount; ++b) {
        float t = (float)b / (float)std::max(1, bandCount - 1);
        float y = t * H;
        float jitter = std::sin((t * 41.f) + i * 13.f) * (8.f + 24.f * i);
        float bandH = 1.f + 3.f * i + (b % 3 == 0 ? 2.f : 0.f);
        sf::RectangleShape band(sf::Vector2f(W * (0.35f + 0.55f * ((b % 5) / 4.f)), bandH));
        band.setPosition(W * 0.08f + jitter, y + std::sin(t * 17.f) * 6.f * i);
        sf::Uint8 alpha = (sf::Uint8)clampf(18.f + 95.f * i + 90.f * flash, 0.f, 255.f);
        if (b % 4 == 0) band.setFillColor(sf::Color(255, 80, 80, alpha));
        else band.setFillColor(sf::Color(235, 235, 235, alpha));
        window.draw(band);
    }

    int noiseCount = 16 + (int)(i * 58.f);
    for (int n = 0; n < noiseCount; ++n) {
        float nx = (float)(std::rand() % std::max(1u, window.getSize().x));
        float ny = (float)(std::rand() % std::max(1u, window.getSize().y));
        float nw = 8.f + (float)(std::rand() % 34) * (0.35f + i);
        float nh = 1.f + (float)(std::rand() % 5);
        sf::RectangleShape px(sf::Vector2f(nw, nh));
        px.setPosition(nx, ny);
        sf::Uint8 alpha = (sf::Uint8)clampf(10.f + (std::rand() % 45) * (0.5f + i), 0.f, 255.f);
        if (n % 6 == 0) px.setFillColor(sf::Color(255, 40, 40, alpha));
        else px.setFillColor(sf::Color(255, 255, 255, alpha));
        window.draw(px);
    }

    sf::RectangleShape vignette(sf::Vector2f(W, H));
    vignette.setPosition(0.f, 0.f);
    vignette.setFillColor(sf::Color(18, 0, 0, (sf::Uint8)clampf(15.f + 95.f * i + 80.f * flash, 0.f, 255.f)));
    window.draw(vignette);
}

static void renderDoors(const std::vector<Door>& doors) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(3.8f);
    glBegin(GL_POINTS);

    for (const auto& door : doors) {
        if (door.revealTimer <= 0.f && !door.open) continue;
        float fade = door.open ? 1.f : clampf(door.revealTimer / 1.7f, 0.f, 1.f);

        if (door.open) {
            glColor4f(0.16f, 1.0f, 0.25f, 0.95f);
        } else {
            if (door.requirement == DoorRequirement::PanelOnly)
                glColor4f(1.0f, 0.80f, 0.18f, 0.42f + 0.48f * fade);
            else if (door.requirement == DoorRequirement::SwitchAndPanel)
                glColor4f(1.0f, 0.42f, 0.18f, 0.42f + 0.48f * fade);
            else
                glColor4f(1.0f, 0.18f, 0.18f, 0.42f + 0.48f * fade);
        }

        float y1 = door.pos.y;
        float y2 = door.pos.y + door.height;

        if (door.axis == 'z') {
            float z1 = door.pos.z - door.width * 0.5f;
            float z2 = door.pos.z + door.width * 0.5f;
            float x = door.pos.x;

            for (int i = 0; i <= 24; ++i) {
                float t = (float)i / 24.f;
                float y = y1 + (y2 - y1) * t;
                glVertex3f(x, y, z1);
                glVertex3f(x, y, z2);
            }
            for (int i = 0; i <= 20; ++i) {
                float t = (float)i / 20.f;
                float z = z1 + (z2 - z1) * t;
                glVertex3f(x, y2, z);
            }
            if (!door.open) {
                for (int i = 0; i <= 14; ++i) {
                    float t = (float)i / 14.f;
                    float y = y1 + (y2 - y1) * t;
                    glVertex3f(x, y, door.pos.z);
                }
            }
        } else {
            float x1 = door.pos.x - door.width * 0.5f;
            float x2 = door.pos.x + door.width * 0.5f;
            float z = door.pos.z;

            for (int i = 0; i <= 24; ++i) {
                float t = (float)i / 24.f;
                float y = y1 + (y2 - y1) * t;
                glVertex3f(x1, y, z);
                glVertex3f(x2, y, z);
            }
            for (int i = 0; i <= 20; ++i) {
                float t = (float)i / 20.f;
                float x = x1 + (x2 - x1) * t;
                glVertex3f(x, y2, z);
            }
            if (!door.open) {
                for (int i = 0; i <= 14; ++i) {
                    float t = (float)i / 14.f;
                    float y = y1 + (y2 - y1) * t;
                    glVertex3f(door.pos.x, y, z);
                }
            }
        }
    }

    glEnd();
    glDisable(GL_BLEND);
}

// shared font used by both the HUD and the menu screens
static sf::Font g_font;
static bool g_fontLoaded = false;
static bool g_fontAttempted = false;

static void ensureFont() {
    if (g_fontAttempted) return;
    g_fontAttempted = true;
    if (g_font.loadFromFile("rainyhearts.ttf")) {
        g_fontLoaded = true;
        std::cout << "Font loaded successfully from rainyhearts.ttf\n";
    } else {
        std::cout << "Warning: Could not load rainyhearts.ttf, using rectangle indicator\n";
    }
}

enum class GameState { MainMenu, Settings, Playing, GameOver, GameComplete };

// user-configurable values with small preset arrays
struct Settings {
    int sensitivityIdx = 1; // 0 = Low | 1 = Medium (default) | 2 = High
    int fovIdx = 2; // maps to: 0=70 | 1=80 | 2=90 | 3=100 | 4=110

    float mouseSens() const {
        static const float p[] = {0.0015f, 0.0022f, 0.003f};
        return p[sensitivityIdx];
    }
    float fov() const {
        static const float p[] = {70.f, 80.f, 90.f, 100.f, 110.f};
        return p[fovIdx];
    }
    const char* sensLabel() const {
        static const char* l[] = {"Low", "Medium", "High"};
        return l[sensitivityIdx];
    }
    const char* fovLabel() const {
        static const char* l[] = {"70", "80", "90", "100", "110"};
        return l[fovIdx];
    }
    void cycleSens() { sensitivityIdx = (sensitivityIdx + 1) % 3; }
    void cycleFov() { fovIdx = (fovIdx + 1) % 5; }
};

// draws text centred at (cx, cy) using the shared font and returns its global bounds
static sf::FloatRect drawCenteredText(sf::RenderWindow& window,
                                      const std::string& str,
                                      unsigned int charSize,
                                      float cx, float cy,
                                      sf::Color color)
{
    sf::Text t;
    t.setFont(g_font);
    t.setString(str);
    t.setCharacterSize(charSize);
    t.setFillColor(color);
    sf::FloatRect lb = t.getLocalBounds();
    t.setOrigin(lb.left + lb.width * 0.5f, lb.top + lb.height * 0.5f);
    t.setPosition(cx, cy);
    window.draw(t);
    return t.getGlobalBounds();
}

// used to widen the hit-box around menu text
static sf::FloatRect expand(sf::FloatRect r, float dx, float dy) {
    return {r.left - dx, r.top - dy, r.width + 2.f * dx, r.height + 2.f * dy};
}

// render the main menu screen
static void renderMainMenu(sf::RenderWindow& window,
                           std::vector<sf::FloatRect>& boundsOut)
{
    ensureFont();
    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;

    sf::RectangleShape bg(sf::Vector2f(W, H));
    bg.setFillColor(sf::Color(4, 4, 7));
    window.draw(bg);

    sf::RectangleShape rule(sf::Vector2f(W * 0.45f, 1.f));
    rule.setFillColor(sf::Color(55, 55, 75));
    rule.setOrigin(W * 0.225f, 0.f);
    rule.setPosition(W * 0.5f, H * 0.345f);
    window.draw(rule);

    if (g_fontLoaded) {
        drawCenteredText(window, "LiDAR Escape", 54, W * 0.5f, H * 0.21f, sf::Color(228, 228, 235));
        drawCenteredText(window, "find the exit", 17, W * 0.5f, H * 0.21f + 58.f, sf::Color(85, 85, 108));
    }

    static const char* kLabels[] = {"Start", "Settings", "Exit"};
    constexpr int kCount = 3;
    boundsOut.resize(kCount);

    const sf::Vector2i mp = sf::Mouse::getPosition(window);
    const float startY = H * 0.43f;
    const float step = 64.f;

    for (int i = 0; i < kCount; ++i) {
        const float y = startY + (float)i * step;

        if (g_fontLoaded) {
            sf::Text t;
            t.setFont(g_font);
            t.setString(kLabels[i]);
            t.setCharacterSize(34);
            sf::FloatRect lb = t.getLocalBounds();
            t.setOrigin(lb.left + lb.width * 0.5f, lb.top + lb.height * 0.5f);
            t.setPosition(W * 0.5f, y);

            boundsOut[i] = expand(t.getGlobalBounds(), 24.f, 12.f);
            const bool hov = boundsOut[i].contains((float)mp.x, (float)mp.y);
            t.setFillColor(hov ? sf::Color(255, 208, 70) : sf::Color(182, 182, 198));
            window.draw(t);

            if (hov) {
                sf::RectangleShape bar(sf::Vector2f(4.f, 22.f));
                bar.setFillColor(sf::Color(255, 208, 70, 210));
                bar.setOrigin(2.f, 11.f);
                bar.setPosition(W * 0.5f - lb.width * 0.5f - 22.f, y);
                window.draw(bar);
            }
        } else {
            //fallback
            sf::RectangleShape btn(sf::Vector2f(200.f, 44.f));
            btn.setOrigin(100.f, 22.f);
            btn.setPosition(W * 0.5f, y);
            boundsOut[i] = btn.getGlobalBounds();
            const bool hov = boundsOut[i].contains((float)mp.x, (float)mp.y);
            btn.setFillColor(hov ? sf::Color(70, 55, 10) : sf::Color(25, 25, 35));
            window.draw(btn);
        }
    }
}

// render the settings screen
static void renderSettingsMenu(sf::RenderWindow& window,
                               const Settings& cfg,
                               std::vector<sf::FloatRect>& boundsOut)
{
    ensureFont();
    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;

    sf::RectangleShape bg(sf::Vector2f(W, H));
    bg.setFillColor(sf::Color(4, 4, 7));
    window.draw(bg);

    if (g_fontLoaded)
        drawCenteredText(window, "Settings", 48, W * 0.5f, H * 0.19f, sf::Color(228, 228, 235));

    const std::string labels[3] = {
        std::string("Sensitivity:    < ") + cfg.sensLabel() + " >",
        std::string("Field of View:  < ") + cfg.fovLabel() + " >",
        "Back"
    };

    boundsOut.resize(3);
    const sf::Vector2i mp = sf::Mouse::getPosition(window);
    const float startY = H * 0.38f;
    const float step = 68.f;

    for (int i = 0; i < 3; ++i) {
        const float y = startY + (float)i * step;

        if (g_fontLoaded) {
            sf::Text t;
            t.setFont(g_font);
            t.setString(labels[i]);
            t.setCharacterSize(30);
            sf::FloatRect lb = t.getLocalBounds();
            t.setOrigin(lb.left + lb.width * 0.5f, lb.top + lb.height * 0.5f);
            t.setPosition(W * 0.5f, y);

            boundsOut[i] = expand(t.getGlobalBounds(), 24.f, 12.f);
            const bool hov = boundsOut[i].contains((float)mp.x, (float)mp.y);
            t.setFillColor(hov ? sf::Color(255, 208, 70) : sf::Color(182, 182, 198));
            window.draw(t);
        } else {
            // fallback
            sf::RectangleShape btn(sf::Vector2f(300.f, 44.f));
            btn.setOrigin(150.f, 22.f);
            btn.setPosition(W * 0.5f, y);
            boundsOut[i] = btn.getGlobalBounds();
            const bool hov = boundsOut[i].contains((float)mp.x, (float)mp.y);
            btn.setFillColor(hov ? sf::Color(70, 55, 10) : sf::Color(25, 25, 35));
            window.draw(btn);
        }
    }

    if (g_fontLoaded)
        drawCenteredText(window, "click a setting to cycle its value", 15, W * 0.5f, H * 0.76f, sf::Color(60, 60, 82));
}

static void renderGameOverOverlay(sf::RenderWindow& window,
                                  std::vector<sf::FloatRect>& boundsOut,
                                  const std::string& reason)
{
    ensureFont();
    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;

    sf::RectangleShape shade(sf::Vector2f(W, H));
    shade.setFillColor(sf::Color(0, 0, 0, 175));
    window.draw(shade);

    const float panelW = 520.f;
    const float panelH = 340.f;
    const float panelLeft = W * 0.5f - panelW * 0.5f;
    const float panelTop = H * 0.5f - panelH * 0.5f;

    sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
    panel.setPosition(panelLeft, panelTop);
    panel.setFillColor(sf::Color(10, 10, 16, 235));
    panel.setOutlineThickness(2.f);
    panel.setOutlineColor(sf::Color(110, 28, 28, 220));
    window.draw(panel);

    if (g_fontLoaded) {
        drawCenteredText(window, "Game Over", 44, W * 0.5f, panelTop + 72.f, sf::Color(235, 210, 210));
        drawCenteredText(window, reason, 18, W * 0.5f, panelTop + 126.f, sf::Color(175, 175, 190));
    }

    boundsOut.resize(2);
    const sf::Vector2i mp = sf::Mouse::getPosition(window);

    sf::FloatRect restartRect(panelLeft + 130.f, panelTop + 186.f, 260.f, 52.f);
    sf::FloatRect quitRect(panelLeft + 130.f, panelTop + 252.f, 260.f, 52.f);
    boundsOut[0] = restartRect;
    boundsOut[1] = quitRect;

    auto drawButton = [&](const sf::FloatRect& r, const std::string& label, sf::Color base, sf::Color hover) {
        bool hov = r.contains((float)mp.x, (float)mp.y);
        sf::RectangleShape btn(sf::Vector2f(r.width, r.height));
        btn.setPosition(r.left, r.top);
        btn.setFillColor(hov ? hover : base);
        btn.setOutlineThickness(2.f);
        btn.setOutlineColor(hov ? sf::Color(255, 210, 120) : sf::Color(170, 90, 90));
        window.draw(btn);
        if (g_fontLoaded) {
            drawCenteredText(window, label, 26, r.left + r.width * 0.5f, r.top + r.height * 0.5f + 4.f, sf::Color::White);
        }
    };

    drawButton(restartRect, "Restart", sf::Color(95, 24, 24, 235), sf::Color(145, 36, 36, 240));
    drawButton(quitRect, "Quit", sf::Color(48, 48, 60, 235), sf::Color(76, 76, 96, 240));
}

// render the game complete screen
static void renderGameCompleteOverlay(sf::RenderWindow& window,
                                      std::vector<sf::FloatRect>& boundsOut)
{
    ensureFont();
    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;

    sf::RectangleShape shade(sf::Vector2f(W, H));
    shade.setFillColor(sf::Color(0, 0, 0, 175));
    window.draw(shade);

    const float panelW = 560.f;
    const float panelH = 380.f;
    const float panelLeft = W * 0.5f - panelW * 0.5f;
    const float panelTop = H * 0.5f - panelH * 0.5f;

    sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
    panel.setPosition(panelLeft, panelTop);
    panel.setFillColor(sf::Color(10, 10, 16, 235));
    panel.setOutlineThickness(2.f);
    panel.setOutlineColor(sf::Color(200, 180, 60, 220));
    window.draw(panel);

    if (g_fontLoaded) {
        drawCenteredText(window, "Congratulations!", 44, W * 0.5f, panelTop + 72.f, sf::Color(240, 220, 120));
        drawCenteredText(window, "You have completed the game!", 20, W * 0.5f, panelTop + 130.f, sf::Color(190, 190, 210));
        drawCenteredText(window, "Thank you for playing.", 18, W * 0.5f, panelTop + 172.f, sf::Color(160, 160, 180));
    }

    boundsOut.resize(2);
    const sf::Vector2i mp = sf::Mouse::getPosition(window);

    sf::FloatRect mainMenuRect(panelLeft + 40.f, panelTop + 230.f, 220.f, 52.f);
    sf::FloatRect quitRect(panelLeft + 300.f, panelTop + 230.f, 220.f, 52.f);
    boundsOut[0] = mainMenuRect;
    boundsOut[1] = quitRect;

    auto drawButton = [&](const sf::FloatRect& r, const std::string& label, sf::Color base, sf::Color hover) {
        bool hov = r.contains((float)mp.x, (float)mp.y);
        sf::RectangleShape btn(sf::Vector2f(r.width, r.height));
        btn.setPosition(r.left, r.top);
        btn.setFillColor(hov ? hover : base);
        btn.setOutlineThickness(2.f);
        btn.setOutlineColor(hov ? sf::Color(255, 210, 120) : sf::Color(170, 130, 50));
        window.draw(btn);
        if (g_fontLoaded) {
            drawCenteredText(window, label, 26, r.left + r.width * 0.5f, r.top + r.height * 0.5f + 4.f, sf::Color::White);
        }
    };

    drawButton(mainMenuRect, "Main Menu", sf::Color(48, 48, 60, 235), sf::Color(76, 76, 96, 240));
    drawButton(quitRect, "Quit", sf::Color(95, 24, 24, 235), sf::Color(145, 36, 36, 240));
}

static void renderPanelOverlay(sf::RenderWindow& window,
                               const std::string& collectedCode,
                               const std::string& inputCode,
                               bool switchesRequired,
                               bool switchesDone,
                               const std::string& message)
{
    ensureFont();
    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;

    sf::RectangleShape shade(sf::Vector2f(W, H));
    shade.setFillColor(sf::Color(0, 0, 0, 180));
    window.draw(shade);

    const float panelW = 620.f;
    const float panelH = 360.f;
    const float panelLeft = W * 0.5f - panelW * 0.5f;
    const float panelTop = H * 0.5f - panelH * 0.5f;

    sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
    panel.setPosition(panelLeft, panelTop);
    panel.setFillColor(sf::Color(12, 12, 18, 240));
    panel.setOutlineThickness(2.f);
    panel.setOutlineColor(sf::Color(190, 160, 60, 220));
    window.draw(panel);

    if (g_fontLoaded) {
        drawCenteredText(window, "Door Panel", 36, W * 0.5f, panelTop + 46.f, sf::Color(245, 220, 110));

        sf::Text line1;
        line1.setFont(g_font);
        line1.setCharacterSize(24);
        line1.setFillColor(sf::Color(230, 230, 230));
        line1.setString("Collected code: " + collectedCode);
        line1.setPosition(panelLeft + 40.f, panelTop + 96.f);
        window.draw(line1);

        sf::Text line2;
        line2.setFont(g_font);
        line2.setCharacterSize(24);
        line2.setFillColor(sf::Color(255, 255, 255));
        line2.setString("Input: " + inputCode);
        line2.setPosition(panelLeft + 40.f, panelTop + 144.f);
        window.draw(line2);

        sf::Text line3;
        line3.setFont(g_font);
        line3.setCharacterSize(18);
        line3.setFillColor(switchesDone ? sf::Color(120, 255, 140) : sf::Color(255, 200, 120));
        if (switchesRequired) {
            line3.setString(std::string("Switches required: ") + (switchesDone ? "DONE" : "NOT YET"));
        } else {
            line3.setString("Switches required: NO");
        }
        line3.setPosition(panelLeft + 40.f, panelTop + 200.f);
        window.draw(line3);

        sf::Text line4;
        line4.setFont(g_font);
        line4.setCharacterSize(16);
        line4.setFillColor(sf::Color(170, 170, 190));
        line4.setString("Type digits/letters, Backspace deletes, Enter confirms, Esc closes");
        line4.setPosition(panelLeft + 40.f, panelTop + 248.f);
        window.draw(line4);

        if (!message.empty()) {
            sf::Text msg;
            msg.setFont(g_font);
            msg.setCharacterSize(18);
            msg.setFillColor(sf::Color(255, 180, 180));
            msg.setString(message);
            msg.setPosition(panelLeft + 40.f, panelTop + 290.f);
            window.draw(msg);
        }
    }
}

static void renderBatteryHUD(sf::RenderWindow& window,
                             float battery,
                             bool areaScanHeld,
                             int levelNumber,
                             int switchesActive,
                             int switchesTotal,
                             int papersCollected,
                             int papersTotal,
                             bool nearInteractable,
                             bool panelMenuOpen)
{
    sf::RectangleShape bg(sf::Vector2f(220.f, 20.f));
    bg.setPosition(12.f, 12.f);
    bg.setFillColor(sf::Color(30, 10, 10));
    window.draw(bg);

    sf::RectangleShape fill(sf::Vector2f(220.f * (battery / 100.f), 20.f));
    fill.setPosition(12.f, 12.f);
    fill.setFillColor(sf::Color(220, 220, 220));
    window.draw(fill);

    ensureFont();

    if (g_fontLoaded) {
        sf::Text modeText;
        modeText.setFont(g_font);
        modeText.setCharacterSize(18);
        modeText.setFillColor(sf::Color::White);
        modeText.setPosition(12.f, 40.f);

        if (areaScanHeld)
            modeText.setString("Scan Mode: AREA (120 deg, 150 rays)");
        else
            modeText.setString("Scan Mode: LOCAL (32 deg, 90 rays)");

        window.draw(modeText);

        sf::Text batteryText;
        batteryText.setFont(g_font);
        batteryText.setCharacterSize(14);
        batteryText.setFillColor(sf::Color::White);
        batteryText.setPosition(12.f, 65.f);

        char batteryStr[192];
        std::sprintf(
            batteryStr,
            "Battery: %.0f%%   Level: %d   Switches: %d/%d   Papers: %d/%d",
            battery, levelNumber, switchesActive, switchesTotal, papersCollected, papersTotal
        );
        batteryText.setString(batteryStr);
        window.draw(batteryText);

        sf::Text interactText;
        interactText.setFont(g_font);
        interactText.setCharacterSize(14);
        interactText.setFillColor(panelMenuOpen ? sf::Color(255, 220, 140) : sf::Color(190, 190, 210));
        interactText.setPosition(12.f, 87.f);

        if (panelMenuOpen)
            interactText.setString("Panel open");
        else if (nearInteractable)
            interactText.setString("E = interact");
        else
            interactText.setString("Walk into papers and batteries to pick them up");

        window.draw(interactText);
    } else {
        sf::RectangleShape modeRect(sf::Vector2f(220.f, 18.f));
        modeRect.setPosition(12.f, 40.f);
        modeRect.setFillColor(areaScanHeld ? sf::Color(0, 100, 0) : sf::Color(100, 0, 0));
        window.draw(modeRect);

        sf::RectangleShape batteryRect(sf::Vector2f(220.f, 14.f));
        batteryRect.setPosition(12.f, 65.f);
        batteryRect.setFillColor(sf::Color(50, 50, 50));
        window.draw(batteryRect);
    }
}

// sets up window, OpenGL, loads map, runs game loop
int main() {
    std::srand((unsigned)std::time(nullptr)); // seed random generator for scan
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antialiasingLevel = 4;
    settings.majorVersion = 2;
    settings.minorVersion = 1;

    sf::RenderWindow window(
        sf::VideoMode(1280, 720),
        "LiDAR Escape",
        sf::Style::Default,
        settings
    );

    window.setVerticalSyncEnabled(true);
    window.setMouseCursorVisible(true);
    window.setMouseCursorGrabbed(false);

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat lightPos[] = {0.f, 2.2f, 0.f, 1.f};
    GLfloat lightDiffuse[] = {0.015f, 0.015f, 0.02f, 1.f};
    GLfloat lightAmbient[] = {0.f, 0.f, 0.f, 1.f};
    GLfloat globalAmbient[] = {0.f, 0.f, 0.f, 1.f};

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    MapLoader map;
    int currentLevel = 1;

    auto loadLevelData = [&](int levelIndex) {
        MapLoader newMap;
        std::string filename = "map-" + std::to_string(levelIndex) + ".txt";
        bool loaded = newMap.load(filename);

        if (!loaded && levelIndex == 1) {
            loaded = newMap.load("map.txt");
        }
        if (!loaded && levelIndex == 1) {
            newMap.loadFallbackMap();
            loaded = true;
        }
        if (!loaded) {
            return false;
        }

        map = newMap;
        currentLevel = levelIndex;
        return true;
    };

    if (!loadLevelData(1)) {
        std::cout << "Could not load any starting map, using fallback map\n";
        map.loadFallbackMap();
        currentLevel = 1;
    }

    Camera cam;
    cam.pos = map.spawn;
    cam.updateFront();

    PointCloud cloud;

    sf::SoundBuffer footstepBuffer;
    sf::SoundBuffer stalkerStepBuffer;
    sf::Sound footstepSound;
    sf::Sound stalkerStepSound;
    sf::Music ambienceMusic;
    bool footstepLoaded = false;
    bool stalkerStepLoaded = false;
    bool ambienceLoaded = false;
    float footstepTimer = 0.f;
    float stalkerAudioTimer = 0.f;
    bool playerWasMoving = false;

    footstepLoaded = footstepBuffer.loadFromFile("assets/audio/footstep.wav");
    if (footstepLoaded) {
        footstepSound.setBuffer(footstepBuffer);
        footstepSound.setVolume(28.f);
    } else {
        std::cout << "Missing placeholder audio: assets/audio/footstep.wav";
    }

    stalkerStepLoaded = stalkerStepBuffer.loadFromFile("assets/audio/stalker.wav");
    if (stalkerStepLoaded) {
        stalkerStepSound.setBuffer(stalkerStepBuffer);
        stalkerStepSound.setVolume(42.f);
    } else {
        std::cout << "Missing placeholder audio: assets/audio/stalker.wav";
    }

    ambienceLoaded = ambienceMusic.openFromFile("assets/audio/ambience.wav");
    if (ambienceLoaded) {
        ambienceMusic.setLoop(true);
        ambienceMusic.setVolume(22.f);
    } else {
        std::cout << "Missing placeholder audio: assets/audio/ambience.wav";
    }

    float battery = 100.f;
    float batteryDepletedTimer = 0.f;
    bool localScanHeld = false;
    bool areaScanHeld = false;
    bool parasiteActive = false;
    float parasiteTimer = 0.f;
    float parasiteShakeMeter = 0.f;
    float parasiteCooldown = 7.5f;
    float parasiteOverlay = 0.f;
    float parasiteMotionTime = 0.f;
    float parasiteShakeDirection = 0.f;
    bool interactPressed = false;
    bool panelMenuOpen = false;
    int activePanelIndex = -1;
    std::string panelInput;
    std::string panelMessage;
    std::string gameOverReason = "You were caught by the spikes";
    bool scanAlarmFlash = false;
    const float passiveRecharge = 3.5f;
    const float localScanDrain = 3.0f;
    const float areaScanDrain = 4.5f;
    const float playerRadius = 0.28f;

    GameState state = GameState::MainMenu;

    Settings cfg;
    std::vector<sf::FloatRect> menuBounds;

    auto allSwitchesDone = [&]() -> bool {
        if (map.switches.empty()) return true;
        return std::all_of(map.switches.begin(), map.switches.end(), [](const Switch& sw) {
            return sw.activated;
        });
    };

    auto updateDoorsFromProgress = [&]() {
        bool switchesDone = allSwitchesDone();

        for (auto& door : map.doors) {
            bool shouldOpen = false;

            if (door.requirement == DoorRequirement::SwitchOnly) {
                shouldOpen = switchesDone;
            } else if (door.requirement == DoorRequirement::PanelOnly) {
                shouldOpen = door.panelUnlocked;
            } else {
                shouldOpen = switchesDone && door.panelUnlocked;
            }

            door.open = shouldOpen;
            if (door.open) {
                door.revealTimer = std::max(door.revealTimer, 2.4f);
            }
        }
    };

    auto makeCollectedCodeMasked = [&]() -> std::string {
        // Find the expected code length from the first panel that has a code
        int maxPos = 0;
        for (const auto& panel : map.panels) {
            if ((int)panel.code.length() > maxPos) {
                maxPos = (int)panel.code.length();
            }
        }

        if (maxPos == 0) return "____";

        std::string code(maxPos, '_');
        for (const auto& paper : map.papers) {
            if (paper.collected && paper.position >= 1 && paper.position <= maxPos) {
                code[paper.position - 1] = paper.symbol[0];
            }
        }
        // Format with spaces between digits for readability
        std::string formatted;
        for (size_t i = 0; i < code.length(); ++i) {
            if (i > 0) formatted += " ";
            formatted += code[i];
        }
        return formatted;
    };

    auto applySpawnState = [&]() {
        cam.pos = map.spawn;
        cam.yaw = -1.5707963f;
        cam.pitch = 0.f;
        cam.mouseSens = cfg.mouseSens();
        cam.fovY = cfg.fov();
        cam.updateFront();
        battery = 100.f;
        batteryDepletedTimer = 0.f;
        localScanHeld = false;
        areaScanHeld = false;
        parasiteActive = false;
        parasiteTimer = 0.f;
        parasiteShakeMeter = 0.f;
        parasiteCooldown = 7.5f;
        parasiteOverlay = 0.f;
        parasiteMotionTime = 0.f;
        parasiteShakeDirection = 0.f;
        footstepTimer = 0.f;
        stalkerAudioTimer = 0.f;
        playerWasMoving = false;
        if (footstepSound.getStatus() == sf::Sound::Playing) footstepSound.stop();
        if (stalkerStepSound.getStatus() == sf::Sound::Playing) stalkerStepSound.stop();
        if (ambienceLoaded && ambienceMusic.getStatus() != sf::Music::Playing) ambienceMusic.play();
        interactPressed = false;
        panelMenuOpen = false;
        activePanelIndex = -1;
        panelInput.clear();
        panelMessage.clear();
        cloud.points.clear();
        for (auto& spike : map.spikes) spike.revealTimer = 0.f;
        for (auto& sw : map.switches) {
            sw.activated = false;
            sw.revealTimer = 0.f;
        }
        for (auto& panel : map.panels) {
            panel.revealTimer = 0.f;
        }
        for (auto& paper : map.papers) {
            paper.collected = false;
            paper.revealTimer = 0.f;
        }
        for (auto& batteryPickup : map.batteries) {
            batteryPickup.collected = false;
            batteryPickup.revealTimer = 0.f;
        }
        for (auto& stalker : map.stalkers) {
            stalker.pos = stalker.initialPos;
            stalker.alerted = false;
            stalker.revealTimer = 0.f;
            stalker.alertVisualTimer = 0.f;
        }
        for (auto& door : map.doors) {
            door.open = false;
            door.revealTimer = 0.f;
            door.panelUnlocked = false;
        }
        gameOverReason = "You were caught by the spikes";
        scanAlarmFlash = false;
        updateDoorsFromProgress();
        sf::Vector2u sz = window.getSize();
        sf::Mouse::setPosition(sf::Vector2i((int)sz.x / 2, (int)sz.y / 2), window);
    };

    auto enterPlaying = [&]() {
        state = GameState::Playing;
        if (ambienceLoaded && ambienceMusic.getStatus() != sf::Music::Playing) ambienceMusic.play();
        if (!panelMenuOpen) {
            window.setMouseCursorVisible(false);
            window.setMouseCursorGrabbed(true);
        }
    };

    auto closePanelMenu = [&]() {
        panelMenuOpen = false;
        activePanelIndex = -1;
        panelInput.clear();
        panelMessage.clear();
        if (state == GameState::Playing) {
            window.setMouseCursorVisible(false);
            window.setMouseCursorGrabbed(true);
            sf::Vector2u sz = window.getSize();
            sf::Mouse::setPosition(sf::Vector2i((int)sz.x / 2, (int)sz.y / 2), window);
        }
    };

    auto openPanelMenu = [&](int panelIndex) {
        panelMenuOpen = true;
        activePanelIndex = panelIndex;
        panelInput.clear();
        panelMessage.clear();
        localScanHeld = false;
        areaScanHeld = false;
        window.setMouseCursorVisible(true);
        window.setMouseCursorGrabbed(false);
    };

    auto startGame = [&]() {
        loadLevelData(1);
        applySpawnState();
        enterPlaying();
    };

    auto restartLevel = [&]() {
        applySpawnState();
        enterPlaying();
    };

    auto goToNextLevel = [&]() {
        int nextLevel = currentLevel + 1;
        if (loadLevelData(nextLevel)) {
            applySpawnState();
            enterPlaying();
        } else {
            state = GameState::GameComplete;
            window.setMouseCursorVisible(true);
            window.setMouseCursorGrabbed(false);
            if (ambienceMusic.getStatus() == sf::Music::Playing) ambienceMusic.stop();
        }
    };

    sf::Clock clock;
    sf::Vector2u size = window.getSize();

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (event.type == sf::Event::Resized) {
                glViewport(0, 0, event.size.width, event.size.height);
                size = window.getSize();
            }

            if (state == GameState::MainMenu) {
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                    window.close();

                if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2i mp(event.mouseButton.x, event.mouseButton.y);
                    for (int i = 0; i < (int)menuBounds.size(); ++i) {
                        if (!menuBounds[i].contains((float)mp.x, (float)mp.y)) continue;
                        if      (i == 0) startGame();
                        else if (i == 1) state = GameState::Settings;
                        else if (i == 2) window.close();
                        break;
                    }
                }
            }
            else if (state == GameState::Settings) {
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                    state = GameState::MainMenu;

                if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2i mp(event.mouseButton.x, event.mouseButton.y);
                    for (int i = 0; i < (int)menuBounds.size(); ++i) {
                        if (!menuBounds[i].contains((float)mp.x, (float)mp.y)) continue;
                        if      (i == 0) cfg.cycleSens();
                        else if (i == 1) cfg.cycleFov();
                        else if (i == 2) state = GameState::MainMenu;
                        break;
                    }
                }
            }
            else if (state == GameState::GameOver) {
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                    state = GameState::MainMenu;

                if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2i mp(event.mouseButton.x, event.mouseButton.y);
                    if (menuBounds.size() >= 1 && menuBounds[0].contains((float)mp.x, (float)mp.y)) {
                        restartLevel();
                    } else if (menuBounds.size() >= 2 && menuBounds[1].contains((float)mp.x, (float)mp.y)) {
                        window.close();
                    }
                }
            }
            else if (state == GameState::GameComplete) {
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                    state = GameState::MainMenu;

                if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2i mp(event.mouseButton.x, event.mouseButton.y);
                    if (menuBounds.size() >= 1 && menuBounds[0].contains((float)mp.x, (float)mp.y)) {
                        state = GameState::MainMenu;
                    } else if (menuBounds.size() >= 2 && menuBounds[1].contains((float)mp.x, (float)mp.y)) {
                        window.close();
                    }
                }
            }
            else {
                if (panelMenuOpen) {
                    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                        closePanelMenu();
                    } else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::BackSpace) {
                        if (!panelInput.empty()) panelInput.pop_back();
                    } else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
                        bool switchesDone = allSwitchesDone();
                        bool unlockedAny = false;
                        bool panelNeedsSwitches = false;

                        for (auto& door : map.doors) {
                            if (door.panelIndex != activePanelIndex) continue;
                            if (door.requirement == DoorRequirement::SwitchOnly) continue;

                            if (door.requirement == DoorRequirement::SwitchAndPanel) {
                                panelNeedsSwitches = true;
                                if (!switchesDone) continue;
                            }

                            if (panelInput == door.code) {
                                door.panelUnlocked = true;
                                unlockedAny = true;
                                door.revealTimer = std::max(door.revealTimer, 3.0f);
                            }
                        }

                        if (unlockedAny) {
                            panelMessage = "Correct code";
                            updateDoorsFromProgress();
                            closePanelMenu();
                        } else if (panelNeedsSwitches && !switchesDone) {
                            panelMessage = "Activate all switches first";
                        } else {
                            panelMessage = "Wrong code";
                        }
                    } else if (event.type == sf::Event::TextEntered) {
                        char c = static_cast<char>(event.text.unicode);
                        if (event.text.unicode >= 32 && event.text.unicode < 127) {
                            if (std::isalnum(static_cast<unsigned char>(c)) && panelInput.size() < 16) {
                                panelInput.push_back(c);
                            }
                        }
                    }
                } else {
                    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                        state = GameState::MainMenu;
                        localScanHeld = false;
                        areaScanHeld = false;
                        if (footstepSound.getStatus() == sf::Sound::Playing) footstepSound.stop();
                        if (stalkerStepSound.getStatus() == sf::Sound::Playing) stalkerStepSound.stop();
                        if (ambienceMusic.getStatus() == sf::Music::Playing) ambienceMusic.pause();
                        window.setMouseCursorVisible(true);
                        window.setMouseCursorGrabbed(false);
                    }

                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)
                        localScanHeld = true;
                    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
                        localScanHeld = false;

                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right)
                        areaScanHeld = true;
                    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right)
                        areaScanHeld = false;

                    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::E)
                        interactPressed = true;

                    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::P) {
                        parasiteActive = true;
                        parasiteTimer = 0.f;
                        parasiteShakeMeter = 0.f;
                        parasiteOverlay = 0.f;
                        parasiteMotionTime = 0.f;
                        parasiteShakeDirection = 0.f;
                    }
                }
            }
        }

        float dt = clock.restart().asSeconds();
        dt = std::min(dt, 0.033f);

        if (state == GameState::Playing) {
            if (!panelMenuOpen) {
                sf::Vector2i center((int)size.x / 2, (int)size.y / 2);
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                sf::Vector2i delta = mousePos - center;
                sf::Mouse::setPosition(center, window);

                if (parasiteActive) {
                    float horizontal = (float)delta.x;
                    float strongMove = std::max(0.f, std::abs(horizontal) - 9.f);
                    float dir = (horizontal > 0.f) ? 1.f : ((horizontal < 0.f) ? -1.f : 0.f);
                    if (strongMove > 0.f && dir != 0.f) {
                        if (parasiteShakeDirection != 0.f && dir != parasiteShakeDirection) {
                            parasiteShakeMeter = clampf(parasiteShakeMeter + strongMove * 0.014f, 0.f, 1.35f);
                        } else {
                            parasiteShakeMeter = clampf(parasiteShakeMeter + strongMove * 0.0035f, 0.f, 1.35f);
                        }
                        parasiteShakeDirection = dir;
                    } else {
                        parasiteShakeMeter = std::max(0.f, parasiteShakeMeter - dt * 0.55f);
                    }
                }

                cam.yaw += (float)delta.x * cam.mouseSens;
                cam.pitch -= (float)delta.y * cam.mouseSens;
                cam.pitch = clampf(cam.pitch, -1.50f, 1.50f);
                cam.updateFront();

                Vec3 forward = cam.front;
                forward.y = 0.f;
                forward = normalize(forward);

                Vec3 right = normalize(Vec3{-forward.z, 0.f, forward.x});
                Vec3 move{0.f, 0.f, 0.f};

                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) move = move + forward;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) move = move - forward;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) move = move - right;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) move = move + right;

                if (lengthVec(move) > 0.001f) move = normalize(move) * cam.moveSpeed;

                bool playerMoving = lengthVec(move) > 0.001f;

                if (playerMoving) {
                    if (!playerWasMoving) {
                        footstepTimer = 0.f;
                    }
                    footstepTimer -= dt;
                    if (footstepLoaded && footstepTimer <= 0.f) {
                        footstepSound.play();
                        footstepTimer = 0.42f;
                    }
                } else {
                    if (playerWasMoving && footstepSound.getStatus() == sf::Sound::Playing) {
                        footstepSound.stop();
                    }
                    footstepTimer = 0.f;
                }
                playerWasMoving = playerMoving;

                Vec3 deltaMove = move * dt;
                moveWithSlide(cam.pos, deltaMove, playerRadius, map.boxes);
                cam.pos.y = map.spawn.y;
            }

            updateStalkers(map.stalkers, cam.pos, playerRadius, dt, map.boxes);

            float nearestAlertedStalkerDist = 9999.f;
            bool anyAlertedStalker = false;
            for (const auto& stalker : map.stalkers) {
                if (!stalker.active || !stalker.alerted) continue;
                anyAlertedStalker = true;
                float dx = cam.pos.x - stalker.pos.x;
                float dz = cam.pos.z - stalker.pos.z;
                float d = std::sqrt(dx * dx + dz * dz);
                nearestAlertedStalkerDist = std::min(nearestAlertedStalkerDist, d);
            }
            if (anyAlertedStalker) {
                stalkerAudioTimer -= dt;
                float stalkerVolume = 18.f;
                if (nearestAlertedStalkerDist < 14.f) {
                    stalkerVolume = 18.f + (1.f - clampf((nearestAlertedStalkerDist - 1.4f) / (14.f - 1.4f), 0.f, 1.f)) * 55.f;
                }
                if (stalkerStepLoaded) {
                    stalkerStepSound.setVolume(stalkerVolume);
                    if (stalkerAudioTimer <= 0.f) {
                        stalkerStepSound.play();
                        stalkerAudioTimer = 0.55f;
                    }
                }
            } else {
                stalkerAudioTimer = 0.f;
            }

            if (collidesWithAnySpike(cam.pos, playerRadius, map.spikes)) {
                localScanHeld = false;
                areaScanHeld = false;
                interactPressed = false;
                panelMenuOpen = false;
                gameOverReason = "You were caught by the spikes";
                state = GameState::GameOver;
                window.setMouseCursorVisible(true);
                window.setMouseCursorGrabbed(false);
            }

            if (state == GameState::Playing && collidesWithAnyStalker(cam.pos, playerRadius, map.stalkers)) {
                localScanHeld = false;
                areaScanHeld = false;
                interactPressed = false;
                panelMenuOpen = false;
                gameOverReason = "You were caught by the stalker";
                state = GameState::GameOver;
                window.setMouseCursorVisible(true);
                window.setMouseCursorGrabbed(false);
            }

            if (state == GameState::Playing) {
                parasiteCooldown = std::max(0.f, parasiteCooldown - dt);
                if (!parasiteActive && parasiteCooldown <= 0.f) {
                    float eventChance = dt * 0.018f;
                    if (((float)std::rand() / (float)RAND_MAX) < eventChance) {
                        parasiteActive = true;
                        parasiteTimer = 0.f;
                        parasiteShakeMeter = 0.f;
                        parasiteOverlay = 0.f;
                        parasiteMotionTime = 0.f;
                        parasiteShakeDirection = 0.f;
                    }
                }

                if (parasiteActive) {
                    parasiteTimer += dt;
                    parasiteMotionTime += dt;
                    battery = std::max(0.f, battery - (8.5f * dt));
                    parasiteOverlay = std::min(1.f, parasiteOverlay + dt * 1.2f);
                    if (parasiteShakeMeter >= 1.f) {
                        parasiteActive = false;
                        parasiteTimer = 0.f;
                        parasiteShakeMeter = 0.f;
                        parasiteCooldown = 14.f + ((float)std::rand() / (float)RAND_MAX) * 12.f;
                        parasiteShakeDirection = 0.f;
                    }
                } else {
                    parasiteOverlay = std::max(0.f, parasiteOverlay - dt * 0.9f);
                }

                for (auto& paper : map.papers) {
                    if (!paper.collected && isNearPaperPickup(cam.pos, paper)) {
                        paper.collected = true;
                        paper.revealTimer = 0.f;
                    }
                }

                for (auto& batteryPickup : map.batteries) {
                    if (!batteryPickup.collected && isNearBatteryPickup(cam.pos, batteryPickup)) {
                        battery = clampf(battery + batteryPickup.chargeAmount, 0.f, 100.f);
                        batteryPickup.collected = true;
                        batteryPickup.revealTimer = 0.f;
                    }
                }
            }

            if (state == GameState::Playing && !panelMenuOpen && interactPressed) {
                bool handled = false;

                for (auto& sw : map.switches) {
                    if (!sw.activated && isNearSwitchInteraction(cam.pos, sw)) {
                        sw.activated = true;
                        sw.revealTimer = 3.0f;
                        updateDoorsFromProgress();
                        handled = true;
                        break;
                    }
                }

                if (!handled) {
                    for (int i = 0; i < (int)map.panels.size(); ++i) {
                        if (isNearPanelInteraction(cam.pos, map.panels[i])) {
                            openPanelMenu(i);
                            handled = true;
                            break;
                        }
                    }
                }

                interactPressed = false;
            }

            if (state == GameState::Playing) {
                for (const auto& door : map.doors) {
                    if (isNearOpenDoor(cam.pos, door)) {
                        goToNextLevel();
                        break;
                    }
                }
            }

            if (batteryDepletedTimer > 0.f) {
                batteryDepletedTimer -= dt;
                if (batteryDepletedTimer < 0.f) batteryDepletedTimer = 0.f;
            }

            bool scanningAllowed = battery > 0.f && batteryDepletedTimer <= 0.f;
            bool scanning = state == GameState::Playing && !panelMenuOpen && (localScanHeld || areaScanHeld) && scanningAllowed;

            if (scanning) {
                if (areaScanHeld) {
                    cloud.scan(cam, map.boxes, map.spikes, map.switches, map.doors, map.panels, map.papers, map.batteries, map.stalkers, 150, 120.f, 12.f);
                    battery -= areaScanDrain * dt;
                } else {
                    cloud.scan(cam, map.boxes, map.spikes, map.switches, map.doors, map.panels, map.papers, map.batteries, map.stalkers, 90, 32.f, 10.f);
                    battery -= localScanDrain * dt;
                }
                if (battery <= 0.f && batteryDepletedTimer <= 0.f) {
                    batteryDepletedTimer = 0.8f;
                }
                battery = clampf(battery, 0.f, 100.f);
            } else {
                battery += passiveRecharge * dt;
                battery = clampf(battery, 0.f, 100.f);
            }

            cloud.update(dt);
            updateSpikeReveal(map.spikes, dt);
            updateSwitchReveal(map.switches, dt);
            updateDoorReveal(map.doors, dt);
            updatePanelReveal(map.panels, dt);
            updatePaperReveal(map.papers, dt);
            updateBatteryReveal(map.batteries, dt);
            updateStalkerReveal(map.stalkers, dt);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (state == GameState::Playing || state == GameState::GameOver) {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(cam.fovY, (double)size.x / (double)size.y, 0.05, 100.0);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            gluLookAt(
                cam.pos.x, cam.pos.y, cam.pos.z,
                cam.pos.x + cam.front.x, cam.pos.y + cam.front.y, cam.pos.z + cam.front.z,
                0.0, 1.0, 0.0
            );

            glEnable(GL_LIGHTING);
            glColor3f(0.025f, 0.025f, 0.03f);
            for (const auto& wb : map.boxes) {
                drawBox(wb.box);
            }

            renderDoors(map.doors);
            renderPanels(map.panels);
            renderPapers(map.papers);
            renderBatteries(map.batteries);
            renderSwitches(map.switches);
            renderSpikes(map.spikes);
            renderStalkers(map.stalkers);
            cloud.render();

            int switchesActive = 0;
            for (const auto& sw : map.switches) if (sw.activated) ++switchesActive;

            int papersCollected = 0;
            for (const auto& paper : map.papers) if (paper.collected) ++papersCollected;

            bool nearInteractable = false;
            if (!panelMenuOpen) {
                for (const auto& sw : map.switches) {
                    if (!sw.activated && isNearSwitchInteraction(cam.pos, sw)) {
                        nearInteractable = true;
                        break;
                    }
                }
                if (!nearInteractable) {
                    for (const auto& panel : map.panels) {
                        if (isNearPanelInteraction(cam.pos, panel)) {
                            nearInteractable = true;
                            break;
                        }
                    }
                }
            }

            float nearestStalkerDist = 9999.f;
            for (const auto& stalker : map.stalkers) {
                if (!stalker.active) continue;
                float dx = cam.pos.x - stalker.pos.x;
                float dz = cam.pos.z - stalker.pos.z;
                float d = std::sqrt(dx * dx + dz * dz);
                nearestStalkerDist = std::min(nearestStalkerDist, d);
            }
            float stalkerGlitch = 0.f;
            if (nearestStalkerDist < 6.f) {
                stalkerGlitch = 1.f - clampf((nearestStalkerDist - 1.1f) / (6.f - 1.1f), 0.f, 1.f);
            }

            window.pushGLStates();
            renderBatteryHUD(
                window,
                battery,
                areaScanHeld,
                currentLevel,
                switchesActive,
                (int)map.switches.size(),
                papersCollected,
                (int)map.papers.size(),
                nearInteractable,
                panelMenuOpen
            );
            renderParasiteOverlay(window, parasiteOverlay, parasiteShakeMeter, parasiteMotionTime);
            renderStalkerGlitch(window, stalkerGlitch, scanAlarmFlash);
            if (panelMenuOpen) {
                bool switchesDone = allSwitchesDone();
                bool switchesRequired = false;
                for (const auto& door : map.doors) {
                    if (door.panelIndex == activePanelIndex && door.requirement == DoorRequirement::SwitchAndPanel) {
                        switchesRequired = true;
                        break;
                    }
                }
                renderPanelOverlay(window, makeCollectedCodeMasked(), panelInput, switchesRequired, switchesDone, panelMessage);
            }
            if (state == GameState::GameOver) {
                renderGameOverOverlay(window, menuBounds, gameOverReason);
            }
            window.popGLStates();
        } else {
            window.pushGLStates();
            if (state == GameState::MainMenu)
                renderMainMenu(window, menuBounds);
            else if (state == GameState::Settings)
                renderSettingsMenu(window, cfg, menuBounds);
            else if (state == GameState::GameComplete)
                renderGameCompleteOverlay(window, menuBounds);
            window.popGLStates();
        }

        window.display();
    }

    return 0;
}
