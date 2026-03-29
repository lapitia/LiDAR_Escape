// SFML for window creation, input, and 2D rendering (HUD)
// OpenGL and GLU for 3D rendering
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
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

// MapLoader: handles loading a map from a text file and provides a fallback (hard‑coded map if the file is missing)
// one box per line: type minX minY minZ maxX maxY maxZ
class MapLoader {
public:
    std::vector<WorldBox> boxes;
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

    // build a simple test map when no map file exists
    void loadFallbackMap() {
        boxes.clear();
        int id = 0;
        addBox(id++, "floor",   {-8.f, -0.5f, -18.f}, {8.f, 0.f, 10.f});
        addBox(id++, "ceiling", {-8.f,  2.15f, -18.f}, {8.f, 2.45f, 10.f});
        addBox(id++, "wall", {-8.f, 0.f, -18.f}, {-7.5f, 2.15f, 10.f});
        addBox(id++, "wall", { 7.5f, 0.f, -18.f}, { 8.f, 2.15f, 10.f});
        addBox(id++, "wall", {-8.f, 0.f, -18.f}, { 8.f, 2.15f, -17.5f});
        addBox(id++, "wall", {-8.f, 0.f,  9.5f}, { 8.f, 2.15f, 10.f});
        addBox(id++, "wall", {-2.2f, 0.f, -14.f}, {-1.6f, 2.15f, 4.f});
        addBox(id++, "wall", { 1.6f, 0.f, -14.f}, { 2.2f, 2.15f, 4.f});
        addBox(id++, "wall", {-1.1f, 0.f, -6.0f}, {0.9f, 2.15f, -5.4f});
        addBox(id++, "wall", {-5.5f, 0.f, 4.f}, {-5.0f, 2.15f, 9.5f});
        addBox(id++, "wall", { 5.0f, 0.f, 4.f}, { 5.5f, 2.15f, 9.5f});
        addBox(id++, "wall", {-5.5f, 0.f, 9.0f}, { 5.5f, 2.15f, 9.5f});

        spawn = {0.f, 1.55f, 7.f};
    }

    // load map from a text file
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        boxes.clear();
        std::string line;
        int id = 0;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue; // skip empty and comment lines

            std::istringstream iss(line);
            std::string token;
            iss >> token;

