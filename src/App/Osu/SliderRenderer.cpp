#include "SliderRenderer.h"

#include <limits>

#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "GameRules.h"
#include "OpenGLHeaders.h"
#include "OpenGLLegacyInterface.h"
#include "Osu.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Skin.h"
#include "VertexArrayObject.h"



Shader *SliderRenderer::BLEND_SHADER = NULL;

float SliderRenderer::MESH_CENTER_HEIGHT =
    0.5f;  // Camera::buildMatrixOrtho2D() uses -1 to 1 for zn/zf, so don't make this too high
int SliderRenderer::UNIT_CIRCLE_SUBDIVISIONS = 0;  // see osu_slider_body_unit_circle_subdivisions now
std::vector<float> SliderRenderer::UNIT_CIRCLE;
VertexArrayObject *SliderRenderer::UNIT_CIRCLE_VAO = NULL;
VertexArrayObject *SliderRenderer::UNIT_CIRCLE_VAO_BAKED = NULL;
VertexArrayObject *SliderRenderer::UNIT_CIRCLE_VAO_TRIANGLES = NULL;
float SliderRenderer::UNIT_CIRCLE_VAO_DIAMETER = 0.0f;

float SliderRenderer::fBoundingBoxMinX = (std::numeric_limits<float>::max)();
float SliderRenderer::fBoundingBoxMaxX = 0.0f;
float SliderRenderer::fBoundingBoxMinY = (std::numeric_limits<float>::max)();
float SliderRenderer::fBoundingBoxMaxY = 0.0f;

VertexArrayObject *SliderRenderer::generateVAO(const std::vector<Vector2> &points, float hitcircleDiameter,
                                               Vector3 translation, bool skipOOBPoints) {
    resourceManager->requestNextLoadUnmanaged();
    VertexArrayObject *vao = resourceManager->createVertexArrayObject();

    checkUpdateVars(hitcircleDiameter);

    const Vector3 xOffset = Vector3(hitcircleDiameter, 0, 0);
    const Vector3 yOffset = Vector3(0, hitcircleDiameter, 0);

    const bool debugSquareVao = cv::slider_debug_draw_square_vao.getBool();

    for(int i = 0; i < points.size(); i++) {
        // fuck oob sliders
        if(skipOOBPoints) {
            if(points[i].x < -hitcircleDiameter - GameRules::OSU_COORD_WIDTH * 2 ||
               points[i].x > osu->getScreenWidth() + hitcircleDiameter + GameRules::OSU_COORD_WIDTH * 2 ||
               points[i].y < -hitcircleDiameter - GameRules::OSU_COORD_HEIGHT * 2 ||
               points[i].y > osu->getScreenHeight() + hitcircleDiameter + GameRules::OSU_COORD_HEIGHT * 2)
                continue;
        }

        if(!debugSquareVao) {
            const std::vector<Vector3> &meshVertices = UNIT_CIRCLE_VAO_TRIANGLES->getVertices();
            const std::vector<std::vector<Vector2>> &meshTexCoords = UNIT_CIRCLE_VAO_TRIANGLES->getTexcoords();
            for(int v = 0; v < meshVertices.size(); v++) {
                vao->addVertex(meshVertices[v] + Vector3(points[i].x, points[i].y, 0) + translation);
                vao->addTexcoord(meshTexCoords[0][v]);
            }
        } else {
            const Vector3 topLeft =
                Vector3(points[i].x, points[i].y, 0) - xOffset / 2.0f - yOffset / 2.0f + translation;
            const Vector3 topRight = topLeft + xOffset;
            const Vector3 bottomLeft = topLeft + yOffset;
            const Vector3 bottomRight = bottomLeft + xOffset;

            vao->addVertex(topLeft);
            vao->addTexcoord(0, 0);

            vao->addVertex(bottomLeft);
            vao->addTexcoord(0, 1);

            vao->addVertex(bottomRight);
            vao->addTexcoord(1, 1);

            vao->addVertex(topLeft);
            vao->addTexcoord(0, 0);

            vao->addVertex(bottomRight);
            vao->addTexcoord(1, 1);

            vao->addVertex(topRight);
            vao->addTexcoord(1, 0);
        }
    }

    if(vao->getNumVertices() > 0)
        resourceManager->loadResource(vao);
    else
        debugLog("SliderRenderer::generateSliderVAO() ERROR: Zero triangles!\n");

    return vao;
}

