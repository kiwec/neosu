//================ Copyright (c) 2020, PG, All rights reserved. =================//
//
// Purpose:		vprof gui overlay
//
// $NoKeywords: $vprof
//===============================================================================//

#ifndef VISUALPROFILER_H
#define VISUALPROFILER_H

#include "CBaseUIElement.h"

class ConVar;
class ProfilerNode;
class ProfilerProfile;

class McFont;
class VertexArrayObject;

class VisualProfiler : public CBaseUIElement {
   public:
    VisualProfiler();
    ~VisualProfiler() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void incrementInfoBladeDisplayMode();
    void decrementInfoBladeDisplayMode();

    void addInfoBladeAppTextLine(const UString &text);

    void setProfile(ProfilerProfile *profile);
    void setRequiresAltShiftKeysToFreeze(bool requiresAltShiftKeysToFreeze) {
        this->bRequiresAltShiftKeysToFreeze = requiresAltShiftKeysToFreeze;
    }

    bool isEnabled() override;

   private:
    enum INFO_BLADE_DISPLAY_MODE {
        INFO_BLADE_DISPLAY_MODE_DEFAULT = 0,

        INFO_BLADE_DISPLAY_MODE_GPU_INFO = 1,
        INFO_BLADE_DISPLAY_MODE_ENGINE_INFO = 2,
        INFO_BLADE_DISPLAY_MODE_APP_INFO = 3,

        INFO_BLADE_DISPLAY_MODE_COUNT = 4
    };

    struct TEXT_LINE {
        UString text;
        int width;
    };

   private:
    struct NODE {
        const ProfilerNode *node;
        int depth;
    };

    struct SPIKE {
        NODE node;
        double timeLastFrame;
        u32 id;
    };

    struct GROUP {
        const char *name;
        int id;
        Color color;
    };

   private:
    static void collectProfilerNodesRecursive(const ProfilerNode *node, int depth, std::vector<NODE> &nodes,
                                              SPIKE &spike);
    static void collectProfilerNodesSpikeRecursive(const ProfilerNode *node, int depth, std::vector<SPIKE> &spikeNodes);

    static int getGraphWidth();
    static int getGraphHeight();

    static void addTextLine(const UString &text, McFont *font, std::vector<TEXT_LINE> &textLines);

    static void drawStringWithShadow(McFont *font, const UString &string, Color color);

    int iPrevVaoWidth;
    int iPrevVaoHeight;
    int iPrevVaoGroups;
    float fPrevVaoMaxRange;
    float fPrevVaoAlpha;

    int iCurLinePos;

    int iDrawGroupID;
    int iDrawSwapBuffersGroupID;

    ProfilerProfile *profile;
    std::vector<GROUP> groups;
    std::vector<NODE> nodes;
    std::vector<SPIKE> spikes;

    SPIKE spike;
    std::vector<SPIKE> spikeNodes;
    u32 spikeIDCounter;

    McFont *font;
    McFont *fontConsole;
    VertexArrayObject *lineVao;

    bool bScheduledForceRebuildLineVao;
    bool bRequiresAltShiftKeysToFreeze;

    std::vector<TEXT_LINE> textLines;
    std::vector<UString> appTextLines;
};

extern VisualProfiler *vprof;

#endif
