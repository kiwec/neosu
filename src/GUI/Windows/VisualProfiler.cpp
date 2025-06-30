#include "VisualProfiler.h"

#include <string.h>

#include "AnimationHandler.h"
#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Profiler.h"
#include "ResourceManager.h"
#include "SoundEngine.h"

using namespace std;

VisualProfiler *vprof = NULL;

VisualProfiler::VisualProfiler() : CBaseUIElement(0, 0, 0, 0, "") {
    vprof = this;

    this->spike.node.depth = -1;
    this->spike.node.node = NULL;
    this->spike.timeLastFrame = 0.0;

    this->iPrevVaoWidth = 0;
    this->iPrevVaoHeight = 0;
    this->iPrevVaoGroups = 0;
    this->fPrevVaoMaxRange = 0.0f;
    this->fPrevVaoAlpha = -1.0f;

    this->iCurLinePos = 0;

    this->iDrawGroupID = -1;
    this->iDrawSwapBuffersGroupID = -1;

    this->spikeIDCounter = 0;

    this->setProfile(&g_profCurrentProfile);  // by default we look at the standard full engine-wide profile

    this->font = resourceManager->getFont("FONT_DEFAULT");
    this->fontConsole = resourceManager->getFont("FONT_CONSOLE");
    this->lineVao = resourceManager->createVertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_LINES,
                                                             Graphics::USAGE_TYPE::USAGE_DYNAMIC, true);

    this->bScheduledForceRebuildLineVao = false;
    this->bRequiresAltShiftKeysToFreeze = false;
}

VisualProfiler::~VisualProfiler() { vprof = NULL; }

