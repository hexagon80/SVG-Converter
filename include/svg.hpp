#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace svg {

// If simplier than cubic will be 0
struct Curve{
    CCPoint p0;
    CCPoint p1;
    CCPoint p2;
    CCPoint p3;
};

struct Path{
    std::vector<Curve> curves;
    std::vector<CCPoint> points;

    bool closed;

    void simplify(float threshold);
    void pointify(double roughness);

    // Vector of triangles, triangulated by earcut.hpp
    std::vector<std::array<CCPoint, 3>> Earcut();

    Path() = default;
    Path(std::vector<CCPoint> p) : points(p), closed(true) {};
    Path(std::vector<CCPoint> p, bool closed) : points(p), closed(closed) {};
};

// Based on Allium's Ramer–Douglas–Peucker algorithm
void simplifyPoints(std::vector<cocos2d::CCPoint> &points, float threshold);

struct Shape{
    std::vector<Path> paths;
    unsigned int fillColor;
    unsigned int strokeColor;
    float strokeWidth;
    bool hasStroke;
    bool hasFill;
};

struct SVG{
    std::vector<Shape> shapes;
};

struct ParseOptions {
    float quality;
    bool ignoreStroke;
};

struct Parser{
    std::filesystem::path file;
    ParseOptions config;

    geode::Result<SVG> Parse();
};

struct RenderOptions {
    float quality;
    unsigned int Layer;
    CCPoint position = {0.f, 0.f};
};

struct ObjCommand {
    uint16_t key;

    CCPoint pos;

    float scaleX;
    float scaleY;

    float rotX;
    float rotY;

    uint8_t layer;

    uint32_t colorKey;
};

struct RenderResult {
    std::vector<ObjCommand> commands;
    std::unordered_set<uint32_t> usedColors;
};

struct Renderer{
    SVG svg;

    RenderOptions config;

    std::vector<ObjCommand> ToPlace;

    std::unordered_set<uint32_t> usedColors;
    
    RenderResult RenderSVG();

    void RenderFill(Path& path, int colorKey);
    
    void RenderTriangle(std::array<CCPoint, 3>& pts, int colorKey);

    void RenderStroke(Path& path, float strokeWidth, int colorKey);

    uint32_t NewColor(_ccColor3B color);

    // Unpacks a nanosvg color, packed 0xAABBGGRR
    static ccColor4B UnpackColor(unsigned int color);

    static float MiterExtension(const CCPoint& dir0, const CCPoint& dir1, float halfWidth);

    static float AngleTo(CCPoint const& a, CCPoint const& b);
};

int packColor(ccColor3B c);

ccColor3B UnpackColor(uint32_t c);

} //namespace svg