void SliderRenderer::draw(const std::vector<Vector2> &points, const std::vector<Vector2> &alwaysPoints,
                          float hitcircleDiameter, float from, float to, Color undimmedColor, float colorRGBMultiplier,
                          float alpha, long sliderTimeForRainbow) {
    if(cv::slider_alpha_multiplier.getFloat() <= 0.0f || alpha <= 0.0f) return;

    checkUpdateVars(hitcircleDiameter);

    const int drawFromIndex = std::clamp<int>((int)std::round(points.size() * from), 0, points.size());
    const int drawUpToIndex = std::clamp<int>((int)std::round(points.size() * to), 0, points.size());

    // debug sliders
    if(cv::slider_debug_draw.getBool()) {
        const float circleImageScale = hitcircleDiameter / (float)osu->getSkin()->getHitCircle()->getWidth();
        const float circleImageScaleInv = (1.0f / circleImageScale);

        const float width = (float)osu->getSkin()->getHitCircle()->getWidth();
        const float height = (float)osu->getSkin()->getHitCircle()->getHeight();

        const float x = (-width / 2.0f);
        const float y = (-height / 2.0f);
        const float z = -1.0f;

        g->pushTransform();
        {
            g->scale(circleImageScale, circleImageScale);

            const Color dimmedColor = Colors::scale(undimmedColor, colorRGBMultiplier);

            g->setColor(dimmedColor);
            g->setAlpha(alpha * cv::slider_alpha_multiplier.getFloat());
            osu->getSkin()->getHitCircle()->bind();
            {
                for(int i = drawFromIndex; i < drawUpToIndex; i++) {
                    const Vector2 point = points[i] * circleImageScaleInv;

                    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
                    vao.empty();
                    {
                        vao.addTexcoord(0, 0);
                        vao.addVertex(point.x + x, point.y + y, z);

                        vao.addTexcoord(0, 1);
                        vao.addVertex(point.x + x, point.y + y + height, z);

                        vao.addTexcoord(1, 1);
                        vao.addVertex(point.x + x + width, point.y + y + height, z);

                        vao.addTexcoord(1, 0);
                        vao.addVertex(point.x + x + width, point.y + y, z);
                    }
                    g->drawVAO(&vao);
                }
            }
            osu->getSkin()->getHitCircle()->unbind();
        }
        g->popTransform();

        return;  // nothing more to draw here
    }

    // reset
    resetRenderTargetBoundingBox();

    // draw entire slider into framebuffer
    g->setDepthBuffer(true);
    g->setBlending(false);
    {
        osu->getSliderFrameBuffer()->enable();
        {
            const Color undimmedBorderColor =
                cv::slider_border_tint_combo_color.getBool() ? undimmedColor : osu->getSkin()->getSliderBorderColor();
            const Color undimmedBodyColor =
                osu->getSkin()->isSliderTrackOverridden() ? osu->getSkin()->getSliderTrackOverride() : undimmedColor;

            Color dimmedBorderColor;
            Color dimmedBodyColor;

            if(cv::slider_rainbow.getBool()) {
                float frequency = 0.3f;
                float time = engine->getTime() * 20;

                const Channel red1 = std::sin(frequency * time + 0 + sliderTimeForRainbow) * 127 + 128;
                const Channel green1 = std::sin(frequency * time + 2 + sliderTimeForRainbow) * 127 + 128;
                const Channel blue1 = std::sin(frequency * time + 4 + sliderTimeForRainbow) * 127 + 128;

                const Channel red2 = std::sin(frequency * time * 1.5f + 0 + sliderTimeForRainbow) * 127 + 128;
                const Channel green2 = std::sin(frequency * time * 1.5f + 2 + sliderTimeForRainbow) * 127 + 128;
                const Channel blue2 = std::sin(frequency * time * 1.5f + 4 + sliderTimeForRainbow) * 127 + 128;

                dimmedBorderColor = rgb(red1, green1, blue1);
                dimmedBodyColor = rgb(red2, green2, blue2);
            } else {
                dimmedBorderColor = Colors::scale(undimmedBorderColor, colorRGBMultiplier);
                dimmedBodyColor = Colors::scale(undimmedBodyColor, colorRGBMultiplier);
            }

            if(!cv::slider_use_gradient_image.getBool()) {
                BLEND_SHADER->enable();
                BLEND_SHADER->setUniform1i("style", cv::slider_osu_next_style.getBool() ? 1 : 0);
                BLEND_SHADER->setUniform1f("bodyAlphaMultiplier", cv::slider_body_alpha_multiplier.getFloat());
                BLEND_SHADER->setUniform1f("bodyColorSaturation", cv::slider_body_color_saturation.getFloat());
                BLEND_SHADER->setUniform1f("borderSizeMultiplier", cv::slider_border_size_multiplier.getFloat());
                BLEND_SHADER->setUniform1f("borderFeather", cv::slider_border_feather.getFloat());
                BLEND_SHADER->setUniform3f("colBorder", dimmedBorderColor.Rf(), dimmedBorderColor.Gf(),
                                           dimmedBorderColor.Bf());
                BLEND_SHADER->setUniform3f("colBody", dimmedBodyColor.Rf(), dimmedBodyColor.Gf(), dimmedBodyColor.Bf());
            }

            g->setColor(argb(1.0f, colorRGBMultiplier, colorRGBMultiplier,
                             colorRGBMultiplier));  // this only affects the gradient image if used (meaning shaders
                                                    // either don't work or are disabled on purpose)
            osu->getSkin()->getSliderGradient()->bind();
            {
                // draw curve mesh
                {
                    drawFillSliderBodyPeppy(
                        points, (cv::slider_legacy_use_baked_vao.getBool() ? UNIT_CIRCLE_VAO_BAKED : UNIT_CIRCLE_VAO),
                        hitcircleDiameter / 2.0f, drawFromIndex, drawUpToIndex, BLEND_SHADER);

                    if(alwaysPoints.size() > 0)
                        drawFillSliderBodyPeppy(alwaysPoints, UNIT_CIRCLE_VAO_BAKED, hitcircleDiameter / 2.0f, 0,
                                                alwaysPoints.size(), BLEND_SHADER);
                }
            }

            if(!cv::slider_use_gradient_image.getBool()) BLEND_SHADER->disable();
        }
        osu->getSliderFrameBuffer()->disable();
    }
    g->setBlending(true);
    g->setDepthBuffer(false);

    // now draw the slider to the screen (with alpha blending enabled again)
    const int pixelFudge = 2;
    SliderRenderer::fBoundingBoxMinX -= pixelFudge;
    SliderRenderer::fBoundingBoxMaxX += pixelFudge;
    SliderRenderer::fBoundingBoxMinY -= pixelFudge;
    SliderRenderer::fBoundingBoxMaxY += pixelFudge;

    osu->getSliderFrameBuffer()->setColor(argb(alpha * cv::slider_alpha_multiplier.getFloat(), 1.0f, 1.0f, 1.0f));
    osu->getSliderFrameBuffer()->drawRect(SliderRenderer::fBoundingBoxMinX, SliderRenderer::fBoundingBoxMinY,
                                          SliderRenderer::fBoundingBoxMaxX - SliderRenderer::fBoundingBoxMinX,
                                          SliderRenderer::fBoundingBoxMaxY - SliderRenderer::fBoundingBoxMinY);
}