void VisualProfiler::draw() {
    VPROF_BUDGET("VisualProfiler::draw", VPROF_BUDGETGROUP_DRAW);
    if(!cv_vprof.getBool() || !this->bVisible) return;

    // draw miscellaneous debug infos
    const int displayMode = cv_vprof_display_mode.getInt();
    if(displayMode != INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_DEFAULT) {
        McFont *textFont = this->font;
        float textScale = 1.0f;
        {
            switch(displayMode) {
                case INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_GPU_INFO: {
                    const int vramTotalMB = g->getVRAMTotal() / 1024;
                    const int vramRemainingMB = g->getVRAMRemaining() / 1024;

                    UString vendor = g->getVendor();
                    UString model = g->getModel();
                    UString version = g->getVersion();

                    UString line1 = "GPU Vendor: ";
                    line1.append(vendor);
                    addTextLine(line1, textFont, this->textLines);
                    UString line2 = "Model: ";
                    line2.append(model);
                    addTextLine(line2, textFont, this->textLines);
                    UString line3 = "Version: ";
                    line3.append(version);
                    addTextLine(line3, textFont, this->textLines);
                    addTextLine(
                        UString::format("Resolution: %i x %i", (int)g->getResolution().x, (int)g->getResolution().y),
                        textFont, this->textLines);
                    addTextLine(UString::format("NativeRes: %i x %i", (int)env->getNativeScreenSize().x,
                                                (int)env->getNativeScreenSize().y),
                                textFont, this->textLines);
                    addTextLine(UString::format("Env DPI Scale: %f", env->getDPIScale()), textFont, this->textLines);
                    addTextLine(UString::format("Env DPI: %i", (int)env->getDPI()), textFont, this->textLines);
                    // addTextLine(UString::format("Renderer: %s", typeid().name()), textFont, this->textLines); //
                    // TODO: add g->getName() or something
                    addTextLine(UString::format("VRAM: %i MB / %i MB", vramRemainingMB, vramTotalMB), textFont,
                                this->textLines);
                } break;

                case INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_ENGINE_INFO: {
                    textFont = this->fontConsole;
                    textScale = std::round(env->getDPIScale() + 0.255f);

                    const double time = engine->getTime();
                    const double timeRunning = engine->getTimeRunning();
                    const double dilation = (timeRunning - time);
                    const Vector2 envMousePos = env->getMousePos();

                    addTextLine(UString::format("ConVars: %zu", convar->getConVarArray().size()), textFont,
                                this->textLines);
                    addTextLine(UString::format("Monitor: [%i] of %zu", env->getMonitor(), env->getMonitors().size()),
                                textFont, this->textLines);
                    addTextLine(UString::format("Env Mouse Pos: %i x %i", (int)envMousePos.x, (int)envMousePos.y),
                                textFont, this->textLines);
                    addTextLine(UString::format("Sound Device: %s", soundEngine->getOutputDeviceName().toUtf8()),
                                textFont, this->textLines);
                    addTextLine(UString::format("Sound Volume: %f", soundEngine->getVolume()), textFont,
                                this->textLines);
                    addTextLine(UString::format("RM Threads: %zu", resourceManager->getNumActiveThreads()), textFont,
                                this->textLines);
                    addTextLine(UString::format("RM LoadingWork: %zu", resourceManager->getNumLoadingWork()), textFont,
                                this->textLines);
                    addTextLine(
                        UString::format("RM LoadingWorkAD: %zu", resourceManager->getNumLoadingWorkAsyncDestroy()),
                        textFont, this->textLines);
                    addTextLine(UString::format("RM Named Resources: %zu", resourceManager->getResources().size()),
                                textFont, this->textLines);
                    addTextLine(UString::format("Animations: %zu", anim->getNumActiveAnimations()), textFont,
                                this->textLines);
                    addTextLine(UString::format("Frame: %lu", engine->getFrameCount()), textFont, this->textLines);
                    addTextLine(UString::format("Time: %f", time), textFont, this->textLines);
                    addTextLine(UString::format("Realtime: %f", timeRunning), textFont, this->textLines);
                    addTextLine(UString::format("Time Dilation: %f", dilation), textFont, this->textLines);
                } break;

                case INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_APP_INFO: {
                    textFont = this->fontConsole;
                    textScale = std::round(env->getDPIScale() + 0.255f);

                    for(size_t i = 0; i < this->appTextLines.size(); i++) {
                        addTextLine(this->appTextLines[i], textFont, this->textLines);
                    }

                    if(this->appTextLines.size() < 1) addTextLine("(Empty)", textFont, this->textLines);
                } break;
            }
        }

        if(this->textLines.size() > 0) {
            const Color textColor = 0xffffffff;
            const int margin = cv_vprof_graph_margin.getFloat() * env->getDPIScale();
            const int marginBox = 6 * env->getDPIScale();

            int largestLineWidth = 0;
            for(const TEXT_LINE &textLine : this->textLines) {
                if(textLine.width > largestLineWidth) largestLineWidth = textLine.width;
            }
            largestLineWidth *= textScale;

            const int boxX = -margin + (engine->getScreenWidth() - largestLineWidth) - marginBox;
            const int boxY = margin - marginBox;
            const int boxWidth = largestLineWidth + 2 * marginBox;
            const int boxHeight = marginBox * 2 + textFont->getHeight() * textScale +
                                  (textFont->getHeight() * textScale * 1.5f) * (this->textLines.size() - 1);

            // draw background
            g->setColor(0xaa000000);
            g->fillRect(boxX - 1, boxY - 1, boxWidth + 1, boxHeight + 1);
            g->setColor(0xff777777);
            g->drawRect(boxX - 1, boxY - 1, boxWidth + 1, boxHeight + 1);

            // draw text
            g->pushTransform();
            {
                g->scale(textScale, textScale);
                g->translate(-margin, (int)(textFont->getHeight() * textScale + margin));

                for(size_t i = 0; i < this->textLines.size(); i++) {
                    if(i > 0) g->translate(0, (int)(textFont->getHeight() * textScale * 1.5f));

                    g->pushTransform();
                    {
                        g->translate(engine->getScreenWidth() - this->textLines[i].width * textScale, 0);
                        drawStringWithShadow(textFont, this->textLines[i].text, textColor);
                    }
                    g->popTransform();
                }
            }
            g->popTransform();
        }

        this->textLines.clear();
        this->appTextLines.clear();
    }

    // draw profiler node tree extended details
    if(cv_debug_vprof.getBool()) {
        VPROF_BUDGET("DebugText", VPROF_BUDGETGROUP_DRAW);

        g->setColor(0xffcccccc);
        g->pushTransform();
        {
            g->translate(0, this->font->getHeight());

            g->drawString(this->font, UString::format("%i nodes", this->profile->getNumNodes()));
            g->translate(0, this->font->getHeight() * 1.5f);

            g->drawString(this->font, UString::format("%i groups", this->profile->getNumGroups()));
            g->translate(0, this->font->getHeight() * 1.5f);

            g->drawString(this->font, "----------------------------------------------------");
            g->translate(0, this->font->getHeight() * 1.5f);

            for(int i = this->nodes.size() - 1; i >= 0; i--) {
                g->pushTransform();
                {
                    g->translate(this->font->getHeight() * 3 * (this->nodes[i].depth - 1), 0);
                    g->drawString(this->font,
                                  UString::format("[%s] - %s = %f ms", this->nodes[i].node->getName(),
                                                  this->profile->getGroupName(this->nodes[i].node->getGroupID()),
                                                  this->nodes[i].node->getTimeLastFrame() * 1000.0));
                }
                g->popTransform();

                g->translate(0, this->font->getHeight() * 1.5f);
            }

            g->drawString(this->font, "----------------------------------------------------");
            g->translate(0, this->font->getHeight() * 1.5f);

            for(int i = 0; i < this->profile->getNumGroups(); i++) {
                const char *groupName = this->profile->getGroupName(i);
                const double sum = this->profile->sumTimes(i);

                g->drawString(this->font, UString::format("%s = %f ms", groupName, sum * 1000.0));
                g->translate(0, this->font->getHeight() * 1.5f);
            }
        }
        g->popTransform();
    }

    // draw extended spike details tree (profiler node snapshot)
    if(cv_vprof_spike.getBool() && !cv_debug_vprof.getBool()) {
        if(this->spike.node.node != NULL) {
            if(cv_vprof_spike.getInt() == 2) {
                VPROF_BUDGET("DebugText", VPROF_BUDGETGROUP_DRAW);

                g->setColor(0xffcccccc);
                g->pushTransform();
                {
                    g->translate(0, this->font->getHeight());

                    for(int i = this->spikeNodes.size() - 1; i >= 0; i--) {
                        g->pushTransform();
                        {
                            g->translate(this->font->getHeight() * 3 * (this->spikeNodes[i].node.depth - 1), 0);
                            g->drawString(this->font,
                                          UString::format(
                                              "[%s] - %s = %f ms", this->spikeNodes[i].node.node->getName(),
                                              this->profile->getGroupName(this->spikeNodes[i].node.node->getGroupID()),
                                              this->spikeNodes[i].timeLastFrame * 1000.0));
                        }
                        g->popTransform();

                        g->translate(0, this->font->getHeight() * 1.5f);
                    }
                }
                g->popTransform();
            }
        }
    }

    // draw graph
    if(cv_vprof_graph.getBool()) {
        VPROF_BUDGET("LineGraph", VPROF_BUDGETGROUP_DRAW);

        const int width = getGraphWidth();
        const int height = getGraphHeight();
        const int margin = cv_vprof_graph_margin.getFloat() * env->getDPIScale();

        const int xPos = engine->getScreenWidth() - width - margin;
        const int yPos = engine->getScreenHeight() - height - margin +
                         (mouse->isMiddleDown() ? mouse->getPos().y - engine->getScreenHeight() : 0);

        // draw background
        g->setColor(0xaa000000);
        g->fillRect(xPos - 1, yPos - 1, width + 1, height + 1);
        g->setColor(0xff777777);
        g->drawRect(xPos - 1, yPos - 1, width + 1, height + 1);

        // draw lines
        g->setColor(0xff00aa00);
        g->pushTransform();
        {
            const int stride = 2 * this->iPrevVaoGroups;

            // behind
            this->lineVao->setDrawRange(this->iCurLinePos * stride, this->iPrevVaoWidth * stride);
            g->translate(xPos + 1 - this->iCurLinePos, yPos + height);
            g->drawVAO(this->lineVao);

            // forward
            this->lineVao->setDrawRange(0, this->iCurLinePos * stride);
            g->translate(this->iPrevVaoWidth, 0);
            g->drawVAO(this->lineVao);
        }
        g->popTransform();

        // draw labels
        if(keyboard->isControlDown()) {
            const int margin = 3 * env->getDPIScale();

            // y-axis range
            g->pushTransform();
            {
                g->translate((int)(xPos + margin), (int)(yPos + this->font->getHeight() + margin));
                drawStringWithShadow(this->font, UString::format("%g ms", cv_vprof_graph_range_max.getFloat()),
                                     0xffffffff);

                g->translate(0, (int)(height - this->font->getHeight() - 2 * margin));
                drawStringWithShadow(this->font, "0 ms", 0xffffffff);
            }
            g->popTransform();

            // colored group names
            g->pushTransform();
            {
                const int padding = 6 * env->getDPIScale();

                g->translate((int)(xPos - 3 * margin), (int)(yPos + height - padding));

                for(size_t i = 1; i < this->groups.size(); i++) {
                    const int stringWidth = (int)(this->font->getStringWidth(this->groups[i].name));
                    g->translate(-stringWidth, 0);
                    drawStringWithShadow(this->font, this->groups[i].name, this->groups[i].color);
                    g->translate(stringWidth, (int)(-this->font->getHeight() - padding));
                }
            }
            g->popTransform();
        }

        // draw top spike text above graph
        if(cv_vprof_spike.getBool() && !cv_debug_vprof.getBool()) {
            if(this->spike.node.node != NULL) {
                if(cv_vprof_spike.getInt() == 1) {
                    const int margin = 6 * env->getDPIScale();

                    g->setColor(0xffcccccc);
                    g->pushTransform();
                    {
                        g->translate((int)(xPos + margin), (int)(yPos - 2 * margin));
                        /// drawStringWithShadow(UString::format("[%s] = %g ms", this->spike.node.node->getName(),
                        /// m_spike.timeLastFrame * 1000.0), this->groups[m_spike.node.node->getGroupID()].color);
                        g->drawString(this->font, UString::format("Spike = %g ms", this->spike.timeLastFrame * 1000.0));
                    }
                    g->popTransform();
                }
            }
        }
    }
}

