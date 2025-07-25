#include "ModFPoSu.h"

#include <sstream>

#include "AnimationHandler.h"
#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "Beatmap.h"
#include "Camera.h"
#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "OpenGLHeaders.h"
#include "OpenGLLegacyInterface.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"

constexpr const float ModFPoSu::SIZEDIV3D;
constexpr const int ModFPoSu::SUBDIVISIONS;

ModFPoSu::ModFPoSu() {
    // vars
    this->fCircumLength = 0.0f;
    this->camera = new Camera(Vector3(0, 0, 0), Vector3(0, 0, -1));
    this->bKeyLeftDown = false;
    this->bKeyUpDown = false;
    this->bKeyRightDown = false;
    this->bKeyDownDown = false;
    this->bKeySpaceDown = false;
    this->bKeySpaceUpDown = false;
    this->bZoomKeyDown = false;
    this->bZoomed = false;
    this->fZoomFOVAnimPercent = 0.0f;

    this->fEdgeDistance = 0.0f;
    this->bCrosshairIntersectsScreen = false;

    // load resources
    this->vao = resourceManager->createVertexArrayObject();
    this->vaoCube = resourceManager->createVertexArrayObject();

    this->hitcircleShader = NULL;

    // convar callbacks
    cv::fposu_curved.setCallback(SA::MakeDelegate<&ModFPoSu::onCurvedChange>(this));
    cv::fposu_distance.setCallback(SA::MakeDelegate<&ModFPoSu::onDistanceChange>(this));
    cv::fposu_noclip.setCallback(SA::MakeDelegate<&ModFPoSu::onNoclipChange>(this));

    // init
    this->makePlayfield();
    this->makeBackgroundCube();
}

ModFPoSu::~ModFPoSu() { anim->deleteExistingAnimation(&this->fZoomFOVAnimPercent); }

void ModFPoSu::draw() {
    if(!cv::mod_fposu.getBool()) return;

    const float fov = std::lerp(cv::fposu_fov.getFloat(), cv::fposu_zoom_fov.getFloat(), this->fZoomFOVAnimPercent);
    Matrix4 projectionMatrix =
        cv::fposu_vertical_fov.getBool()
            ? Camera::buildMatrixPerspectiveFovVertical(
                  glm::radians(fov), ((float)osu->getScreenWidth() / (float)osu->getScreenHeight()), 0.05f, 1000.0f)
            : Camera::buildMatrixPerspectiveFovHorizontal(
                  glm::radians(fov), ((float)osu->getScreenHeight() / (float)osu->getScreenWidth()), 0.05f, 1000.0f);
    Matrix4 viewMatrix = Camera::buildMatrixLookAt(
        this->camera->getPos(), this->camera->getPos() + this->camera->getViewDirection(), this->camera->getViewUp());

    // HACKHACK: there is currently no way to directly modify the viewport origin, so the only option for rendering
    // non-2d stuff with correct offsets (i.e. top left) is by rendering into a rendertarget HACKHACK: abusing
    // sliderFrameBuffer

    osu->getSliderFrameBuffer()->enable();
    {
        const Vector2 resolutionBackup = g->getResolution();
        g->onResolutionChange(
            osu->getSliderFrameBuffer()
                ->getSize());  // set renderer resolution to game resolution (to correctly support letterboxing etc.)
        {
            g->clearDepthBuffer();
            g->pushTransform();
            {
                g->setWorldMatrix(viewMatrix);
                g->setProjectionMatrix(projectionMatrix);

                g->setBlending(false);
                {
                    // regular fposu "2d" render path

                    g->setDepthBuffer(true);
                    {
                        // axis lines at (0, 0, 0)
                        if(cv::fposu_noclip.getBool()) {
                            static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_LINES);
                            vao.empty();
                            {
                                Vector3 pos = Vector3(0, 0, 0);
                                float length = 1.0f;

                                vao.addColor(0xffff0000);
                                vao.addVertex(pos.x, pos.y, pos.z);
                                vao.addColor(0xffff0000);
                                vao.addVertex(pos.x + length, pos.y, pos.z);

                                vao.addColor(0xff00ff00);
                                vao.addVertex(pos.x, pos.y, pos.z);
                                vao.addColor(0xff00ff00);
                                vao.addVertex(pos.x, pos.y + length, pos.z);

                                vao.addColor(0xff0000ff);
                                vao.addVertex(pos.x, pos.y, pos.z);
                                vao.addColor(0xff0000ff);
                                vao.addVertex(pos.x, pos.y, pos.z + length);
                            }
                            g->setColor(0xffffffff);
                            g->drawVAO(&vao);
                        }

                        // cube
                        if(cv::fposu_cube.getBool()) {
                            osu->getSkin()->getBackgroundCube()->bind();
                            {
                                g->setColor(argb(255, std::clamp<int>(cv::fposu_cube_tint_r.getInt(), 0, 255),
                                                  std::clamp<int>(cv::fposu_cube_tint_g.getInt(), 0, 255),
                                                  std::clamp<int>(cv::fposu_cube_tint_b.getInt(), 0, 255)));
                                g->drawVAO(this->vaoCube);
                            }
                            osu->getSkin()->getBackgroundCube()->unbind();
                        }
                    }
                    g->setDepthBuffer(false);

                    if(cv::fposu_transparent_playfield.getBool()) g->setBlending(true);

                    Matrix4 worldMatrix = this->modelMatrix;
                    g->setWorldMatrixMul(worldMatrix);
                    {
                        osu->getPlayfieldBuffer()->bind();
                        {
                            g->setColor(0xffffffff);
                            g->drawVAO(this->vao);
                        }
                        osu->getPlayfieldBuffer()->unbind();
                    }

                    // (no setBlending(false), since we are already at the end)
                }
                if(!cv::fposu_transparent_playfield.getBool()) g->setBlending(true);
            }
            g->popTransform();
        }
        g->onResolutionChange(resolutionBackup);
    }
    osu->getSliderFrameBuffer()->disable();

    // finally, draw that to the screen
    g->setBlending(false);
    { osu->getSliderFrameBuffer()->draw(0, 0); }
    g->setBlending(true);
}

