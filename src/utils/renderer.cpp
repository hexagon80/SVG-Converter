#include <svg.hpp>
#include <earcut.hpp>
#include <agg_curves.h>

using namespace geode::prelude;
using namespace svg;

// Moved here for clarity, all the geomety functions

// Creates a new Renderer color key.
uint32_t Renderer::NewColor(ccColor3B color) {
    uint32_t key = packColor(color);
    usedColors.insert(key);
    return key;
}

float Renderer::MiterExtension(const CCPoint& d0, const CCPoint& d1, float halfW) {
    // Flipped
    CCPoint n0(d0.y, -d0.x);
    CCPoint n1(d1.y, -d1.x);

    float crossD = d0.cross(d1);

    if (fabs(crossD) < 1e-6f) {
        return halfW;
    }

    CCPoint dn = n1 - n0;

    float t = (dn.x * d1.y - dn.y * d1.x) / crossD;

    float miterLen = t * halfW;

    miterLen = fabs(miterLen);

    const float MITER_LIMIT = 4.0f * halfW;
    if (miterLen > MITER_LIMIT) {
        miterLen = MITER_LIMIT;
    }

    return miterLen;
}

RenderResult Renderer::RenderSVG(){
        ToPlace.clear();
        ToPlace.reserve(50000);

        usedColors.clear();
        usedColors.reserve(50); // aprox.

        auto detail = config.quality;

        for (auto& shape : svg.shapes){
            auto fill = UnpackColor(shape.fillColor);
                if (fill.a == 0) shape.hasFill = false;

            auto stroke = UnpackColor(shape.strokeColor);
                if (stroke.a == 0) shape.hasStroke = false;

            uint32_t fillKey = 0;
            uint32_t strokeKey = 0;

            if (shape.hasFill)
                fillKey = NewColor(ccc3(fill.r, fill.g, fill.b));

            if (shape.hasStroke)
                strokeKey = NewColor(ccc3(stroke.r, stroke.g, stroke.b));

            Mpoly = shape.BuildMultiPolygon(detail);

            if (shape.hasFill) {
                for (auto& poly : Mpoly) {
                    auto tris = TriangulatePolygon(poly);

                    for (auto& tri : tris)
                        RenderTriangle(tri, fillKey);
                }
            }

            for (auto& path : shape.paths) {
                if (!shape.hasStroke) continue;

                path.pointify(detail);
                path.simplify(1.f / detail);

                RenderStroke(path, shape.strokeWidth, strokeKey);
            }
        }
        return {ToPlace, usedColors};
}

void Path::pointify(double roughness){
    // Approximated, to reduce massive allocs.
    points.reserve(points.size() + curves.size() * 8);

    for (auto& curve : curves){
        agg::curve4_div generator;
        generator.approximation_scale(roughness);

        generator.init(
            curve.p0.x, curve.p0.y,
            curve.p1.x,curve.p1.y,
            curve.p2.x, curve.p2.y,
            curve.p3.x, curve.p3.y
        );

        double x;
        double y;

        while (generator.vertex(&x, &y) != agg::path_cmd_stop) {
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            
            float Xf = static_cast<float>(x);
            float Yf = static_cast<float>(y);
            points.push_back({Xf, Yf});
        }
    }
    curves.clear();
    curves.shrink_to_fit();
}