void VisualProfiler::drawStringWithShadow(McFont *font, const UString &string, Color color) {
    if(font == NULL) return;

    const int shadowOffset = 1 * env->getDPIScale();

    g->translate(shadowOffset, shadowOffset);
    {
        g->setColor(0xff000000);
        g->drawString(font, string);
        g->translate(-shadowOffset, -shadowOffset);
        {
            g->setColor(color);
            g->drawString(font, string);
        }
    }
}

void VisualProfiler::mouse_update(bool *propagate_clicks) {
    VPROF_BUDGET("VisualProfiler::update", VPROF_BUDGETGROUP_UPDATE);
    CBaseUIElement::mouse_update(propagate_clicks);
    if(!cv_vprof.getBool() || !this->bVisible) return;

    const bool isFrozen = (keyboard->isShiftDown() && (!this->bRequiresAltShiftKeysToFreeze || keyboard->isAltDown()));

    if(cv_debug_vprof.getBool() || cv_vprof_spike.getBool()) {
        if(!isFrozen) {
            SPIKE spike;
            {
                spike.node.depth = -1;
                spike.node.node = NULL;
                spike.timeLastFrame = 0.0;
                spike.id = this->spikeIDCounter++;
            }

            // run regular debug node collector
            this->nodes.clear();
            collectProfilerNodesRecursive(this->profile->getRoot(), 0, this->nodes, spike);

            // run spike collector and updater
            if(cv_vprof_spike.getBool()) {
                const int graphWidth = getGraphWidth();

                this->spikes.push_back(spike);

                if(this->spikes.size() > graphWidth) this->spikes.erase(this->spikes.begin());

                SPIKE &newSpike = this->spikes[0];

                for(size_t i = 0; i < this->spikes.size(); i++) {
                    if(this->spikes[i].timeLastFrame > newSpike.timeLastFrame) newSpike = this->spikes[i];
                }

                if(newSpike.id != this->spike.id) {
                    const bool isNewSpikeLarger = (newSpike.timeLastFrame > this->spike.timeLastFrame);

                    this->spike = newSpike;

                    // NOTE: since we only store 1 spike snapshot, once that is erased (this->spikes.size() >
                    // graphWidth) and we have to "fall back" to a "lower" spike, we don't have any data on that lower
                    // spike anymore so, we simply only create a new snapshot if we have a new larger spike (since that
                    // is guaranteed to be the currently active one, i.e. going through node data in m_profile will
                    // return its data) (storing graphWidth amounts of snapshots seems unnecessarily wasteful, and I
                    // like this solution)
                    if(isNewSpikeLarger) {
                        this->spikeNodes.clear();
                        collectProfilerNodesSpikeRecursive(this->spike.node.node, 1, this->spikeNodes);
                    }
                }
            }
        }
    }

    // lazy rebuild group/color list
    if(this->groups.size() < this->profile->getNumGroups()) {
        // reset
        this->iDrawGroupID = -1;
        this->iDrawSwapBuffersGroupID = -1;

        const int curNumGroups = this->groups.size();
        const int actualNumGroups = this->profile->getNumGroups();

        for(int i = curNumGroups; i < actualNumGroups; i++) {
            GROUP group;

            group.name = this->profile->getGroupName(i);
            group.id = i;

            // hardcoded colors for some groups
            if(strcmp(group.name, VPROF_BUDGETGROUP_ROOT) ==
               0)  // NOTE: VPROF_BUDGETGROUP_ROOT is used for drawing the profiling overhead time if
                   // cv_vprof_graph_draw_overhead is enabled
                group.color = 0xffffffff;
            else if(strcmp(group.name, VPROF_BUDGETGROUP_SLEEP) == 0)
                group.color = 0xff5555bb;
            else if(strcmp(group.name, VPROF_BUDGETGROUP_WNDPROC) == 0)
                group.color = 0xffffff00;
            else if(strcmp(group.name, VPROF_BUDGETGROUP_UPDATE) == 0)
                group.color = 0xff00bb00;
            else if(strcmp(group.name, VPROF_BUDGETGROUP_DRAW) == 0) {
                group.color = 0xffbf6500;
                this->iDrawGroupID = group.id;
            } else if(strcmp(group.name, VPROF_BUDGETGROUP_DRAW_SWAPBUFFERS) == 0) {
                group.color = 0xffff0000;
                this->iDrawSwapBuffersGroupID = group.id;
            } else
                group.color = 0xff00ffff;  // default to turquoise

            this->groups.push_back(group);
        }
    }

    // and handle line updates
    {
        const int numGroups = this->groups.size();
        const int graphWidth = getGraphWidth();
        const int graphHeight = getGraphHeight();
        const float maxRange = cv_vprof_graph_range_max.getFloat();
        const float alpha = cv_vprof_graph_alpha.getFloat();

        // lazy rebuild line vao if parameters change
        if(this->bScheduledForceRebuildLineVao || this->iPrevVaoWidth != graphWidth ||
           this->iPrevVaoHeight != graphHeight || this->iPrevVaoGroups != numGroups ||
           this->fPrevVaoMaxRange != maxRange || this->fPrevVaoAlpha != alpha) {
            this->bScheduledForceRebuildLineVao = false;

            this->iPrevVaoWidth = graphWidth;
            this->iPrevVaoHeight = graphHeight;
            this->iPrevVaoGroups = numGroups;
            this->fPrevVaoMaxRange = maxRange;
            this->fPrevVaoAlpha = alpha;

            this->lineVao->release();

            // preallocate 2 vertices per line
            for(int x = 0; x < graphWidth; x++) {
                for(int g = 0; g < numGroups; g++) {
                    const Color color = argb(this->fPrevVaoAlpha, this->groups[g].color.Rf(),
                                             this->groups[g].color.Gf(), this->groups[g].color.Bf());

                    // this->lineVao->addVertex(x, -(((float)graphHeight)/(float)numGroups)*g, 0);
                    this->lineVao->addVertex(x, 0, 0);
                    this->lineVao->addColor(color);

                    // this->lineVao->addVertex(x, -(((float)graphHeight)/(float)numGroups)*(g + 1), 0);
                    this->lineVao->addVertex(x, 0, 0);
                    this->lineVao->addColor(color);
                }
            }

            // and bake
            resourceManager->loadResource(this->lineVao);
        }

        // regular line update
        if(!isFrozen) {
            if(this->lineVao->isReady()) {
                // one new multi-line per frame
                this->iCurLinePos = this->iCurLinePos % graphWidth;

                // if enabled, calculate and draw overhead
                // the overhead is the time spent between not having any profiler node active/alive, and should always
                // be <= 0 it is usually slightly negative (in the order of 10 microseconds, includes rounding errors
                // and timer inaccuracies) if the overhead ever gets positive then either there are no nodes covering
                // all paths below VPROF_MAIN(), or there is a serious problem with measuring time via
                // engine->getTimeReal()
                double profilingOverheadTime = 0.0;
                if(cv_vprof_graph_draw_overhead.getBool()) {
                    const int rootGroupID = 0;
                    double sumGroupTimes = 0.0;
                    {
                        for(size_t i = 1; i < this->groups.size(); i++)  // NOTE: start at 1, ignore rootGroupID
                        {
                            sumGroupTimes += this->profile->sumTimes(this->groups[i].id);
                        }
                    }
                    profilingOverheadTime = std::max(0.0, this->profile->sumTimes(rootGroupID) - sumGroupTimes);
                }

                // go through every group and build the new multi-line
                int heightCounter = 0;
                for(int i = 0; i < numGroups && (size_t)i < this->groups.size(); i++) {
                    const double rawDuration =
                        (i == 0 ? profilingOverheadTime : this->profile->sumTimes(this->groups[i].id));
                    const double duration =
                        (i == this->iDrawGroupID
                             ? rawDuration - this->profile->sumTimes(this->iDrawSwapBuffersGroupID)
                             : rawDuration);  // special case: hardcoded fix for nested groups (only draw + swap atm)
                    const int lineHeight = (int)(((duration * 1000.0) / (double)maxRange) * (double)graphHeight);

                    this->lineVao->setVertex(this->iCurLinePos * numGroups * 2 + i * 2, this->iCurLinePos,
                                             heightCounter);
                    this->lineVao->setVertex(this->iCurLinePos * numGroups * 2 + i * 2 + 1, this->iCurLinePos,
                                             heightCounter - lineHeight);

                    heightCounter -= lineHeight;
                }

                // re-bake
                resourceManager->loadResource(this->lineVao);

                this->iCurLinePos++;
            }
        }
    }
}