void ModFPoSu::update() {
    if(!cv::mod_fposu.getBool()) return;

    if(cv::fposu_noclip.getBool()) this->noclipMove();

    this->modelMatrix = Matrix4();
    {
        this->modelMatrix.scale(
            1.0f,
            (osu->getPlayfieldBuffer()->getHeight() / osu->getPlayfieldBuffer()->getWidth()) * (this->fCircumLength),
            1.0f);

        // rotate around center
        {
            this->modelMatrix.translate(0, 0, cv::fposu_distance.getFloat());  // (compensate for mesh offset)
            {
                this->modelMatrix.rotateX(cv::fposu_playfield_rotation_x.getFloat());
                this->modelMatrix.rotateY(cv::fposu_playfield_rotation_y.getFloat());
                this->modelMatrix.rotateZ(cv::fposu_playfield_rotation_z.getFloat());
            }
            this->modelMatrix.translate(0, 0, -cv::fposu_distance.getFloat());  // (restore)
        }

        // NOTE: slightly move back by default to avoid aliasing with background cube
        this->modelMatrix.translate(cv::fposu_playfield_position_x.getFloat(), cv::fposu_playfield_position_y.getFloat(),
                                    -0.0015f + cv::fposu_playfield_position_z.getFloat());

        if(cv::fposu_mod_strafing.getBool()) {
            if(osu->isInPlayMode()) {
                const long curMusicPos = osu->getSelectedBeatmap()->getCurMusicPos();

                const float speedMultiplierCompensation = 1.0f / osu->getSelectedBeatmap()->getSpeedMultiplier();

                const float x = std::sin((curMusicPos / 1000.0f) * 5 * speedMultiplierCompensation *
                                         cv::fposu_mod_strafing_frequency_x.getFloat()) *
                                cv::fposu_mod_strafing_strength_x.getFloat();
                const float y = std::sin((curMusicPos / 1000.0f) * 5 * speedMultiplierCompensation *
                                         cv::fposu_mod_strafing_frequency_y.getFloat()) *
                                cv::fposu_mod_strafing_strength_y.getFloat();
                const float z = std::sin((curMusicPos / 1000.0f) * 5 * speedMultiplierCompensation *
                                         cv::fposu_mod_strafing_frequency_z.getFloat()) *
                                cv::fposu_mod_strafing_strength_z.getFloat();

                this->modelMatrix.translate(x, y, z);
            }
        }
    }

    const bool isAutoCursor =
        (osu->getModAuto() || osu->getModAutopilot() || osu->getSelectedBeatmap()->is_watching || bancho->spectating);

    this->bCrosshairIntersectsScreen = true;
    if(!cv::fposu_absolute_mode.getBool() && !isAutoCursor)
    {
        // regular mouse position mode

        // calculate mouse delta
        Vector2 rawDelta = mouse->getRawDelta() /
                           cv::mouse_sensitivity.getFloat();  // HACKHACK: undo engine mouse sensitivity multiplier

        // apply fposu mouse sensitivity multiplier
        const double countsPerCm = (double)cv::fposu_mouse_dpi.getInt() / 2.54;
        const double cmPer360 = cv::fposu_mouse_cm_360.getFloat();
        const double countsPer360 = cmPer360 * countsPerCm;
        const double multiplier = 360.0 / countsPer360;
        rawDelta *= multiplier;

        // apply zoom_sensitivity_ratio if zoomed
        if(this->bZoomed && cv::fposu_zoom_sensitivity_ratio.getFloat() > 0.0f)
            // see https://www.reddit.com/r/GlobalOffensive/comments/3vxkav/how_zoomed_sensitivity_works/
            rawDelta *=
                (cv::fposu_zoom_fov.getFloat() / cv::fposu_fov.getFloat()) * cv::fposu_zoom_sensitivity_ratio.getFloat();

        // update camera
        if(rawDelta.x != 0.0f)
            this->camera->rotateY(rawDelta.x * (cv::fposu_invert_horizontal.getBool() ? 1.0f : -1.0f));
        if(rawDelta.y != 0.0f) this->camera->rotateX(rawDelta.y * (cv::fposu_invert_vertical.getBool() ? 1.0f : -1.0f));

        // calculate ray-mesh intersection and set new mouse pos
        Vector2 newMousePos = this->intersectRayMesh(this->camera->getPos(), this->camera->getViewDirection());

        const bool osCursorVisible = (env->isCursorVisible() || !env->isCursorInWindow() || !engine->hasFocus());

        if(!osCursorVisible) {
            // special case: force to center of screen if no intersection
            if(newMousePos.x == 0.0f && newMousePos.y == 0.0f) {
                this->bCrosshairIntersectsScreen = false;
                newMousePos = osu->getScreenSize() / 2;
            }

            this->setMousePosCompensated(newMousePos);
        }
    } else {
        // absolute mouse position mode (or auto)
        Vector2 mousePos = mouse->getPos();
        auto beatmap = osu->getSelectedBeatmap();
        if(isAutoCursor && !beatmap->isPaused()) {
            mousePos = beatmap->getCursorPos();
        }

        this->bCrosshairIntersectsScreen = true;
        this->camera->lookAt(this->calculateUnProjectedVector(mousePos));
    }
}

