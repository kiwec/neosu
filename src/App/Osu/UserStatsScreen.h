#pragma once
#include "ScreenBackable.h"

class ConVar;
class CBaseUIContainer;
class CBaseUIScrollView;
class UIContextMenu;
class UserCard;
class ScoreButton;

class UserStatsScreen : public ScreenBackable {
   public:
    UserStatsScreen();
    ~UserStatsScreen() override = default;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    CBaseUIContainer *setVisible(bool visible) override;

    void rebuildScoreButtons();

   private:
    void onBack() override;
    void updateLayout() override;

    void onScoreClicked(CBaseUIButton *button);
    void onMenuClicked(CBaseUIButton *button);
    void onMenuSelected(UString text, int id);
    void onCopyAllScoresClicked();
    void onCopyAllScoresUserSelected(UString text, int id);
    void onCopyAllScoresConfirmed(UString text, int id);
    void onDeleteAllScoresClicked();
    void onDeleteAllScoresConfirmed(UString text, int id);

    UserCard *m_userCard = nullptr;
    UIContextMenu *m_contextMenu = nullptr;

    CBaseUIScrollView *m_scores = nullptr;
    std::vector<ScoreButton *> m_scoreButtons;
};
