#include "graphics/preview/preview_sun_gizmo.h"

#include <algorithm>
#include <cmath>

#include <glad/glad.h>
#include <ncl_math/vec3.h>

#include "graphics/render/ScopedGlState.h"

namespace
{
    constexpr int kGizmoSize = 112;
    constexpr int kSphereSegments = 24;
    constexpr int kSphereRings = 12;

    void Vertex(const ncl_math::Vector3& point)
    {
        glVertex3fv(point.data());
    }
} // namespace

void PreviewSunGizmo_Draw(const PreviewCamera& camera, const PreviewLight& light, const PreviewRect& view)
{
    render::ScopedGlState saved;
    int side = std::min({kGizmoSize, view.wide, view.tall});

    glViewport(view.x + view.wide - side, view.y + view.tall - side, side, side);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);

    int units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);

    for (int unit = 0; unit < units; unit++)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glDisable(GL_TEXTURE_2D);
    }

    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -2, 2);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    ncl_math::Vector3 forward;
    ncl_math::Vector3 right;
    ncl_math::Vector3 up;
    ncl_math::AngleVectors(camera.angles, forward.data(), right.data(), up.data());
    ncl_math::Vector3 travel = ncl_math::Vector3::Ref(light.direction).Normalized();

    const auto project = [&forward, &right, &up](const ncl_math::Vector3& point) {
        return ncl_math::Vector3{point.Dot(right), point.Dot(up), -point.Dot(forward)};
    };

    ncl_math::Vector3 center = travel * -0.45f;
    ncl_math::Vector3 tangent =
        travel.Cross(std::abs(travel.z) < 0.9f ? ncl_math::Vector3{0, 0, 1} : ncl_math::Vector3{0, 1, 0}).Normalized();
    ncl_math::Vector3 bitangent = travel.Cross(tangent);

    glShadeModel(GL_SMOOTH);

    for (int ring = 0; ring < kSphereRings; ring++)
    {
        glBegin(GL_QUAD_STRIP);

        for (int segment = 0; segment <= kSphereSegments; segment++)
        {
            float longitude = 2 * ncl_math::kPi * segment / kSphereSegments;

            for (int row = 0; row < 2; row++)
            {
                float latitude = ncl_math::kPi * ((ring + row) / static_cast<float>(kSphereRings) - 0.5f);
                ncl_math::Vector3 normal{
                    std::cos(latitude) * std::cos(longitude), std::cos(latitude) * std::sin(longitude), std::sin(latitude)
                };
                float shade = 0.55f + 0.45f * std::max(0.0f, -normal.Dot(forward));
                glColor3f(light.color[0] * shade, light.color[1] * shade, light.color[2] * shade);
                Vertex(project(center + normal * 0.19f));
            }
        }

        glEnd();
    }

    glColor3fv(light.color);
    glLineWidth(1.5f);
    glBegin(GL_LINES);

    for (int ray = 0; ray < 5; ray++)
    {
        float angle = ray * 2 * ncl_math::kPi / 5;
        ncl_math::Vector3 offset = (tangent * std::cos(angle) + bitangent * std::sin(angle)) * 0.25f;
        ncl_math::Vector3 start = center + offset + travel * 0.1f;
        ncl_math::Vector3 end = center + offset + travel * 1.0f;
        Vertex(project(start));
        Vertex(project(end));
        Vertex(project(end));
        Vertex(project(end - travel * 0.17f + tangent * 0.07f));
        Vertex(project(end));
        Vertex(project(end - travel * 0.17f - tangent * 0.07f));
    }

    glEnd();
}