void ModFPoSu::noclipMove() {
    const float noclipSpeed = cv::fposu_noclipspeed.getFloat() * (keyboard->isShiftDown() ? 3.0f : 1.0f) *
                              (keyboard->isControlDown() ? 0.2f : 1);
    const float noclipAccelerate = cv::fposu_noclipaccelerate.getFloat();
    const float friction = cv::fposu_noclipfriction.getFloat();

    // build direction vector based on player key inputs
    Vector3 wishdir;
    {
        wishdir += (this->bKeyUpDown ? this->camera->getViewDirection() : Vector3());
        wishdir -= (this->bKeyDownDown ? this->camera->getViewDirection() : Vector3());
        wishdir += (this->bKeyLeftDown ? this->camera->getViewRight() : Vector3());
        wishdir -= (this->bKeyRightDown ? this->camera->getViewRight() : Vector3());
        wishdir +=
            (this->bKeySpaceDown ? (this->bKeySpaceUpDown ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, -1.0f, 0.0f))
                                 : Vector3());
    }

    // normalize
    float wishspeed = 0.0f;
    {
        const float length = wishdir.length();
        if(length > 0.0f) {
            wishdir /= length;  // normalize
            wishspeed = noclipSpeed;
        }
    }

    // friction (deccelerate)
    {
        const float spd = this->vVelocity.length();
        if(spd > 0.00000001f) {
            // only apply friction once we "stop" moving (special case for noclip mode)
            if(wishspeed == 0.0f) {
                const float drop = spd * friction * engine->getFrameTime();

                float newSpeed = spd - drop;
                {
                    if(newSpeed < 0.0f) newSpeed = 0.0f;
                }
                newSpeed /= spd;

                this->vVelocity *= newSpeed;
            }
        } else
            this->vVelocity.zero();
    }

    // accelerate
    {
        float addspeed = wishspeed;
        if(addspeed > 0.0f) {
            float accelspeed = noclipAccelerate * engine->getFrameTime() * wishspeed;

            if(accelspeed > addspeed) accelspeed = addspeed;

            this->vVelocity += accelspeed * wishdir;
        }
    }

    // clamp to max speed
    if(this->vVelocity.length() > noclipSpeed) this->vVelocity.setLength(noclipSpeed);

    // move
    this->camera->setPos(this->camera->getPos() + this->vVelocity * engine->getFrameTime());
}

