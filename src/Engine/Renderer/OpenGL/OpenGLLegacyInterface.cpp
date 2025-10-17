// Copyright (c) 2016, PG, All rights reserved.
#include "OpenGLLegacyInterface.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include "Camera.h"
#include "ConVar.h"
#include "Engine.h"

#include "Font.h"
#include "OpenGLImage.h"
#include "OpenGLRenderTarget.h"
#include "OpenGLShader.h"
#include "OpenGLVertexArrayObject.h"
#include "Logging.h"

#include "SDLGLInterface.h"
#include "OpenGLStateCache.h"

#include "shaders.h"

#include <cstddef>

OpenGLLegacyInterface::OpenGLLegacyInterface()
    : Graphics(),
      vResolution(engine->getScreenSize())  // initial viewport size = window size
{
    // quality
    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST);
    glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);

    // enable
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnable(GL_COLOR_MATERIAL);

    // disable
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    // shading
    glShadeModel(GL_SMOOTH);

    // blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    // culling
    glFrontFace(GL_CCW);

    // debugging
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
    glDebugMessageCallbackARB(SDLGLInterface::glDebugCB, nullptr);

    // initialize the state cache
    OpenGLStateCache::initialize();
}

void OpenGLLegacyInterface::beginScene() {
    this->bInScene = true;

    Matrix4 defaultProjectionMatrix =
        Camera::buildMatrixOrtho2D(0, this->vResolution.x, this->vResolution.y, 0, -1.0f, 1.0f);

    // push main transforms
    pushTransform();
    setProjectionMatrix(defaultProjectionMatrix);
    if(cv::r_globaloffset_x.getFloat() != cv::r_globaloffset_x.getDefaultFloat() ||
       cv::r_globaloffset_y.getFloat() != cv::r_globaloffset_y.getDefaultFloat())
        translate(cv::r_globaloffset_x.getFloat(), cv::r_globaloffset_y.getFloat());

    // and apply them
    updateTransform();

    // set clear color and clear
    // glClearColor(1, 1, 1, 1);
    // glClearColor(0.9568f, 0.9686f, 0.9882f, 1);
    glClearColor(0, 0, 0, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void OpenGLLegacyInterface::endScene() {
    popTransform();

    if constexpr(Env::cfg(BUILD::DEBUG)) {
        checkStackLeaks();

        if(this->clipRectStack.size() > 0) {
            engine->showMessageErrorFatal("ClipRect Stack Leak", "Make sure all push*() have a pop*()!");
            engine->shutdown();
        }
    }

    this->bInScene = false;
}

void OpenGLLegacyInterface::clearDepthBuffer() { glClear(GL_DEPTH_BUFFER_BIT); }

void OpenGLLegacyInterface::setColor(Color color) {
    if(color == this->color) return;

    this->color = color;

    glColor4ub(this->color.R(), this->color.G(), this->color.B(), this->color.A());
}

void OpenGLLegacyInterface::drawPixels(int x, int y, int width, int height, Graphics::DRAWPIXELS_TYPE type,
                                       const void *pixels) {
    glRasterPos2i(x, y + height);  // '+height' because of opengl bottom left origin, but engine top left origin
    glDrawPixels(width, height, GL_RGBA,
                 (type == Graphics::DRAWPIXELS_TYPE::DRAWPIXELS_UBYTE ? GL_UNSIGNED_BYTE : GL_FLOAT), pixels);
}

void OpenGLLegacyInterface::drawPixel(int x, int y) {
    updateTransform();

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_POINTS);
    {
        glVertex2i(x, y);
    }
    glEnd();
}

void OpenGLLegacyInterface::drawLinef(float x1, float y1, float x2, float y2) {
    updateTransform();

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_LINES);
    {
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
}

void OpenGLLegacyInterface::drawRectf(const RectOptions &opts) {
    updateTransform();

    glDisable(GL_TEXTURE_2D);

    // set line width if specified
    if(opts.lineThickness != 1.0f) {
        glLineWidth(opts.lineThickness);
    }

    glBegin(opts.withColor ? GL_LINES : GL_LINE_LOOP);
    if(opts.withColor) {
        setColor(opts.top);
        glVertex2f(opts.x, opts.y);
        glVertex2f(opts.x + opts.width, opts.y);

        setColor(opts.left);
        glVertex2f(opts.x, opts.y + opts.height);
        glVertex2f(opts.x, opts.y);

        setColor(opts.bottom);
        glVertex2f(opts.x + opts.width, opts.y + opts.height);
        glVertex2f(opts.x, opts.y + opts.height);

        setColor(opts.right);
        glVertex2f(opts.x + opts.width, opts.y);
        glVertex2f(opts.x + opts.width, opts.y + opts.height);
    } else {
        glVertex2f(opts.x, opts.y);
        glVertex2f(opts.x + opts.width, opts.y);
        glVertex2f(opts.x + opts.width, opts.y + opts.height);
        glVertex2f(opts.x, opts.y + opts.height);
    }
    glEnd();

    // restore line width
    if(opts.lineThickness != 1.0f) {
        glLineWidth(1.0f);
    }
}

void OpenGLLegacyInterface::fillRectf(float x, float y, float width, float height) {
    updateTransform();

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    {
        glVertex2f(x, y);
        glVertex2f(x, (y + height));
        glVertex2f((x + width), (y + height));
        glVertex2f((x + width), y);
    }
    glEnd();
}

void OpenGLLegacyInterface::fillRoundedRect(int x, int y, int width, int height, int radius) {
    float xOffset = x + radius;
    float yOffset = y + radius;

    double i = 0;
    const double factor = 0.05;

    updateTransform();

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_POLYGON);
    {
        // top left
        for(i = PI; i <= (1.5 * PI); i += factor) {
            glVertex2d(radius * std::cos(i) + xOffset, radius * std::sin(i) + yOffset);
        }

        // top right
        xOffset = x + width - radius;
        for(i = (1.5 * PI); i <= (2 * PI); i += factor) {
            glVertex2d(radius * std::cos(i) + xOffset, radius * std::sin(i) + yOffset);
        }

        // bottom right
        yOffset = y + height - radius;
        for(i = 0; i <= (0.5 * PI); i += factor) {
            glVertex2d(radius * std::cos(i) + xOffset, radius * std::sin(i) + yOffset);
        }

        // bottom left
        xOffset = x + radius;
        for(i = (0.5 * PI); i <= PI; i += factor) {
            glVertex2d(radius * std::cos(i) + xOffset, radius * std::sin(i) + yOffset);
        }
    }
    glEnd();
}

