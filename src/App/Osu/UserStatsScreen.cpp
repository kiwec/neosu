// Copyright (c) 2019, PG, All rights reserved.
#include "UserStatsScreen.h"

#include "Bancho.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "HUD.h"
#include "Icons.h"
#include "Keyboard.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Replay.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SongBrowser/ScoreButton.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "TooltipOverlay.h"
#include "UIContextMenu.h"
#include "UserCard.h"

UserStatsScreen::UserStatsScreen() : ScreenBackable() {
    this->m_userCard = new UserCard(0);
    this->addBaseUIElement(this->m_userCard);

    this->m_contextMenu = std::make_unique<UIContextMenu>();
    this->m_contextMenu->setVisible(true);

    this->m_scores = new CBaseUIScrollView();
    this->m_scores->setBackgroundColor(0xff222222);
    this->m_scores->setHorizontalScrolling(false);
    this->m_scores->setVerticalScrolling(true);
    this->addBaseUIElement(m_scores);
}

void UserStatsScreen::draw() {
    if(!this->isVisible()) return;

    auto screen = osu->getScreenSize();
    g->setColor(rgb(0, 0, 0));
    g->fillRect(0, 0, screen.x, screen.y);

    ScreenBackable::draw();
    m_contextMenu->draw();
}

void UserStatsScreen::mouse_update(bool *propagate_clicks) {
    if(!this->isVisible()) return;

    m_contextMenu->mouse_update(propagate_clicks);
    ScreenBackable::mouse_update(propagate_clicks);

    // prevent clicks from affecting lower screens (sorry if this fucked you over)
    *propagate_clicks = false;
}

CBaseUIContainer *UserStatsScreen::setVisible(bool visible) {
    if(visible == this->isVisible()) return this;

    ScreenBackable::setVisible(visible);
    osu->toggleSongBrowser();

    if(this->isVisible()) {
        rebuildScoreButtons();
    } else {
        m_contextMenu->setVisible2(false);
    }

    return this;
}

void UserStatsScreen::onBack() { setVisible(false); }

void UserStatsScreen::rebuildScoreButtons() {
    // hard reset (delete)
    m_scores->getContainer()->freeElements();
    m_scoreButtons.clear();

    this->m_userCard->setID(bancho->user_id);
    this->m_userCard->updateUserStats();

    i32 i = 0;
    std::vector<FinishedScore *> scores = db->getPlayerPPScores(cv::name.getString()).ppScores;
    for(auto &score : scores | std::views::reverse) {
        if(i >= cv::ui_top_ranks_max.getInt()) break;
        const float weight = Database::getWeightForIndex(i);

        DatabaseBeatmap *diff = db->getBeatmapDifficulty(score->beatmap_hash);
        if(!diff) continue;

        UString title = "...";
        if(diff != nullptr) {
            title = diff->getArtist().c_str();
            title.append(" - ");
            title.append(diff->getTitle().c_str());
            title.append(" [");
            title.append(diff->getDifficultyName().c_str());
            title.append("]");
        }

        auto *button = new ScoreButton(this->m_contextMenu.get(), 0, 0, 300, 100, ScoreButton::STYLE::TOP_RANKS);
        button->map_hash = score->beatmap_hash;
        button->setScore(*score, diff, ++i, title, weight);
        button->setClickCallback([](CBaseUIButton *button) {
            auto score = ((ScoreButton *)button)->getScore();
            auto song_button = (CarouselButton *)osu->getSongBrowser()->hashToSongButton[score.beatmap_hash];
            osu->userStats->setVisible(false);
            osu->getSongBrowser()->selectSongButton(song_button);
            osu->getSongBrowser()->highlightScore(score.unixTimestamp);
        });

        m_scoreButtons.push_back(button);
        m_scores->getContainer()->addBaseUIElement(button);
    }

    updateLayout();
}

void UserStatsScreen::updateLayout() {
    ScreenBackable::updateLayout();

    const float dpiScale = Osu::getUIScale();

    setSize(osu->getScreenSize());

    const int scoreListHeight = osu->getScreenHeight() * 0.8f;
    m_scores->setSize(osu->getScreenWidth() * 0.6f, scoreListHeight);
    m_scores->setPos(osu->getScreenWidth() / 2 - m_scores->getSize().x / 2, osu->getScreenHeight() - scoreListHeight);

    const int margin = 5 * dpiScale;
    const int padding = 5 * dpiScale;

    // NOTE: these can't really be uiScale'd, because of the fixed aspect ratio
    const int scoreButtonWidth = m_scores->getSize().x * 0.92f - 2 * margin;
    const int scoreButtonHeight = scoreButtonWidth * 0.065f;
    for(int i = 0; i < m_scoreButtons.size(); i++) {
        m_scoreButtons[i]->setSize(scoreButtonWidth - 2, scoreButtonHeight);
        m_scoreButtons[i]->setRelPos(margin, margin + i * (scoreButtonHeight + padding));
    }
    m_scores->getContainer()->update_pos();

    m_scores->setScrollSizeToContent();

    const int userButtonHeight = m_scores->getPos().y * 0.6f;
    this->m_userCard->setSize(userButtonHeight * 3.5f, userButtonHeight);
    this->m_userCard->setPos(osu->getScreenWidth() / 2 - this->m_userCard->getSize().x / 2,
                             m_scores->getPos().y / 2 - this->m_userCard->getSize().y / 2);
}