void ModFPoSu::onKeyDown(KeyboardEvent &key) {
    if(key == (KEYCODE)cv::FPOSU_ZOOM.getInt() && !this->bZoomKeyDown) {
        this->bZoomKeyDown = true;

        if(!this->bZoomed || cv::fposu_zoom_toggle.getBool()) {
            if(!cv::fposu_zoom_toggle.getBool())
                this->bZoomed = true;
            else
                this->bZoomed = !this->bZoomed;

            this->handleZoomedChange();
        }
    }

    if(key == KEY_A) this->bKeyLeftDown = true;
    if(key == KEY_W) this->bKeyUpDown = true;
    if(key == KEY_D) this->bKeyRightDown = true;
    if(key == KEY_S) this->bKeyDownDown = true;
    if(key == KEY_SPACE) {
        if(!this->bKeySpaceDown) this->bKeySpaceUpDown = !this->bKeySpaceUpDown;

        this->bKeySpaceDown = true;
    }
}

void ModFPoSu::onKeyUp(KeyboardEvent &key) {
    if(key == (KEYCODE)cv::FPOSU_ZOOM.getInt()) {
        this->bZoomKeyDown = false;

        if(this->bZoomed && !cv::fposu_zoom_toggle.getBool()) {
            this->bZoomed = false;
            this->handleZoomedChange();
        }
    }

    if(key == KEY_A) this->bKeyLeftDown = false;
    if(key == KEY_W) this->bKeyUpDown = false;
    if(key == KEY_D) this->bKeyRightDown = false;
    if(key == KEY_S) this->bKeyDownDown = false;
    if(key == KEY_SPACE) this->bKeySpaceDown = false;
}

void ModFPoSu::handleZoomedChange() {
    if(this->bZoomed)
        anim->moveQuadOut(&this->fZoomFOVAnimPercent, 1.0f,
                          (1.0f - this->fZoomFOVAnimPercent) * cv::fposu_zoom_anim_duration.getFloat(), true);
    else
        anim->moveQuadOut(&this->fZoomFOVAnimPercent, 0.0f,
                          this->fZoomFOVAnimPercent * cv::fposu_zoom_anim_duration.getFloat(), true);
}