void OpenGLLegacyInterface::fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                                         Color bottomLeftColor, Color bottomRightColor) {
    updateTransform();

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    {
        setColor(topLeftColor);
        glVertex2i(x, y);

        setColor(topRightColor);
        glVertex2i((x + width), y);

        setColor(bottomRightColor);
        glVertex2i((x + width), (y + height));

        setColor(bottomLeftColor);
        glVertex2i(x, (y + height));
    }
    glEnd();
}

void OpenGLLegacyInterface::drawQuad(int x, int y, int width, int height) {
    updateTransform();

    const int left = x;
    const int right = (x + width);

    const int top = (y + height);
    const int bottom = y;

    glBegin(GL_QUADS);
    {
        glTexCoord2i(0, 0);
        glVertex2i(left, bottom);

        glTexCoord2i(0, 1);
        glVertex2i(left, top);

        glTexCoord2i(1, 1);
        glVertex2i(right, top);

        glTexCoord2i(1, 0);
        glVertex2i(right, bottom);
    }
    glEnd();
}

void OpenGLLegacyInterface::drawQuad(vec2 topLeft, vec2 topRight, vec2 bottomRight, vec2 bottomLeft, Color topLeftColor,
                                     Color topRightColor, Color bottomRightColor, Color bottomLeftColor) {
    updateTransform();

    glBegin(GL_QUADS);
    {
        setColor(topLeftColor);
        glTexCoord2f(0.f, 0.f);
        glVertex2f(topLeft.x, topLeft.y);

        setColor(bottomLeftColor);
        glTexCoord2f(0.f, 1.f);
        glVertex2f(bottomLeft.x, bottomLeft.y);

        setColor(bottomRightColor);
        glTexCoord2f(1.f, 1.f);
        glVertex2f(bottomRight.x, bottomRight.y);

        setColor(topRightColor);
        glTexCoord2f(1.f, 0.f);
        glVertex2f(topRight.x, topRight.y);
    }
    glEnd();
}