void SliderRenderer::draw(VertexArrayObject *vao, const std::vector<Vector2> &alwaysPoints, Vector2 translation,
                          float scale, float hitcircleDiameter, float from, float to, Color undimmedColor,
                          float colorRGBMultiplier, float alpha, long sliderTimeForRainbow, bool doEnableRenderTarget,
                          bool doDisableRenderTarget, bool doDrawSliderFrameBufferToScreen) {
    if((cv::slider_alpha_multiplier.getFloat() <= 0.0f && doDrawSliderFrameBufferToScreen) ||
       (alpha <= 0.0f && doDrawSliderFrameBufferToScreen) || vao == NULL)
        return;

    checkUpdateVars(hitcircleDiameter);

    if(cv::slider_debug_draw_square_vao.getBool()) {
        const Color dimmedColor = Colors::scale(undimmedColor, colorRGBMultiplier);

        g->setColor(dimmedColor);
        g->setAlpha(alpha * cv::slider_alpha_multiplier.getFloat());
        osu->getSkin()->getHitCircle()->bind();

        vao->setDrawPercent(from, to, 6);  // HACKHACK: hardcoded magic number
        {
            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate(translation.x, translation.y);

                g->drawVAO(vao);
            }
            g->popTransform();
        }

        return;  // nothing more to draw here
    }

    // draw entire slider into framebuffer
    g->setDepthBuffer(true);
    g->setBlending(false);
    {
        if(doEnableRenderTarget) osu->getSliderFrameBuffer()->enable();

        // render
        {
            const Color undimmedBorderColor =
                cv::slider_border_tint_combo_color.getBool() ? undimmedColor : osu->getSkin()->getSliderBorderColor();
            const Color undimmedBodyColor =
                osu->getSkin()->isSliderTrackOverridden() ? osu->getSkin()->getSliderTrackOverride() : undimmedColor;

            Color dimmedBorderColor;
            Color dimmedBodyColor;

            if(cv::slider_rainbow.getBool()) {
                float frequency = 0.3f;
                float time = engine->getTime() * 20;

                const Channel red1 = std::sin(frequency * time + 0 + sliderTimeForRainbow) * 127 + 128;
                const Channel green1 = std::sin(frequency * time + 2 + sliderTimeForRainbow) * 127 + 128;
                const Channel blue1 = std::sin(frequency * time + 4 + sliderTimeForRainbow) * 127 + 128;

                const Channel red2 = std::sin(frequency * time * 1.5f + 0 + sliderTimeForRainbow) * 127 + 128;
                const Channel green2 = std::sin(frequency * time * 1.5f + 2 + sliderTimeForRainbow) * 127 + 128;
                const Channel blue2 = std::sin(frequency * time * 1.5f + 4 + sliderTimeForRainbow) * 127 + 128;

                dimmedBorderColor = rgb(red1, green1, blue1);
                dimmedBodyColor = rgb(red2, green2, blue2);
            } else {
                dimmedBorderColor = Colors::scale(undimmedBorderColor, colorRGBMultiplier);
                dimmedBodyColor = Colors::scale(undimmedBodyColor, colorRGBMultiplier);
            }

            if(!cv::slider_use_gradient_image.getBool()) {
                BLEND_SHADER->enable();
                BLEND_SHADER->setUniform1i("style", cv::slider_osu_next_style.getBool() ? 1 : 0);
                BLEND_SHADER->setUniform1f("bodyAlphaMultiplier", cv::slider_body_alpha_multiplier.getFloat());
                BLEND_SHADER->setUniform1f("bodyColorSaturation", cv::slider_body_color_saturation.getFloat());
                BLEND_SHADER->setUniform1f("borderSizeMultiplier", cv::slider_border_size_multiplier.getFloat());
                BLEND_SHADER->setUniform1f("borderFeather", cv::slider_border_feather.getFloat());
                BLEND_SHADER->setUniform3f("colBorder", dimmedBorderColor.Rf(), dimmedBorderColor.Gf(),
                                           dimmedBorderColor.Bf());
                BLEND_SHADER->setUniform3f("colBody", dimmedBodyColor.Rf(), dimmedBodyColor.Gf(), dimmedBodyColor.Bf());
            }

            g->setColor(argb(1.0f, colorRGBMultiplier, colorRGBMultiplier,
                             colorRGBMultiplier));  // this only affects the gradient image if used (meaning shaders
                                                    // either don't work or are disabled on purpose)
            osu->getSkin()->getSliderGradient()->bind();
            {
                // draw curve mesh
                {
                    vao->setDrawPercent(from, to, UNIT_CIRCLE_VAO_TRIANGLES->getVertices().size());
                    g->pushTransform();
                    {
                        g->scale(scale, scale);
                        g->translate(translation.x, translation.y);
                        /// g->scale(scaleToApplyAfterTranslationX, scaleToApplyAfterTranslationY); // aspire slider
                        /// distortions

                        g->drawVAO(vao);
                    }
                    g->popTransform();

                    if(alwaysPoints.size() > 0)
                        drawFillSliderBodyPeppy(alwaysPoints, UNIT_CIRCLE_VAO_BAKED, hitcircleDiameter / 2.0f, 0,
                                                alwaysPoints.size(), BLEND_SHADER);
                }
            }

            if(!cv::slider_use_gradient_image.getBool()) BLEND_SHADER->disable();
        }

        if(doDisableRenderTarget) osu->getSliderFrameBuffer()->disable();
    }
    g->setBlending(true);
    g->setDepthBuffer(false);

    if(doDrawSliderFrameBufferToScreen) {
        osu->getSliderFrameBuffer()->setColor(argb(alpha * cv::slider_alpha_multiplier.getFloat(), 1.0f, 1.0f, 1.0f));
        osu->getSliderFrameBuffer()->draw(0, 0);
    }
}

