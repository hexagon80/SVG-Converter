#include <svg.hpp>

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
using namespace svg;

void Path::simplify(float threshold){
    simplifyPoints(points, threshold);
}

geode::Result<SVG> Parser::Parse(){
    NSVGimage* image = nsvgParseFromFile(
        string::pathToString(file).c_str(),
        "px",
        96.0f
    );
    SVG doc;

    if (!image) return Err("Unexpected pointer error on parsing!");

    if (!image->shapes)
        return Err("SVG has no shapes");

    // Those two ensure both the output is not fliped and centered in 0,0.
    const float hw = image->width  * 0.5f;
    const float hh = image->height * 0.5f;

    for (NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        Shape s;
        s.fillColor   = shape->fill.color;
        s.strokeColor = shape->stroke.color;
        s.strokeWidth = shape->strokeWidth;
        s.hasFill     = shape->fill.type != NSVG_PAINT_NONE;
        s.hasStroke   = shape->stroke.type != NSVG_PAINT_NONE &&
        shape->strokeWidth > 0.0f &&
        !config.ignoreStroke;
        
        for (NSVGpath* path = shape->paths; path; path = path->next){
            if (!path->pts || (path->npts < 4)) continue;

            Path p;
            p.closed = path->closed;
            
            float* pts = path->pts;

            for (int i = 0; i + 3 < path->npts; i += 3){
                Curve segment;

                segment.p0 = CCPoint{ pts[(i + 0) * 2] - hw, hh - pts[(i + 0) * 2 + 1] };
                segment.p1 = CCPoint{ pts[(i + 1) * 2] - hw, hh - pts[(i + 1) * 2 + 1] };
                segment.p2 = CCPoint{ pts[(i + 2) * 2] - hw, hh - pts[(i + 2) * 2 + 1] };
                segment.p3 = CCPoint{ pts[(i + 3) * 2] - hw, hh - pts[(i + 3) * 2 + 1] };
 
                p.curves.push_back(segment);
            }

            s.paths.push_back(std::move(p));
        }

        doc.shapes.push_back(std::move(s));
    }

    nsvgDelete(image);
    return Ok(doc);
}

_ccColor4B Renderer::UnpackColor(unsigned int color){
    // nanosvg colors are packed by 0xAABBGGRR NOT 0xAARRBBGG.
    // We ignore alpha, unless is invisible
    int r =  color        & 0xFF;
    int g = (color >> 8)  & 0xFF;
    int b = (color >> 16) & 0xFF;
    int a = (color >> 24) & 0xFF;

    if (a == 0) return ccc4(0, 0, 0, 0);

    return ccc4(r, g, b, 255);
}

float Renderer::AngleTo(CCPoint const& a, CCPoint const& b) {
    return std::atan2(a.x * b.y - b.x * a.y, a.dot(b));
}

int svg::packColor(ccColor3B c){
    return (c.r << 16) | (c.g << 8) | c.b;
}

ccColor3B svg::UnpackColor(uint32_t key) {
    ccColor3B c;

    c.r = (key >> 16) & 0xFF;
    c.g = (key >> 8) & 0xFF;
    c.b = key & 0xFF;

    return c;
}