void OpenGLLegacyInterface::drawImage(const Image *image, AnchorPoint anchor, float edgeSoftness, McRect clipRect) {
    if(image == nullptr) {
        debugLog("WARNING: Tried to draw image with NULL texture!");
        return;
    }
    if(!image->isReady()) return;
    if(this->color.A() == 0) return;

    const bool clipRectSpecified = vec::length(clipRect.getSize()) != 0;
    bool smoothedEdges = edgeSoftness > 0.0f;

    // initialize shader on first use
    if(smoothedEdges) {
        if(!this->smoothClipShader) {
            this->initSmoothClipShader();
        }
        smoothedEdges = this->smoothClipShader->isReady();
    }

    const bool fallbackClip = clipRectSpecified && !smoothedEdges;

    if(fallbackClip) {
        pushClipRect(clipRect);
    }

    this->updateTransform();

    const float width = image->getWidth();
    const float height = image->getHeight();

    f32 x, y;
    switch(anchor) {
        case AnchorPoint::CENTER:
            x = -width / 2;
            y = -height / 2;
            break;
        case AnchorPoint::TOP_LEFT:
            x = 0;
            y = 0;
            break;
        case AnchorPoint::TOP_RIGHT:
            x = -width;
            y = 0;
            break;
        case AnchorPoint::BOTTOM_LEFT:
            x = 0;
            y = -height;
            break;
        case AnchorPoint::LEFT:
            x = 0;
            y = -height / 2;
            break;
        default:
            abort();
    }

    if(smoothedEdges && !clipRectSpecified) {
        // set a default clip rect as the exact image size if one wasn't explicitly passed, but we still want smoothing
        clipRect = McRect{x, y, width, height};
    }

    if(smoothedEdges) {
        // compensate for viewport changed by rendertargets
        // flip Y for engine<->opengl coordinate origin
        const auto &viewport{OpenGLStateCache::getCurrentViewport()};
        float clipMinX{clipRect.getX() + viewport[0]},                                           //
            clipMinY{viewport[3] - (clipRect.getY() - viewport[1] - 1 + clipRect.getHeight())},  //
            clipMaxX{clipMinX + clipRect.getWidth()},                                            //
            clipMaxY{clipMinY + clipRect.getHeight()};                                           //

        this->smoothClipShader->enable();
        this->smoothClipShader->setUniform2f("rect_min", clipMinX, clipMinY);
        this->smoothClipShader->setUniform2f("rect_max", clipMaxX, clipMaxY);
        this->smoothClipShader->setUniform1f("edge_softness", edgeSoftness);
        //this->smoothClipShader->setUniform1i("texture", 0);
    }

    image->bind();
    {
        glBegin(GL_QUADS);
        {
            glTexCoord2f(0, 0);
            glVertex2f(x, y);

            glTexCoord2f(0, 1);
            glVertex2f(x, (y + height));

            glTexCoord2f(1, 1);
            glVertex2f((x + width), (y + height));

            glTexCoord2f(1, 0);
            glVertex2f((x + width), y);
        }
        glEnd();
    }
    if(cv::r_image_unbind_after_drawimage.getBool()) image->unbind();

    if(smoothedEdges) {
        this->smoothClipShader->disable();
    } else if(fallbackClip) {
        popClipRect();
    }

    if(cv::r_debug_drawimage.getBool()) {
        this->setColor(0xbbff00ff);
        this->drawRect(x, y, width - 1, height - 1);
    }
}