void SliderRenderer::drawFillSliderBodyPeppy(const std::vector<Vector2> &points, VertexArrayObject *circleMesh,
                                             float radius, int drawFromIndex, int drawUpToIndex, Shader * /*shader*/) {
    if(drawFromIndex < 0) drawFromIndex = 0;
    if(drawUpToIndex < 0) drawUpToIndex = points.size();

    g->pushTransform();
    {
        // now, translate and draw the master vao for every curve point
        float startX = 0.0f;
        float startY = 0.0f;
        for(int i = drawFromIndex; i < drawUpToIndex; ++i) {
            const float x = points[i].x;
            const float y = points[i].y;

            // fuck oob sliders
            if(x < -radius * 2 || x > osu->getScreenWidth() + radius * 2 || y < -radius * 2 ||
               y > osu->getScreenHeight() + radius * 2)
                continue;

            g->translate(x - startX, y - startY, 0);

            g->drawVAO(circleMesh);

            startX = x;
            startY = y;

            if(x - radius < SliderRenderer::fBoundingBoxMinX) SliderRenderer::fBoundingBoxMinX = x - radius;
            if(x + radius > SliderRenderer::fBoundingBoxMaxX) SliderRenderer::fBoundingBoxMaxX = x + radius;
            if(y - radius < SliderRenderer::fBoundingBoxMinY) SliderRenderer::fBoundingBoxMinY = y - radius;
            if(y + radius > SliderRenderer::fBoundingBoxMaxY) SliderRenderer::fBoundingBoxMaxY = y + radius;
        }
    }
    g->popTransform();
}

