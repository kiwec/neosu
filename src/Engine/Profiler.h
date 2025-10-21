#pragma once
// Copyright (c) 2020, PG, All rights reserved.
#define VPROF_MAIN()                 \
    g_profCurrentProfile.mainprof(); \
    VPROF("Main")

#define VPROF(name) VPROF_(name, VPROF_BUDGETGROUP_ROOT)
#define VPROF_(name, group) ProfilerScope Prof_(name, group);

#define VPROF_BUDGET(name, group) VPROF_(name, group)

#define VPROF_SCOPE_BEGIN(name) \
    do {                        \
    VPROF(name)
#define VPROF_SCOPE_END() \
    }                     \
    while(0)

#define VPROF_ENTER_SCOPE(name) g_profCurrentProfile.enterScope(name, VPROF_BUDGETGROUP_ROOT)
#define VPROF_EXIT_SCOPE() g_profCurrentProfile.exitScope()

#define VPROF_BUDGETGROUP_ROOT "Root"
#define VPROF_BUDGETGROUP_SLEEP "Sleep"
#define VPROF_BUDGETGROUP_EVENTS "Events"
#define VPROF_BUDGETGROUP_UPDATE "Update"
#define VPROF_BUDGETGROUP_DRAW "Draw"
#define VPROF_BUDGETGROUP_DRAW_SWAPBUFFERS "SwapBuffers"

#define DETAILED_PROFILING

#ifdef DETAILED_PROFILING
#include <execution>
#include "fmt/ranges.h"

#define DBGTIME(__amt__, ...)                                                        \
    do {                                                                             \
        static_assert((uint32_t)__amt__ > 0 && (uint32_t)__amt__ <= 4096);           \
        static std::array<double, (uint32_t)__amt__> lasttms__{};                    \
        static uint32_t lti__ = 0;                                                   \
        static double overall_max__ = 0.0;                                           \
        const double before__ = Timing::getTimeReal();                               \
        do {                                                                         \
            __VA_ARGS__;                                                             \
        } while(false);                                                              \
        const double after__ = Timing::getTimeReal();                                \
        lasttms__[lti__ % (uint32_t)__amt__] = after__ - before__;                   \
        if(!(++lti__ % (uint32_t)__amt__)) {                                         \
            lti__ = 0;                                                               \
            auto current_max__ = std::ranges::max(lasttms__);                        \
            if(current_max__ > overall_max__) overall_max__ = current_max__;         \
            Logger::logRaw("\n\tTIME FOR: " #__VA_ARGS__ "\n\tmax overall: {:.8f}" \
			         "\n\taverage: {:.4f} min: {:.4f} max: {:.4f}" \
			         "\n\tpast " MC_STRINGIZE(__amt__) " times:" \
			                                           "\n\t[ {:.4f} ]", \
			         overall_max__, std::reduce(std::execution::unseq, lasttms__.begin(), lasttms__.end(), 0.0) / ((uint32_t)__amt__), std::ranges::min(lasttms__), \
			         current_max__, fmt::join(lasttms__, ", ")); \
        }                                                                            \
    } while(false);
#define VPROF_MAX_NUM_BUDGETGROUPS 128
#define VPROF_MAX_NUM_NODES 128
#define VPROF_BUDGET_DBG VPROF_BUDGET
#else
#define DBGTIME(...)
#define VPROF_MAX_NUM_BUDGETGROUPS 32
#define VPROF_MAX_NUM_NODES 32
#define VPROF_BUDGET_DBG(...)
#endif

namespace Profiling {
void vprofToggleCB(float newValue);
}

class ProfilerNode {
    friend class ProfilerProfile;

   public:
    ProfilerNode();
    ProfilerNode(const char *name, const char *group, ProfilerNode *parent);

    void enterScope();
    bool exitScope();

    [[nodiscard]] inline const char *getName() const { return this->name; }
    [[nodiscard]] inline int getGroupID() const { return this->iGroupID; }

    [[nodiscard]] inline ProfilerNode *getParent() const { return this->parent; }
    [[nodiscard]] inline ProfilerNode *getChild() const { return this->child; }
    [[nodiscard]] inline ProfilerNode *getSibling() const { return this->sibling; }