void OpenGLLegacyInterface::drawString(McFont *font, const UString &text) {
    if(font == nullptr || text.length() < 1 || !font->isReady()) return;

    updateTransform();

    if(cv::r_debug_flush_drawstring.getBool()) {
        glFinish();
        glFlush();
        glFinish();
        glFlush();
    }

    font->drawString(text);
}

void OpenGLLegacyInterface::drawVAO(VertexArrayObject *vao) {
    if(vao == nullptr) return;

    updateTransform();

    // HACKHACK: disable texturing for special primitives, also for untextured vaos
    if(vao->getPrimitive() == Graphics::PRIMITIVE::PRIMITIVE_LINES ||
       vao->getPrimitive() == Graphics::PRIMITIVE::PRIMITIVE_LINE_STRIP || !vao->hasTexcoords())
        glDisable(GL_TEXTURE_2D);

    // if baked, then we can directly draw the buffer
    if(vao->isReady()) {
        vao->draw();
        return;
    }

    // fetch data references
    const auto &vertices = vao->getVertices();
    const auto &texcoords = vao->getTexcoords();
    const auto &normals = vao->getNormals();
    const auto &colors = vao->getColors();

    const bool hasTexcoords0 = vao->hasTexcoords() && !texcoords.empty();
    const bool hasNormals = !normals.empty();
    const bool hasColors = !colors.empty();

    size_t drawCount = vertices.size();

    // only texture unit 0 handled atm (all that's actually used anyways)
    if(hasTexcoords0) {
        drawCount = std::min(drawCount, texcoords.size());
    }
    if(hasNormals) {
        drawCount = std::min(drawCount, normals.size());
    }
    if(hasColors) {
        drawCount = std::min(drawCount, colors.size());
    }

    if(drawCount == 0) {
        return;
    }

    // unbind any previous VBO/VAO to ensure client-side arrays are used correctly
    OpenGLStateCache::bindArrayBuffer(0);
    if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
        glBindVertexArray(0);
    }

    // enable and set vertex array (always present)
    OpenGLStateCache::enableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices.data());

    // handle texcoords (only unit 0)
    if(hasTexcoords0) {
        OpenGLStateCache::enableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.data());
    } else {
        OpenGLStateCache::disableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    // handle colors
    std::vector<Color> swapped_colors;
    if(hasColors) {
        // check if any color needs conversion (only R and B are swapped)
        bool needsConversion = false;
        MC_UNROLL
        for(size_t i = 0; i < drawCount; ++i) {
            if(colors[i].R() != colors[i].B()) {
                needsConversion = true;
                break;
            }
        }
        // (create temporary swapped buffer for correct RGBA byte order)
        // TODO: just store them properly in the first place
        if(needsConversion) {
            swapped_colors.reserve(drawCount);
            for(size_t i = 0; i < drawCount; ++i) {
                swapped_colors.push_back(abgr(colors[i]));
            }
            OpenGLStateCache::enableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, swapped_colors.data());
        } else {
            OpenGLStateCache::enableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors.data());
        }
    } else {
        OpenGLStateCache::disableClientState(GL_COLOR_ARRAY);
    }

    // handle normals
    if(hasNormals) {
        OpenGLStateCache::enableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, normals.data());
    } else {
        OpenGLStateCache::disableClientState(GL_NORMAL_ARRAY);
    }

    // draw using client-side arrays
    glDrawArrays(SDLGLInterface::primitiveToOpenGLMap[vao->getPrimitive()], 0, static_cast<GLint>(drawCount));
}