void ModFPoSu::setMousePosCompensated(Vector2 newMousePos) {
    // NOTE: letterboxing uses Mouse::setOffset() to offset the virtual engine cursor coordinate system, so we have to
    // respect that when setting a new (absolute) position
    newMousePos -= mouse->getOffset();

    mouse->onPosChange(newMousePos);
    env->setMousePos(newMousePos.x, newMousePos.y);
}

Vector2 ModFPoSu::intersectRayMesh(Vector3 pos, Vector3 dir) {
    std::list<VertexPair>::iterator begin = this->meshList.begin();
    std::list<VertexPair>::iterator next = ++this->meshList.begin();
    int face = 0;
    while(next != this->meshList.end()) {
        const Vector4 topLeft = (this->modelMatrix * Vector4((*begin).a.x, (*begin).a.y, (*begin).a.z, 1.0f));
        const Vector4 right = (this->modelMatrix * Vector4((*next).a.x, (*next).a.y, (*next).a.z, 1.0f));
        const Vector4 down = (this->modelMatrix * Vector4((*begin).b.x, (*begin).b.y, (*begin).b.z, 1.0f));
        // const Vector3 normal = (modelMatrix * (*begin).normal).normalize();

        const Vector3 TopLeft = Vector3(topLeft.x, topLeft.y, topLeft.z);
        const Vector3 Right = Vector3(right.x, right.y, right.z);
        const Vector3 Down = Vector3(down.x, down.y, down.z);

        const Vector3 calculatedNormal = (Right - TopLeft).cross(Down - TopLeft);

        const float denominator = calculatedNormal.dot(dir);
        const float numerator = -calculatedNormal.dot(pos - TopLeft);

        // WARNING: this is a full line trace (i.e. backwards and forwards infinitely far)
        if(denominator == 0.0f) {
            begin++;
            next++;
            face++;
            continue;
        }

        const float t = numerator / denominator;
        const Vector3 intersectionPoint = pos + dir * t;

        if(std::abs(calculatedNormal.dot(intersectionPoint - TopLeft)) < 1e-6f) {
            const float u = (intersectionPoint - TopLeft).dot(Right - TopLeft);
            const float v = (intersectionPoint - TopLeft).dot(Down - TopLeft);

            if(u >= 0 && u <= (Right - TopLeft).dot(Right - TopLeft)) {
                if(v >= 0 && v <= (Down - TopLeft).dot(Down - TopLeft)) {
                    if(denominator > 0.0f)  // only allow forwards trace
                    {
                        const float rightLength = (Right - TopLeft).length();
                        const float downLength = (Down - TopLeft).length();
                        const float x = u / (rightLength * rightLength);
                        const float y = v / (downLength * downLength);
                        const float distancePerFace = (float)osu->getScreenWidth() / pow(2.0f, (float)SUBDIVISIONS);
                        const float distanceInFace = distancePerFace * x;

                        const Vector2 newMousePos =
                            Vector2((distancePerFace * face) + distanceInFace, y * osu->getScreenHeight());

                        return newMousePos;
                    }
                }
            }
        }

        begin++;
        next++;
        face++;
    }

    return Vector2(0, 0);
}