void svg::simplifyPoints(std::vector<CCPoint> &points, float threshold){
    if (points.size() < 2) return;

    float maxDistance = 0.0f;
    size_t maxIndex = 0;

    auto perpendicularDistance = [](const CCPoint& p1, const CCPoint& p2, const CCPoint& pCheck)
    {
        float l2 = p1.getDistanceSq(p2);
        if (l2 == 0.0f) return pCheck.getDistance(p1);

        float t = (pCheck - p1).dot(p2 - p1) / l2;
        t = std::max(0.0f, std::min(1.0f, t));

        CCPoint projection = p1 + (p2 - p1) * t;
        return pCheck.getDistance(projection);
    };

    for (size_t i = 1; i < points.size() - 1; ++i) {
        float distance = perpendicularDistance(points.front(), points.back(), points[i]);
        if (distance > maxDistance) {
            maxDistance = distance;
            maxIndex = i;
        }
    }

    if (maxDistance <= threshold) {
        points = {points.front(), points.back()};
        return;
    }

    std::vector<CCPoint> left(points.begin(), points.begin() + maxIndex + 1);
    std::vector<CCPoint> right(points.begin() + maxIndex, points.end());

    simplifyPoints(left, threshold);
    simplifyPoints(right, threshold);

    left.insert(left.end(), right.begin() + 1, right.end());
    points = std::move(left);
}

BoostMultiPolygon Shape::BuildMultiPolygon(float detail) {
    std::vector<BoostRing> rings;
    for (auto& path : paths) {
        if (path.points.empty()) {
            path.pointify(detail);
            path.simplify(1.f / detail);
        }

        if (path.points.size() < 3) continue;

        BoostRing ring;

        for (auto& p : path.points)
            ring.push_back(BoostPoint(p.x , p.y));

        auto fixed = geometry::impl::correct(ring, boost::geometry::order_undetermined, 0.f);

        for (auto& [fixedRing, area] : fixed) {
            boost::geometry::correct(fixedRing);

            if (fixedRing.size() >= 4)
                rings.push_back(std::move(fixedRing));
        }
    }

    std::vector<int> depth(rings.size(), 0);

    for (size_t i = 0; i < rings.size(); ++i) {
        for (size_t j = 0; j < rings.size(); ++j) {
            if (i == j) continue;

            if (boost::geometry::covered_by(rings[i], rings[j])) {
                depth[i]++;
            }
        }
    }

    BoostMultiPolygon multiPolygon;

    for (size_t i = 0; i < rings.size(); ++i) {
        if (depth[i] % 2 != 0) continue;

        BoostPolygon poly;
        poly.outer() = rings[i];

        for (size_t j = 0; j < rings.size(); ++j) {
            if (i == j) continue;

            if (depth[j] % 2 == 1 &&
                boost::geometry::covered_by(rings[j], rings[i])) {
                poly.inners().push_back(rings[j]);
            }
        }

        multiPolygon.push_back(std::move(poly));
    }

    BoostMultiPolygon fixed;
    geometry::correct(multiPolygon, fixed, 1e-3);

    return fixed;
}

using EarcutPoint = std::array<double, 2>;

std::vector<std::array<CCPoint, 3>> Renderer::TriangulatePolygon(BoostPolygon& poly) {
    std::vector<std::vector<EarcutPoint>> polygon;

    polygon.emplace_back();
    auto& outer = poly.outer();

    for (size_t i = 0; i < outer.size() - 1; ++i) {
        polygon.back().push_back({outer[i].x(), outer[i].y()});
    }

    for (auto& hole : poly.inners()) {
        polygon.emplace_back();

        for (size_t i = 0; i < hole.size() - 1; ++i) {
            polygon.back().push_back({hole[i].x(), hole[i].y()});
        }
    }

    auto indices = mapbox::earcut<uint32_t>(polygon);

    std::vector<std::array<CCPoint, 3>> tris;
    std::vector<EarcutPoint> flat;

    for (auto& ring : polygon) {
        for (auto& p : ring)
            flat.push_back(p);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        CCPoint p0 = { (float)flat[indices[i]][0],     (float)flat[indices[i]][1]   };
        CCPoint p1 = { (float)flat[indices[i+1]][0],   (float)flat[indices[i+1]][1] };
        CCPoint p2 = { (float)flat[indices[i+2]][0],   (float)flat[indices[i+2]][1] };

        tris.push_back({p0, p1, p2});
    }

    return tris;
}