void OpenGLLegacyInterface::setClipRect(McRect clipRect) {
    if(cv::r_debug_disable_cliprect.getBool()) return;
    // if (m_bIs3DScene) return; // TODO

    // rendertargets change the current viewport
    const auto &viewport{OpenGLStateCache::getCurrentViewport()};

    // debugLog("viewport = {}, {}, {}, {}", viewport[0], viewport[1], viewport[2], viewport[3]);

    glEnable(GL_SCISSOR_TEST);
    glScissor((int)clipRect.getX() + viewport[0],
              viewport[3] - ((int)clipRect.getY() - viewport[1] - 1 + (int)clipRect.getHeight()),
              (int)clipRect.getWidth(), (int)clipRect.getHeight());

    // debugLog("scissor = {}, {}, {}, {}", (int)clipRect.getX()+viewport[0],
}

void OpenGLLegacyInterface::pushClipRect(McRect clipRect) {
    if(this->clipRectStack.size() > 0)
        this->clipRectStack.push(this->clipRectStack.top().intersect(clipRect));
    else
        this->clipRectStack.push(clipRect);

    setClipRect(this->clipRectStack.top());
}

void OpenGLLegacyInterface::popClipRect() {
    this->clipRectStack.pop();

    if(this->clipRectStack.size() > 0)
        setClipRect(this->clipRectStack.top());
    else
        setClipping(false);
}

void OpenGLLegacyInterface::pushStencil() {
    // init and clear
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);

    // set mask
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
}

void OpenGLLegacyInterface::fillStencil(bool inside) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_NOTEQUAL, inside ? 0 : 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

void OpenGLLegacyInterface::popStencil() { glDisable(GL_STENCIL_TEST); }

void OpenGLLegacyInterface::setClipping(bool enabled) {
    if(enabled) {
        if(this->clipRectStack.size() > 0) glEnable(GL_SCISSOR_TEST);
    } else
        glDisable(GL_SCISSOR_TEST);
}

void OpenGLLegacyInterface::setAlphaTesting(bool enabled) {
    if(enabled)
        glEnable(GL_ALPHA_TEST);
    else
        glDisable(GL_ALPHA_TEST);
}

void OpenGLLegacyInterface::setAlphaTestFunc(COMPARE_FUNC alphaFunc, float ref) {
    glAlphaFunc(SDLGLInterface::compareFuncToOpenGLMap[alphaFunc], ref);
}

