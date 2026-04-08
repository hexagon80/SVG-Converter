#pragma once

#include <Geode/Geode.hpp>

namespace svg {

// If simplier than cubic will be 0
struct Curve{
    cocos2d::CCPoint p0;
    cocos2d::CCPoint p1;
    cocos2d::CCPoint p2;
    cocos2d::CCPoint p3;
};

struct Path{
    std::vector<Curve> curves;
    std::vector<cocos2d::CCPoint> points;

    bool closed;

    void simplify(float threshold);
    void pointify(double roughness);

    // Vector of triangles, triangulated by earcut.hpp
    std::vector<std::array<cocos2d::CCPoint, 3>> Earcut();

    Path() = default;
    Path(std::vector<cocos2d::CCPoint> p) : points(p), closed(true) {};
    Path(std::vector<cocos2d::CCPoint> p, bool closed) : points(p), closed(closed) {};
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
    short Layer;
    cocos2d::CCPoint position = {0.f, 0.f};
};

struct ObjCommand {
    uint16_t key;

    cocos2d::CCPoint pos;

    float scaleX;
    float scaleY;

    float rotX;
    float rotY;

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
    
    void RenderTriangle(std::array<cocos2d::CCPoint, 3>& pts, int colorKey);

    void RenderStroke(Path& path, float strokeWidth, int colorKey);

    uint32_t NewColor(cocos2d::_ccColor3B color);

    // Unpacks a nanosvg color, packed 0xAABBGGRR
    static cocos2d::ccColor4B UnpackColor(unsigned int color);

    static float MiterExtension(const cocos2d::CCPoint& dir0, const cocos2d::CCPoint& dir1, float halfWidth);

    static float AngleTo(cocos2d::CCPoint const& a, cocos2d::CCPoint const& b);
};

int packColor(cocos2d::ccColor3B c);

cocos2d::ccColor3B UnpackColor(uint32_t c);

} //namespace svg