void Renderer::RenderTriangle(std::array<CCPoint, 3> &pts, int colorKey) {
    constexpr int KEY = 693;
    constexpr float SCALE = 30.0f; // Same for line
    constexpr int LINE_KEY = 211;
        
    auto p1 = pts[0] / SCALE;
    auto p2 = pts[1] / SCALE;
    auto p3 = pts[2] / SCALE;

    auto u = p2 - p1;
    auto v = p3 - p1;

    CCPoint i = -v;
    CCPoint j =  u;

    float iLen = i.getLength();
    float jLen = j.getLength();

    float sum = iLen + jLen;

    float iAngle = Renderer::AngleTo(i, {1.0f, 0.0f});
    float jAngle = Renderer::AngleTo(j, {0.0f, 1.0f});
        
    float rotX = std::fmod(jAngle * 180.f / M_PI + 360.f, 360.f);
    float rotY = std::fmod(iAngle * 180.f / M_PI + 360.f, 360.f);
        
    CCPoint center = (p2 + p3) * 0.5f;

    // In case that the hypotenuse blur is too visible,
    // create a line with renderStroke
    if (sum > (1 / config.quality)) {
        auto real2 = p2.lerp(p1, 0.005f);
        auto real3 = p3.lerp(p1, 0.005f);

        Path path{
        {real2 * SCALE, real3 * SCALE},
                false
        };

        RenderStroke(path, sum / 7.5, colorKey);
    }

    ObjCommand obj;

    obj.key = KEY;
    obj.pos = (center * SCALE) + config.position;
    obj.rotX = rotX;
    obj.rotY = rotY;
    obj.scaleX = iLen;
    obj.scaleY = jLen;
    obj.colorKey = colorKey;

    ToPlace.push_back(obj);
}

void Renderer::RenderStroke(Path& path, float strokeWidth, int colorKey) {
    constexpr int KEY = 211;

    auto& points = path.points;
    bool closed = path.closed;

    size_t n = points.size();

    if (n < 2) return;

    if (closed && n > 1 && points.front() == points.back()) {
        points.pop_back();
        n = points.size();
    }

    float halfW = strokeWidth * 0.5f;

    size_t count = closed ? n : n - 1;

    for (size_t i = 0; i < count; i++) {
        CCPoint A = points[i];
        CCPoint B = closed ? points[(i + 1) % n] : points[i + 1];

        CCPoint dir = B - A;
        float len = dir.getLength();
        if (len < 1e-6f) continue;

        dir /= len;

        float start = 0.f;
        float end = 0.f;
        if (i > 0) {
            CCPoint d0 = (A - points[i-1]).normalize();
            start = Renderer::MiterExtension(d0, dir, halfW);
        } else if (closed) {
            CCPoint d0 = (A - points[(i + n - 1) % n]).normalize();
            start = Renderer::MiterExtension(d0, dir, halfW);
        }

        if (i < n - 2 || closed) {
            CCPoint d1 = (points[(i + 2) % n] - B).normalize();
             end = Renderer::MiterExtension(dir, d1, halfW);
         }

        CCPoint A2 = A - dir * start;
        CCPoint B2 = B + dir * end;

        CCPoint seg = B2 - A2;
        float segLen = seg.getLength();

        if (segLen < 1e-4f) continue;

        float scaleX = segLen / 30.f;
        float scaleY = strokeWidth / 30.f;

        float angle = atan2(seg.y, seg.x);

        CCPoint center = (A2 + B2) * 0.5f;

        ObjCommand obj;

        obj.key = KEY;
        obj.pos = center + config.position;
        obj.rotX = -CC_RADIANS_TO_DEGREES(angle);
        obj.rotY = -CC_RADIANS_TO_DEGREES(angle);
        obj.scaleX = scaleX;
        obj.scaleY = scaleY;
        obj.colorKey = colorKey;

        ToPlace.push_back(obj);
    }
}