void OpenGLLegacyInterface::setBlending(bool enabled) {
    if(enabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}

void OpenGLLegacyInterface::setBlendMode(BLEND_MODE blendMode) {
    switch(blendMode) {
        case BLEND_MODE::BLEND_MODE_ALPHA:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BLEND_MODE::BLEND_MODE_ADDITIVE:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case BLEND_MODE::BLEND_MODE_PREMUL_ALPHA:
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BLEND_MODE::BLEND_MODE_PREMUL_COLOR:
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
}

void OpenGLLegacyInterface::setDepthBuffer(bool enabled) {
    if(enabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

void OpenGLLegacyInterface::setDepthWriting(bool enabled) {
    if(enabled)
        glDepthMask(GL_TRUE);
    else
        glDepthMask(GL_FALSE);
}

void OpenGLLegacyInterface::setColorWriting(bool r, bool g, bool b, bool a) { glColorMask(r, g, b, a); }

void OpenGLLegacyInterface::setColorInversion(bool enabled) {
    if(enabled) {
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(GL_COPY_INVERTED);
    } else {
        glDisable(GL_COLOR_LOGIC_OP);
    }
}

void OpenGLLegacyInterface::setCulling(bool culling) {
    if(culling)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
}

void OpenGLLegacyInterface::setAntialiasing(bool aa) {
    this->bAntiAliasing = aa;
    if(aa)
        glEnable(GL_MULTISAMPLE);
    else
        glDisable(GL_MULTISAMPLE);
}

void OpenGLLegacyInterface::setWireframe(bool enabled) {
    if(enabled)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void OpenGLLegacyInterface::setLineWidth(float width) { glLineWidth(width); }

void OpenGLLegacyInterface::flush() { glFlush(); }

std::vector<u8> OpenGLLegacyInterface::getScreenshot(bool withAlpha) {
    std::vector<u8> result;
    i32 width = this->vResolution.x;
    i32 height = this->vResolution.y;

    // sanity check
    if(width > 65535 || height > 65535 || width < 1 || height < 1) {
        engine->showMessageError("Renderer Error", "getScreenshot() called with invalid arguments!");
        return result;
    }

    const u8 numChannels = withAlpha ? 4 : 3;
    const u32 numElements = width * height * numChannels;
    const GLenum glFormat = withAlpha ? GL_RGBA : GL_RGB;

    // take screenshot
    u8 *pixels = new u8[numElements];
    glFinish();
    for(sSz y = 0; y < height; y++)  // flip it while reading
    {
        glReadPixels(0, (height - (y + 1)), width, 1, glFormat, GL_UNSIGNED_BYTE, &(pixels[y * width * numChannels]));
    }

    // fix alpha channel (set all pixels to fully opaque)
    // TODO: allow proper transparent screenshots
    if(withAlpha) {
        for(u32 i = 3; i < numElements; i += numChannels) {
            pixels[i] = 255;
        }
    }

    // copy to vector
    result.resize(numElements);
    result.assign(pixels, pixels + numElements);
    delete[] pixels;

    return result;
}

void OpenGLLegacyInterface::onResolutionChange(vec2 newResolution) {
    // rebuild viewport
    this->vResolution = newResolution;
    glViewport(0, 0, this->vResolution.x, this->vResolution.y);

    // update state cache with the new viewport
    OpenGLStateCache::setCurrentViewport(0, 0, this->vResolution.x, this->vResolution.y);

    // special case: custom rendertarget resolution rendering, update active projection matrix immediately
    if(this->bInScene) {
        this->projectionTransformStack.top() =
            Camera::buildMatrixOrtho2D(0, this->vResolution.x, this->vResolution.y, 0, -1.0f, 1.0f);
        this->bTransformUpToDate = false;
    }
}

Image *OpenGLLegacyInterface::createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) {
    return new OpenGLImage(filePath, mipmapped, keepInSystemMemory);
}

Image *OpenGLLegacyInterface::createImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) {
    return new OpenGLImage(width, height, mipmapped, keepInSystemMemory);
}

RenderTarget *OpenGLLegacyInterface::createRenderTarget(int x, int y, int width, int height,
                                                        Graphics::MULTISAMPLE_TYPE multiSampleType) {
    return new OpenGLRenderTarget(x, y, width, height, multiSampleType);
}

Shader *OpenGLLegacyInterface::createShaderFromFile(std::string vertexShaderFilePath,
                                                    std::string fragmentShaderFilePath) {
    return new OpenGLShader(vertexShaderFilePath, fragmentShaderFilePath, false);
}

Shader *OpenGLLegacyInterface::createShaderFromSource(std::string vertexShader, std::string fragmentShader) {
    return new OpenGLShader(vertexShader, fragmentShader, true);
}

VertexArrayObject *OpenGLLegacyInterface::createVertexArrayObject(Graphics::PRIMITIVE primitive,
                                                                  Graphics::USAGE_TYPE usage, bool keepInSystemMemory) {
    return new OpenGLVertexArrayObject(primitive, usage, keepInSystemMemory);
}

void OpenGLLegacyInterface::onTransformUpdate(Matrix4 &projectionMatrix, Matrix4 &worldMatrix) {
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projectionMatrix.get());

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(worldMatrix.get());
}

void OpenGLLegacyInterface::initSmoothClipShader() {
    if(this->smoothClipShader != nullptr) return;

    this->smoothClipShader.reset(this->createShaderFromSource(
        std::string(reinterpret_cast<const char *>(GL_smoothclip_vsh), GL_smoothclip_vsh_size()),
        std::string(reinterpret_cast<const char *>(GL_smoothclip_fsh), GL_smoothclip_fsh_size())));

    if(this->smoothClipShader != nullptr) {
        this->smoothClipShader->loadAsync();
        this->smoothClipShader->load();
    }
}

#endif