void SliderRenderer::drawFillSliderBodyMM(const std::vector<Vector2> &points, float radius, int  /*drawFromIndex*/,
                                          int  /*drawUpToIndex*/) {
    // modified version of
    // https://github.com/ppy/osu-framework/blob/master/osu.Framework/Graphics/Lines/Path_DrawNode.cs

    // TODO: remaining problems
    // 1) how to handle snaking? very very annoying to do via draw call bounds (due to inconsistent mesh topology)
    // 2) check performance for baked vao, maybe as a compatibility option for slower pcs
    // 3) recalculating every frame is not an option, way too slow
    // 4) since we already have smoothsnake begin+end circles, could precalculate a list of draw call bounds indices for
    // snaking and shrinking respectively 5) unbaked performance is way worse than legacy sliders, so it will only be
    // used in baked form

    VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES);

    struct Helper {
        static inline Vector2 pointOnCircle(float angle) { return Vector2(std::sin(angle), -std::cos(angle)); }

        static void addLineCap(Vector2 origin, float theta, float thetaDiff, float radius, VertexArrayObject &vao) {
            const float step = PI / 32.0f;  // MAX_RES

            const float dir = std::signbit(thetaDiff) ? -1.0f : 1.0f;
            thetaDiff = dir * thetaDiff;

            const int amountPoints = (int)std::ceil(thetaDiff / step);

            if(amountPoints > 0) {
                if(dir < 0) theta += PI;

                Vector2 current = origin + pointOnCircle(theta) * radius;

                for(int p = 1; p <= amountPoints; p++) {
                    // center
                    vao.addTexcoord(1, 0);
                    vao.addVertex(origin.x, origin.y, MESH_CENTER_HEIGHT);

                    // first outer point
                    vao.addTexcoord(0, 0);
                    vao.addVertex(current.x, current.y);

                    const float angularOffset = std::min(p * step, thetaDiff);
                    current = origin + pointOnCircle(theta + dir * angularOffset) * radius;

                    // second outer point
                    vao.addTexcoord(0, 0);
                    vao.addVertex(current.x, current.y);
                }
            }
        }
    };

    float prevLineTheta = 0.0f;

    for(int i = 1; i < points.size(); i++) {
        const Vector2 lineStartPoint = points[i - 1];
        const Vector2 lineEndPoint = points[i];

        const Vector2 lineDirection = (lineEndPoint - lineStartPoint);
        const Vector2 lineDirectionNormalized = Vector2(lineDirection.x, lineDirection.y).normalize();
        const Vector2 lineOrthogonalDirection = Vector2(-lineDirectionNormalized.y, lineDirectionNormalized.x);

        const float lineTheta = std::atan2(lineEndPoint.y - lineStartPoint.y, lineEndPoint.x - lineStartPoint.x);

        // start cap
        if(i == 1) Helper::addLineCap(lineStartPoint, lineTheta + PI, PI, radius, vao);

        // body
        {
            const Vector2 ortho = lineOrthogonalDirection;

            const Vector2 screenLineLeftStartPoint = lineStartPoint + ortho * radius;
            const Vector2 screenLineLeftEndPoint = lineEndPoint + ortho * radius;

            const Vector2 screenLineRightStartPoint = lineStartPoint - ortho * radius;
            const Vector2 screenLineRightEndPoint = lineEndPoint - ortho * radius;

            const Vector2 screenLineStartPoint = lineStartPoint;
            const Vector2 screenLineEndPoint = lineEndPoint;

            // type is triangles, build a rectangle out of 6 vertices

            //
            // 1   3   5
            // *---*---*
            // |  /|  /|
            // | / | / |     // the line 3-4 is the center of the slider (with a raised z-coordinate for blending)
            // |/  |/  |
            // *---*---*
            // 2   4   6
            //

            vao.addTexcoord(0, 0);
            vao.addVertex(screenLineLeftEndPoint.x, screenLineLeftEndPoint.y);  // 1
            vao.addTexcoord(0, 0);
            vao.addVertex(screenLineLeftStartPoint.x, screenLineLeftStartPoint.y);  // 2
            vao.addTexcoord(1, 0);
            vao.addVertex(screenLineEndPoint.x, screenLineEndPoint.y, MESH_CENTER_HEIGHT);  // 3

            vao.addTexcoord(1, 0);
            vao.addVertex(screenLineEndPoint.x, screenLineEndPoint.y, MESH_CENTER_HEIGHT);  // 3
            vao.addTexcoord(0, 0);
            vao.addVertex(screenLineLeftStartPoint.x, screenLineLeftStartPoint.y);  // 2
            vao.addTexcoord(1, 0);
            vao.addVertex(screenLineStartPoint.x, screenLineStartPoint.y, MESH_CENTER_HEIGHT);  // 4

            vao.addTexcoord(1, 0);
            vao.addVertex(screenLineEndPoint.x, screenLineEndPoint.y, MESH_CENTER_HEIGHT);  // 3
            vao.addTexcoord(1, 0);
            vao.addVertex(screenLineStartPoint.x, screenLineStartPoint.y, MESH_CENTER_HEIGHT);  // 4
            vao.addTexcoord(0, 0);
            vao.addVertex(screenLineRightEndPoint.x, screenLineRightEndPoint.y);  // 5

            vao.addTexcoord(0, 0);
            vao.addVertex(screenLineRightEndPoint.x, screenLineRightEndPoint.y);  // 5
            vao.addTexcoord(1, 0);
            vao.addVertex(screenLineStartPoint.x, screenLineStartPoint.y, MESH_CENTER_HEIGHT);  // 4
            vao.addTexcoord(0, 0);
            vao.addVertex(screenLineRightStartPoint.x, screenLineRightStartPoint.y);  // 6
        }

        // TODO: fix non-rolled-over theta causing full circle to be built at perfect angle points at e.g. reach out at
        // 0:48 these would also break snaking, so we have to handle that special case and not generate any meshes there

        // wedges
        if(i > 1) Helper::addLineCap(lineStartPoint, prevLineTheta, lineTheta - prevLineTheta, radius, vao);

        // end cap
        if(i == (points.size() - 1)) Helper::addLineCap(lineEndPoint, lineTheta, PI, radius, vao);

        prevLineTheta = lineTheta;
    }

    // draw it
    if(vao.getNumVertices() > 0) {
        if(cv::slider_debug_wireframe.getBool()) g->setWireframe(true);

        // draw body
        g->drawVAO(&vao);

        if(cv::slider_debug_wireframe.getBool()) g->setWireframe(false);
    }
}