void VisualProfiler::incrementInfoBladeDisplayMode() {
    cv_vprof_display_mode.setValue((cv_vprof_display_mode.getInt() + 1) %
                                   INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_COUNT);
}

void VisualProfiler::decrementInfoBladeDisplayMode() {
    if(cv_vprof_display_mode.getInt() - 1 < INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_DEFAULT)
        cv_vprof_display_mode.setValue(INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_COUNT - 1);
    else
        cv_vprof_display_mode.setValue(cv_vprof_display_mode.getInt() - 1);
}

void VisualProfiler::addInfoBladeAppTextLine(const UString &text) {
    if(!cv_vprof.getBool() || !this->bVisible ||
       cv_vprof_display_mode.getInt() != INFO_BLADE_DISPLAY_MODE::INFO_BLADE_DISPLAY_MODE_APP_INFO)
        return;

    this->appTextLines.push_back(text);
}

void VisualProfiler::setProfile(ProfilerProfile *profile) {
    this->profile = profile;

    // force everything to get re-built for the new profile with the next frame
    {
        this->groups.clear();

        this->bScheduledForceRebuildLineVao = true;
    }
}

bool VisualProfiler::isEnabled() { return cv_vprof.getBool(); }

void VisualProfiler::collectProfilerNodesRecursive(const ProfilerNode *node, int depth, std::vector<NODE> &nodes,
                                                   SPIKE &spike) {
    if(node == NULL) return;

    // recursive call
    ProfilerNode *child = node->getChild();
    while(child != NULL) {
        collectProfilerNodesRecursive(child, depth + 1, nodes, spike);
        child = child->getSibling();
    }

    // add node (ignore root 0)
    if(depth > 0) {
        NODE entry;

        entry.node = node;
        entry.depth = depth;

        nodes.push_back(entry);

        const double timeLastFrame = node->getTimeLastFrame();
        if(spike.node.node == NULL || timeLastFrame > spike.timeLastFrame) {
            spike.node = entry;
            spike.timeLastFrame = timeLastFrame;
        }
    }
}

void VisualProfiler::collectProfilerNodesSpikeRecursive(const ProfilerNode *node, int depth,
                                                        std::vector<SPIKE> &spikeNodes) {
    if(node == NULL) return;

    // recursive call
    ProfilerNode *child = node->getChild();
    while(child != NULL) {
        collectProfilerNodesSpikeRecursive(child, depth + 1, spikeNodes);
        child = child->getSibling();
    }

    // add spike node (ignore root 0)
    if(depth > 0) {
        SPIKE spike;

        spike.node.node = node;
        spike.node.depth = depth;
        spike.timeLastFrame = node->getTimeLastFrame();

        spikeNodes.push_back(spike);
    }
}

int VisualProfiler::getGraphWidth() { return (cv_vprof_graph_width.getFloat() * env->getDPIScale()); }

int VisualProfiler::getGraphHeight() { return (cv_vprof_graph_height.getFloat() * env->getDPIScale()); }

void VisualProfiler::addTextLine(const UString &text, McFont *font, std::vector<TEXT_LINE> &textLines) {
    TEXT_LINE textLine;
    {
        textLine.text = text;
        textLine.width = font->getStringWidth(text);
    }
    textLines.push_back(textLine);
}