            if (token == "spawn") {
                // spawn line: "spawn x y z"
                iss >> spawn.x >> spawn.y >> spawn.z;
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

    return (dx * dx + dy * dy + dz * dz) < (radius * radius);
}

// floor and ceiling are not solid, just visible
static bool isSolid(const AABB& box) {
    return box.type != "floor" && box.type != "ceiling";
}

// check if player collides with any solid box
static bool collidesAt(const Vec3& pos, float radius, const std::vector<WorldBox>& boxes) {
    for (const auto& wb : boxes) {
        if (!wb.box.active) continue;
        if (!isSolid(wb.box)) continue;
        if (sphereAABB(pos, radius, wb.box)) return true;
    }
    return false;
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
                float sign = (step.x > 0.f) ? 1.f : -1.f;
                float moved = 0.f;
                while (std::abs(moved) < std::abs(step.x)) {
                    float inc = std::min(0.01f, std::abs(step.x - moved));
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
                float sign = (step.z > 0.f) ? 1.f : -1.f;
                float moved = 0.f;
                while (std::abs(moved) < std::abs(step.z)) {
                    float inc = std::min(0.01f, std::abs(step.z - moved));
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
    void scan(const Camera& cam, const std::vector<WorldBox>& boxes, int rays, float coneDeg, float maxDist) {
        float coneRad = coneDeg * 3.1415926f / 180.f;
        float tanCone = std::tan(coneRad * 0.5f);

        Vec3 worldUp{0.f, 1.f, 0.f};
        Vec3 right = normalize(cross(cam.front, worldUp));
        // in case front is parallel to worldUp, fallback to a different right
        if (lengthVec(right) < 0.0001f) right = {1.f, 0.f, 0.f};
        Vec3 up = normalize(cross(right, cam.front));

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
                // check against every active box
                for (const auto& wb : boxes) {
                    if (!wb.box.active) continue;

                    if (sphereAABB(p, 0.08f, wb.box)) {
                        //found a hit
                        // refine the hit distance between the previous step and current
                        float lo = prevT;
                        float hi = t;

                        for (int k = 0; k < 5; ++k) {
                            float mid = 0.5f * (lo + hi);
                            Vec3 mp = cam.pos + dir * mid;

                            if (sphereAABB(mp, 0.08f, wb.box)) {
                                hi = mid;
                            } else {
                                lo = mid;
                            }
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

// shared font used by both the HUD and the menu screens
// previously a static-local inside renderBatteryHUD; promoted to file scope
// so a single load serves all callers — ensureFont() is idempotent
static sf::Font g_font;
static bool     g_fontLoaded    = false;
static bool     g_fontAttempted = false;

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

// game state enum: controls which screen is active at any given time
// MainMenu – title screen shown on launch and when the player presses Escape in-game
// Settings – from MainMenu; lets the player adjust sensitivity and FOV
// Playing – the actual game loop (3-D scene, scanning, movement)
enum class GameState { MainMenu, Settings, Playing };

// user-configurable values with small preset arrays
// cycleSens() and cycleFov() rotate forward through each list on every click
// the camera is updated with the chosen values when startGame() is called
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
    void cycleFov()  { fovIdx         = (fovIdx  + 1) % 5; }
};

// menu rendering helpers

// draws text centred at (cx, cy) using the shared font and returns its global bounds
// used both to place decorative titles and to position clickable item labels
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
    t.setOrigin(lb.left + lb.width  * 0.5f,
                lb.top  + lb.height * 0.5f);
    t.setPosition(cx, cy);
    window.draw(t);
    return t.getGlobalBounds();
}

// used to widen the hit-box around menu text, so small labels are easier to click
static sf::FloatRect expand(sf::FloatRect r, float dx, float dy) {
    return {r.left - dx, r.top - dy, r.width + 2.f * dx, r.height + 2.f * dy};
}

// render the main menu screen: title, subtitle, and three items (Start / Settings / Exit)
// fills boundsOut with one clickable hit-box per item, indexed in that order
// the caller checks boundsOut against MouseButtonReleased events to drive transitions
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
        drawCenteredText(window, "LiDAR Escape", 54,
                         W * 0.5f, H * 0.21f, sf::Color(228, 228, 235));
        drawCenteredText(window, "find the exit", 17,
                         W * 0.5f, H * 0.21f + 58.f, sf::Color(85, 85, 108));
    }

    static const char* kLabels[] = {"Start", "Settings", "Exit"};
    constexpr int kCount = 3;
    boundsOut.resize(kCount);

    const sf::Vector2i mp = sf::Mouse::getPosition(window);
    const float startY = H * 0.43f;
    const float step   = 64.f;

    for (int i = 0; i < kCount; ++i) {
        const float y = startY + (float)i * step;

        if (g_fontLoaded) {
            sf::Text t;
            t.setFont(g_font);
            t.setString(kLabels[i]);
            t.setCharacterSize(34);
            sf::FloatRect lb = t.getLocalBounds();
            t.setOrigin(lb.left + lb.width  * 0.5f,
                        lb.top  + lb.height * 0.5f);
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

// render the settings screen: Sensitivity / Field of View / Back
static void renderSettingsMenu(sf::RenderWindow& window,
                               const Settings& cfg,
                               std::vector<sf::FloatRect>& boundsOut)
{
    ensureFont();
    const float W = (float)window.getSize().x;
    const float H = (float)window.getSize().y;

    // full-screen dark background
    sf::RectangleShape bg(sf::Vector2f(W, H));
    bg.setFillColor(sf::Color(4, 4, 7));
    window.draw(bg);

    if (g_fontLoaded)
        drawCenteredText(window, "Settings", 48,
                         W * 0.5f, H * 0.19f, sf::Color(228, 228, 235));
    
    const std::string labels[3] = {
        std::string("Sensitivity:    < ") + cfg.sensLabel() + " >",
        std::string("Field of View:  < ") + cfg.fovLabel()  + " >",
        "Back"
    };

    boundsOut.resize(3);
    const sf::Vector2i mp = sf::Mouse::getPosition(window);
    const float startY = H * 0.38f;
    const float step   = 68.f;

    for (int i = 0; i < 3; ++i) {
        const float y = startY + (float)i * step;

        if (g_fontLoaded) {
            sf::Text t;
            t.setFont(g_font);
            t.setString(labels[i]);
            t.setCharacterSize(30);
            sf::FloatRect lb = t.getLocalBounds();
            t.setOrigin(lb.left + lb.width  * 0.5f,
                        lb.top  + lb.height * 0.5f);
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

    // small hint shown below the item list
    if (g_fontLoaded)
        drawCenteredText(window, "click a setting to cycle its value", 15,
                         W * 0.5f, H * 0.76f, sf::Color(60, 60, 82));
}

// render a simple battery bar plus scan mode status
static void renderBatteryHUD(sf::RenderWindow& window, float battery, bool areaMode) {
    sf::RectangleShape bg({220.f, 20.f});
    bg.setPosition(12.f, 12.f);
    bg.setFillColor(sf::Color(30, 10, 10));
    window.draw(bg);

    sf::RectangleShape fill({220.f * (battery / 100.f), 20.f});
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

        if (areaMode)
            modeText.setString("Scan Mode: AREA (120 deg, 150 rays)");
        else
            modeText.setString("Scan Mode: LOCAL (32 deg, 90 rays)");

        window.draw(modeText);

        sf::Text batteryText;
        batteryText.setFont(g_font);
        batteryText.setCharacterSize(14);
        batteryText.setFillColor(sf::Color::White);
        batteryText.setPosition(12.f, 65.f);

        char batteryStr[32];
        sprintf(batteryStr, "Battery: %.0f%%", battery);
        batteryText.setString(batteryStr);
        window.draw(batteryText);
    } else {
        sf::RectangleShape modeRect({220.f, 18.f});
        modeRect.setPosition(12.f, 40.f);
        modeRect.setFillColor(areaMode ? sf::Color(0, 100, 0) : sf::Color(100, 0, 0));
        window.draw(modeRect);

        sf::RectangleShape batteryRect({220.f, 14.f});
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

    window.setVerticalSyncEnabled(true); // limit framerate to monitor refresh
    window.setMouseCursorVisible(true);
    window.setMouseCursorGrabbed(false);

    //start with a clear window
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // eable simple lighting. using a single light (LIGHT0) positioned above
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
    if (!map.load("map.txt")) {
        std::cout << "Could not load map.txt, using fallback map\n";
        map.loadFallbackMap();
    }

    // initialize camera at the map's spawn point
    Camera cam;
    cam.pos = map.spawn;
    cam.updateFront();

    PointCloud cloud;

    float battery = 100.f; // start with full battery
    bool localScanHeld = false; // true while left mouse button is pressed
    const float passiveRecharge = 3.5f; // % per second when not scanning
    const float localScanDrain = 18.0f; // % per second while scanning
    const float playerRadius = 0.28f; // collision sphere radius

    bool areaMode = false; // false = local scan, true = area scan

    // current screen: starts on the main menu; transitions to Playing on Start
    GameState state = GameState::MainMenu;

    // applied to the camera when startGame() runs
    Settings cfg;
    std::vector<sf::FloatRect> menuBounds;

    // resets all game-session data
    auto startGame = [&]() {
        cam.pos       = map.spawn;
        cam.mouseSens = cfg.mouseSens();
        cam.fovY      = cfg.fov();
        cam.updateFront();
        battery       = 100.f;
        localScanHeld = false;
        cloud.points.clear();
        state = GameState::Playing;
        window.setMouseCursorVisible(false);
        window.setMouseCursorGrabbed(true); // keep mouse inside window
        sf::Vector2u sz = window.getSize();
        sf::Mouse::setPosition(sf::Vector2i((int)sz.x / 2, (int)sz.y / 2), window);
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

            // main menu events
            if (state == GameState::MainMenu) {
                // escape on the main menu exits the application
                if (event.type == sf::Event::KeyPressed &&
                    event.key.code == sf::Keyboard::Escape)
                    window.close();

                if (event.type == sf::Event::MouseButtonReleased &&
                    event.mouseButton.button == sf::Mouse::Left)
                {
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

            // settings screen events
            else if (state == GameState::Settings) {
                // escape on the settings screen goes back to the main menu
                if (event.type == sf::Event::KeyPressed &&
                    event.key.code == sf::Keyboard::Escape)
                    state = GameState::MainMenu;

                if (event.type == sf::Event::MouseButtonReleased &&
                    event.mouseButton.button == sf::Mouse::Left)
                {
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

            // in-game events
            else {
                // escape in-game returns to the main menu without quitting
                if (event.type == sf::Event::KeyPressed &&
                    event.key.code == sf::Keyboard::Escape)
                {
                    state = GameState::MainMenu;
                    window.setMouseCursorVisible(true);
                    window.setMouseCursorGrabbed(false);
                }

                // left mouse button controls the scan
                if (event.type == sf::Event::MouseButtonPressed &&
                    event.mouseButton.button == sf::Mouse::Left)
                    localScanHeld = true;

                if (event.type == sf::Event::MouseButtonReleased &&
                    event.mouseButton.button == sf::Mouse::Left)
                    localScanHeld = false;

                // press M to toggle local/area scan mode
                if (event.type == sf::Event::KeyPressed &&
                    event.key.code == sf::Keyboard::M)
                    areaMode = !areaMode;
            }
        }

        // delta time (cap at 0.033s ≈ 30 fps for now)
        float dt = clock.restart().asSeconds();
        dt = std::min(dt, 0.033f);

        // game logic runs only while Playing; menus need no per-frame simulation
        if (state == GameState::Playing) {
            // mouse look: compute mouse movement from window center, update orientation
            sf::Vector2i center((int)size.x / 2, (int)size.y / 2);
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            sf::Vector2i delta = mousePos - center;
            sf::Mouse::setPosition(center, window);

            cam.yaw += (float)delta.x * cam.mouseSens;
            cam.pitch -= (float)delta.y * cam.mouseSens; // subtract because y increases downward
            cam.pitch = clampf(cam.pitch, -1.50f, 1.50f); // limit to ~±85° to avoid gimbal lock
            // wiki: Gimbal lock is the loss of one degree of freedom in a multi-dimensional mechanism
            // at certain alignments of the axes. In a three-dimensional three-gimbal mechanism,
            // gimbal lock occurs when the axes of two of the gimbals are driven into a parallel configuration,
            // "locking" the system into rotation in a degenerate two-dimensional space.

            cam.updateFront();

            // movement: compute desired move direction based on keys
            Vec3 forward = cam.front;
            forward.y = 0.f; // project onto horizontal plane
            forward = normalize(forward);

            Vec3 right = normalize(Vec3{-forward.z, 0.f, forward.x}); // perpendicular horizontal
            Vec3 move{0.f, 0.f, 0.f};

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) move = move + forward;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) move = move - forward;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) move = move - right;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) move = move + right;

            if (lengthVec(move) > 0.001f) {
                move = normalize(move) * cam.moveSpeed;
            }

            // apply movement with sliding collision
            Vec3 deltaMove = move * dt;
            moveWithSlide(cam.pos, deltaMove, playerRadius, map.boxes);
            // keep camera Y fixed to spawn height y
            cam.pos.y = map.spawn.y;

            // scanning and battery management
            if (localScanHeld && battery > 0.f) {
                if (areaMode) {
                    // wider and denser scan mode
                    cloud.scan(cam, map.boxes, 150, 120.f, 12.f);
                } else {
                    // generate 90 rays within a 32° cone, up to 10 units distance
                    cloud.scan(cam, map.boxes, 90, 32.f, 10.f);
                }
                battery -= localScanDrain * dt;
            } else {
                battery += passiveRecharge * dt;
            }

            battery = clampf(battery, 0.f, 100.f);
            cloud.update(dt);
        }

        // 3d render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (state == GameState::Playing) {
            // set up camera projection and view matrices
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

            // draw all world boxes
            glEnable(GL_LIGHTING);
            glColor3f(0.025f, 0.025f, 0.03f); // very dark gray/blue
            for (const auto& wb : map.boxes) {
                drawBox(wb.box);
            }

            // draw the point cloud
            cloud.render();

            // 2d render
            // SFML's 2D rendering uses its own OpenGL state; using push and pop to avoid conflicts
            window.pushGLStates();
            renderBatteryHUD(window, battery, areaMode);
            window.popGLStates();

        } else {
            // 2d menu render (no 3d scene behind it)
            // SFML's 2D rendering uses its own OpenGL state; using push and pop to avoid conflicts
            window.pushGLStates();
            if (state == GameState::MainMenu)
                renderMainMenu(window, menuBounds);
            else
                renderSettingsMenu(window, cfg, menuBounds);
            window.popGLStates();
        }

        // swap buffers (display the rendered frame)
        window.display();
    }

    return 0;
}