Vector3 ModFPoSu::calculateUnProjectedVector(Vector2 pos) {
    // calculate 3d position of 2d cursor on screen mesh
    const float cursorXPercent = std::clamp<float>(pos.x / (float)osu->getScreenWidth(), 0.0f, 1.0f);
    const float cursorYPercent = std::clamp<float>(pos.y / (float)osu->getScreenHeight(), 0.0f, 1.0f);

    std::list<VertexPair>::iterator begin = this->meshList.begin();
    std::list<VertexPair>::iterator next = ++this->meshList.begin();
    while(next != this->meshList.end()) {
        Vector3 topLeft = (*begin).a;
        Vector3 bottomLeft = (*begin).b;
        Vector3 topRight = (*next).a;
        // Vector3 bottomRight = (*next).b;

        const float leftTC = (*begin).textureCoordinate;
        const float rightTC = (*next).textureCoordinate;
        const float topTC = 1.0f;
        const float bottomTC = 0.0f;

        if(cursorXPercent >= leftTC && cursorXPercent <= rightTC && cursorYPercent >= bottomTC &&
           cursorYPercent <= topTC) {
            const float tcRightPercent = (cursorXPercent - leftTC) / std::abs(leftTC - rightTC);
            Vector3 right = (topRight - topLeft);
            right.setLength(right.length() * tcRightPercent);

            const float tcDownPercent = (cursorYPercent - bottomTC) / std::abs(topTC - bottomTC);
            Vector3 down = (bottomLeft - topLeft);
            down.setLength(down.length() * tcDownPercent);

            const Vector3 modelPos = (topLeft + right + down);

            const Vector4 worldPos = this->modelMatrix * Vector4(modelPos.x, modelPos.y, modelPos.z, 1.0f);

            return Vector3(worldPos.x, worldPos.y, worldPos.z);
        }

        begin++;
        next++;
    }

    return Vector3(-0.5f, 0.5f, -0.5f);
}

void ModFPoSu::makePlayfield() {
    this->vao->clear();
    this->meshList.clear();

    const float topTC = 1.0f;
    const float bottomTC = 0.0f;
    const float dist = -cv::fposu_distance.getFloat();

    VertexPair vp1 = VertexPair(Vector3(-0.5, 0.5, dist), Vector3(-0.5, -0.5, dist), 0);
    VertexPair vp2 = VertexPair(Vector3(0.5, 0.5, dist), Vector3(0.5, -0.5, dist), 1);

    this->fEdgeDistance = Vector3(0, 0, 0).distance(Vector3(-0.5, 0.0, dist));

    this->meshList.push_back(vp1);
    this->meshList.push_back(vp2);

    std::list<VertexPair>::iterator begin = this->meshList.begin();
    std::list<VertexPair>::iterator end = this->meshList.end();
    --end;
    this->fCircumLength = subdivide(this->meshList, begin, end, SUBDIVISIONS, this->fEdgeDistance);

    begin = this->meshList.begin();
    std::list<VertexPair>::iterator next = ++this->meshList.begin();
    while(next != this->meshList.end()) {
        Vector3 topLeft = (*begin).a;
        Vector3 bottomLeft = (*begin).b;
        Vector3 topRight = (*next).a;
        Vector3 bottomRight = (*next).b;

        const float leftTC = (*begin).textureCoordinate;
        const float rightTC = (*next).textureCoordinate;

        this->vao->addVertex(topLeft);
        this->vao->addTexcoord(leftTC, topTC);
        this->vao->addVertex(topRight);
        this->vao->addTexcoord(rightTC, topTC);
        this->vao->addVertex(bottomLeft);
        this->vao->addTexcoord(leftTC, bottomTC);

        this->vao->addVertex(bottomLeft);
        this->vao->addTexcoord(leftTC, bottomTC);
        this->vao->addVertex(topRight);
        this->vao->addTexcoord(rightTC, topTC);
        this->vao->addVertex(bottomRight);
        this->vao->addTexcoord(rightTC, bottomTC);

        (*begin).normal = normalFromTriangle(topLeft, topRight, bottomLeft);

        begin++;
        next++;
    }
}