void SliderRenderer::checkUpdateVars(float hitcircleDiameter) {
    // static globals

    // build shaders and circle mesh
    if(BLEND_SHADER == NULL)  // only do this once
    {
        // build shaders
        BLEND_SHADER = resourceManager->loadShader("slider.vsh", "slider.fsh", "slider");
    }

    const int subdivisions = cv::slider_body_unit_circle_subdivisions.getInt();
    if(subdivisions != UNIT_CIRCLE_SUBDIVISIONS) {
        UNIT_CIRCLE_SUBDIVISIONS = subdivisions;

        // build unit cone
        {
            UNIT_CIRCLE.clear();

            // tip of the cone
            // texture coordinates
            UNIT_CIRCLE.push_back(1.0f);
            UNIT_CIRCLE.push_back(0.0f);

            // position
            UNIT_CIRCLE.push_back(0.0f);
            UNIT_CIRCLE.push_back(0.0f);
            UNIT_CIRCLE.push_back(MESH_CENTER_HEIGHT);

            for(int j = 0; j < subdivisions; ++j) {
                float phase = j * (float)PI * 2.0f / subdivisions;

                // texture coordinates
                UNIT_CIRCLE.push_back(0.0f);
                UNIT_CIRCLE.push_back(0.0f);

                // positon
                UNIT_CIRCLE.push_back((float)std::sin(phase));
                UNIT_CIRCLE.push_back((float)std::cos(phase));
                UNIT_CIRCLE.push_back(0.0f);
            }

            // texture coordinates
            UNIT_CIRCLE.push_back(0.0f);
            UNIT_CIRCLE.push_back(0.0f);

            // positon
            UNIT_CIRCLE.push_back((float)std::sin(0.0f));
            UNIT_CIRCLE.push_back((float)std::cos(0.0f));
            UNIT_CIRCLE.push_back(0.0f);
        }
    }

    // build vaos
    if(UNIT_CIRCLE_VAO == NULL) UNIT_CIRCLE_VAO = new VertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_FAN);
    if(UNIT_CIRCLE_VAO_BAKED == NULL)
        UNIT_CIRCLE_VAO_BAKED = resourceManager->createVertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_FAN);
    if(UNIT_CIRCLE_VAO_TRIANGLES == NULL)
        UNIT_CIRCLE_VAO_TRIANGLES = new VertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES);

    // (re-)generate master circle mesh (centered) if the size changed
    // dynamic mods like minimize or wobble have to use the legacy renderer anyway, since the slider shape may change
    // every frame
    if(hitcircleDiameter != UNIT_CIRCLE_VAO_DIAMETER) {
        const float radius = hitcircleDiameter / 2.0f;

        UNIT_CIRCLE_VAO_BAKED->release();

        // triangle fan
        UNIT_CIRCLE_VAO_DIAMETER = hitcircleDiameter;
        UNIT_CIRCLE_VAO->clear();
        for(int i = 0; i < UNIT_CIRCLE.size() / 5; i++) {
            Vector3 vertexPos =
                Vector3((radius * UNIT_CIRCLE[i * 5 + 2]), (radius * UNIT_CIRCLE[i * 5 + 3]), UNIT_CIRCLE[i * 5 + 4]);
            Vector2 vertexTexcoord = Vector2(UNIT_CIRCLE[i * 5 + 0], UNIT_CIRCLE[i * 5 + 1]);

            UNIT_CIRCLE_VAO->addVertex(vertexPos);
            UNIT_CIRCLE_VAO->addTexcoord(vertexTexcoord);

            UNIT_CIRCLE_VAO_BAKED->addVertex(vertexPos);
            UNIT_CIRCLE_VAO_BAKED->addTexcoord(vertexTexcoord);
        }

        resourceManager->loadResource(UNIT_CIRCLE_VAO_BAKED);

        // pure triangles (needed for VertexArrayObject, because we can't merge multiple triangle fan meshes into one
        // VertexArrayObject)
        UNIT_CIRCLE_VAO_TRIANGLES->clear();
        Vector3 startVertex =
            Vector3((radius * UNIT_CIRCLE[0 * 5 + 2]), (radius * UNIT_CIRCLE[0 * 5 + 3]), UNIT_CIRCLE[0 * 5 + 4]);
        Vector2 startUV = Vector2(UNIT_CIRCLE[0 * 5 + 0], UNIT_CIRCLE[0 * 5 + 1]);
        for(int i = 1; i < UNIT_CIRCLE.size() / 5 - 1; i++) {
            // center
            UNIT_CIRCLE_VAO_TRIANGLES->addVertex(startVertex);
            UNIT_CIRCLE_VAO_TRIANGLES->addTexcoord(startUV);

            // pizza slice edge 1
            UNIT_CIRCLE_VAO_TRIANGLES->addVertex(
                Vector3((radius * UNIT_CIRCLE[i * 5 + 2]), (radius * UNIT_CIRCLE[i * 5 + 3]), UNIT_CIRCLE[i * 5 + 4]));
            UNIT_CIRCLE_VAO_TRIANGLES->addTexcoord(Vector2(UNIT_CIRCLE[i * 5 + 0], UNIT_CIRCLE[i * 5 + 1]));

            // pizza slice edge 2
            UNIT_CIRCLE_VAO_TRIANGLES->addVertex(Vector3((radius * UNIT_CIRCLE[(i + 1) * 5 + 2]),
                                                         (radius * UNIT_CIRCLE[(i + 1) * 5 + 3]),
                                                         UNIT_CIRCLE[(i + 1) * 5 + 4]));
            UNIT_CIRCLE_VAO_TRIANGLES->addTexcoord(Vector2(UNIT_CIRCLE[(i + 1) * 5 + 0], UNIT_CIRCLE[(i + 1) * 5 + 1]));
        }
    }
}

void SliderRenderer::resetRenderTargetBoundingBox() {
    SliderRenderer::fBoundingBoxMinX = (std::numeric_limits<float>::max)();
    SliderRenderer::fBoundingBoxMaxX = 0.0f;
    SliderRenderer::fBoundingBoxMinY = (std::numeric_limits<float>::max)();
    SliderRenderer::fBoundingBoxMaxY = 0.0f;
}