    [[nodiscard]] inline double getTimeCurrentFrame() const {
        return this->fTimeCurrentFrame;
    }  // NOTE: this is incomplete if retrieved within engine update(), use getTimeLastFrame() instead
    [[nodiscard]] inline double getTimeLastFrame() const { return this->fTimeLastFrame; }

   private:
    inline void constructor(const char *name, const char *group, ProfilerNode *parent);

    ProfilerNode *getSubNode(const char *name, const char *group);

    static int s_iNodeCounter;

    const char *name;
    int iGroupID;

    ProfilerNode *parent;
    ProfilerNode *child;
    ProfilerNode *sibling;

    int iNumRecursions;
    double fTime;
    double fTimeCurrentFrame;
    double fTimeLastFrame;
};

class ProfilerProfile {
    friend class ProfilerNode;

   public:
    ProfilerProfile(bool manualStartViaMain = false);

    inline void mainprof() {
        if(this->bEnableScheduled) {
            this->bEnableScheduled = false;
            this->root.enterScope();
        }

        // collect all durations from the last frame and store them as a complete set
        if(this->iEnabled > 0) {
            for(int i = 0; i < this->iNumNodes; i++) {
                this->nodes[i].fTimeLastFrame = this->nodes[i].fTimeCurrentFrame;
            }
        }
    }

    inline void start() {
        if(++this->iEnabled == 1) {
            if(this->bManualStartViaMain)
                this->bEnableScheduled = true;
            else
                this->root.enterScope();
        }
    }

    inline void stop() {
        if(--this->iEnabled == 0) {
            if(!this->bEnableScheduled) this->root.exitScope();

            this->bEnableScheduled = false;
        }
    }

    inline void enterScope(const char *name, const char *group) {
        if((this->iEnabled != 0 && !this->bEnableScheduled) || !this->bAtRoot) {
            if(name != this->curNode->name)  // NOTE: pointer comparison
                this->curNode = this->curNode->getSubNode(name, group);

            this->curNode->enterScope();

            this->bAtRoot = (this->curNode == &this->root);
        }
    }

    inline void exitScope() {
        if(!this->bAtRoot || (this->iEnabled != 0 && !this->bEnableScheduled)) {
            if(!this->bAtRoot && this->curNode->exitScope()) this->curNode = this->curNode->parent;

            this->bAtRoot = (this->curNode == &this->root);
        }
    }

    [[nodiscard]] inline bool isEnabled() const { return (this->iEnabled != 0 || this->bEnableScheduled); }
    [[nodiscard]] inline bool isAtRoot() const { return this->bAtRoot; }

    [[nodiscard]] inline int getNumGroups() const { return this->iNumGroups; }
    [[nodiscard]] inline int getNumNodes() const { return this->iNumNodes; }

    [[nodiscard]] inline const ProfilerNode *getRoot() const { return &this->root; }

    inline const char *getGroupName(int groupID) {
        return this->groups[groupID < 0 ? 0 : (groupID > this->iNumGroups - 1 ? this->iNumGroups - 1 : groupID)].name;
    }

    double sumTimes(int groupID);
    double sumTimes(ProfilerNode *node, int groupID);

   private:
    struct BUDGETGROUP {
        const char *name;
    };

    int groupNameToID(const char *group);

    int iNumGroups;
    BUDGETGROUP groups[VPROF_MAX_NUM_BUDGETGROUPS]{};

    bool bManualStartViaMain;

    int iEnabled;
    bool bEnableScheduled;
    bool bAtRoot;
    ProfilerNode root;
    ProfilerNode *curNode;

    int iNumNodes;
    ProfilerNode nodes[VPROF_MAX_NUM_NODES];
};

extern ProfilerProfile g_profCurrentProfile;

class ProfilerScope {
   public:
    inline ProfilerScope(const char *name, const char *group) { g_profCurrentProfile.enterScope(name, group); }
    inline ~ProfilerScope() { g_profCurrentProfile.exitScope(); }
};

extern ProfilerProfile g_profCurrentProfile;