void ModFPoSu::makeBackgroundCube() {
    this->vaoCube->clear();

    const float size = cv::fposu_cube_size.getFloat();

    // front
    this->vaoCube->addVertex(-size, -size, -size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);
    this->vaoCube->addVertex(size, -size, -size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);
    this->vaoCube->addVertex(size, size, -size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);

    this->vaoCube->addVertex(size, size, -size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);
    this->vaoCube->addVertex(-size, size, -size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);
    this->vaoCube->addVertex(-size, -size, -size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);

    // back
    this->vaoCube->addVertex(-size, -size, size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);
    this->vaoCube->addVertex(size, -size, size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);
    this->vaoCube->addVertex(size, size, size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);

    this->vaoCube->addVertex(size, size, size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);
    this->vaoCube->addVertex(-size, size, size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);
    this->vaoCube->addVertex(-size, -size, size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);

    // left
    this->vaoCube->addVertex(-size, size, size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);
    this->vaoCube->addVertex(-size, size, -size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);
    this->vaoCube->addVertex(-size, -size, -size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);

    this->vaoCube->addVertex(-size, -size, -size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);
    this->vaoCube->addVertex(-size, -size, size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);
    this->vaoCube->addVertex(-size, size, size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);

    // right
    this->vaoCube->addVertex(size, size, size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);
    this->vaoCube->addVertex(size, size, -size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);
    this->vaoCube->addVertex(size, -size, -size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);

    this->vaoCube->addVertex(size, -size, -size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);
    this->vaoCube->addVertex(size, -size, size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);
    this->vaoCube->addVertex(size, size, size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);

    // bottom
    this->vaoCube->addVertex(-size, -size, -size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);
    this->vaoCube->addVertex(size, -size, -size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);
    this->vaoCube->addVertex(size, -size, size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);

    this->vaoCube->addVertex(size, -size, size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);
    this->vaoCube->addVertex(-size, -size, size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);
    this->vaoCube->addVertex(-size, -size, -size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);

    // top
    this->vaoCube->addVertex(-size, size, -size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);
    this->vaoCube->addVertex(size, size, -size);
    this->vaoCube->addTexcoord(1.0f, 1.0f);
    this->vaoCube->addVertex(size, size, size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);

    this->vaoCube->addVertex(size, size, size);
    this->vaoCube->addTexcoord(1.0f, 0.0f);
    this->vaoCube->addVertex(-size, size, size);
    this->vaoCube->addTexcoord(0.0f, 0.0f);
    this->vaoCube->addVertex(-size, size, -size);
    this->vaoCube->addTexcoord(0.0f, 1.0f);
}

void ModFPoSu::onCurvedChange() { this->makePlayfield(); }

void ModFPoSu::onDistanceChange() { this->makePlayfield(); }

void ModFPoSu::onNoclipChange() {
    if(cv::fposu_noclip.getBool())
        this->camera->setPos(this->vPrevNoclipCameraPos);
    else {
        this->vPrevNoclipCameraPos = this->camera->getPos();
        this->camera->setPos(Vector3(0, 0, 0));
    }
}

float ModFPoSu::subdivide(std::list<VertexPair> &meshList, const std::list<VertexPair>::iterator &begin,
                          const std::list<VertexPair>::iterator &end, int n, float edgeDistance) {
    const Vector3 a = Vector3((*begin).a.x, 0.0f, (*begin).a.z);
    const Vector3 b = Vector3((*end).a.x, 0.0f, (*end).a.z);
    Vector3 middlePoint =
        Vector3(std::lerp(a.x, b.x, 0.5f), std::lerp(a.y, b.y, 0.5f), std::lerp(a.z, b.z, 0.5f));

    if(cv::fposu_curved.getBool()) middlePoint.setLength(edgeDistance);

    Vector3 top, bottom;
    top = bottom = middlePoint;

    top.y = (*begin).a.y;
    bottom.y = (*begin).b.y;

    const float tc = std::lerp((*begin).textureCoordinate, (*end).textureCoordinate, 0.5f);

    VertexPair newVP = VertexPair(top, bottom, tc);
    const std::list<VertexPair>::iterator newPos = meshList.insert(end, newVP);

    float circumLength = 0.0f;

    if(n > 1) {
        circumLength += subdivide(meshList, begin, newPos, n - 1, edgeDistance);
        circumLength += subdivide(meshList, newPos, end, n - 1, edgeDistance);
    } else {
        circumLength += (*begin).a.distance(newVP.a);
        circumLength += newVP.a.distance((*end).a);
    }

    return circumLength;
}

Vector3 ModFPoSu::normalFromTriangle(Vector3 p1, Vector3 p2, Vector3 p3) {
    const Vector3 u = (p2 - p1);
    const Vector3 v = (p3 - p1);

    return u.cross(v).normalize();
}
