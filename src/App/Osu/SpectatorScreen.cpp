#include "SpectatorScreen.h"

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "BanchoUsers.h"
#include "Beatmap.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "Changelog.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Lobby.h"
#include "MainMenu.h"
#include "ModSelector.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "PromptScreen.h"
#include "RankingScreen.h"
#include "ResourceManager.h"
#include "RoomScreen.h"
#include "Skin.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "UIButton.h"
#include "UserCard.h"

static i32 current_map_id = 0;

// TODO @kiwec: test that those bugs have been fixed

// TODO @kiwec: buglist
// - lagspike every second DURING GAMEPLAY!
// - fps dumping to 0 when spectating
//   - during map download
//   - at end of a map?
// - retrying after score doesn't work
// - retrying after death doesn't work
// - teleporting to ranking screen when user is not playing or just started map
// - cursor "times out" randomly during gameplay
// - pause -> retry results in buffering, then chainmiss for like 10 seconds
//   - with buffer 5000, we start 12 seconds behind, and skip the start of the map :(
// - when seeking or initially spectating, all hitobjects before first replay frame should be ignored

#define INIT_LABEL(label_name, default_text, is_big)                      \
    do {                                                                  \
        label_name = new CBaseUILabel(0, 0, 0, 0, "label", default_text); \
        label_name->setFont(is_big ? lfont : font);                       \
        label_name->setSizeToContent(0, 0);                               \
        label_name->setDrawFrame(false);                                  \
        label_name->setDrawBackground(false);                             \
    } while(0)

void start_spectating(i32 user_id) {
    stop_spectating();

    Packet packet;
    packet.id = START_SPECTATING;
    write<i32>(&packet, user_id);
    send_packet(packet);

    auto user_info = get_user_info(user_id, true);
    auto notif = UString::format("Started spectating %s", user_info->name.toUtf8());
    osu->getNotificationOverlay()->addToast(notif, 0xffffdd00);

    bancho.is_spectating = true;
    bancho.spectated_player_id = user_id;
    current_map_id = 0;

    osu->prompt->setVisible(false);
    osu->modSelector->setVisible(false);
    osu->songBrowser2->setVisible(false);
    osu->lobby->setVisible(false);
    osu->changelog->setVisible(false);
    osu->mainMenu->setVisible(false);
    if(osu->room->isVisible()) osu->room->ragequit(false);

    engine->getSound()->play(osu->getSkin()->menuHit);
}

void stop_spectating() {
    if(!bancho.is_spectating) return;

    if(osu->isInPlayMode()) {
        osu->getSelectedBeatmap()->stop(true);
        osu->songBrowser2->bHasSelectedAndIsPlaying = false;
    }

    auto user_info = get_user_info(bancho.spectated_player_id, true);
    auto notif = UString::format("Stopped spectating %s", user_info->name.toUtf8());
    osu->getNotificationOverlay()->addToast(notif, 0xffffdd00);

    bancho.is_spectating = false;
    bancho.spectated_player_id = 0;
    current_map_id = 0;

    Packet packet;
    packet.id = STOP_SPECTATING;
    send_packet(packet);

    osu->mainMenu->setVisible(true);
    engine->getSound()->play(osu->getSkin()->menuBack);
}

SpectatorScreen::SpectatorScreen() {
    this->font = engine->getResourceManager()->getFont("FONT_DEFAULT");
    this->lfont = osu->getSubTitleFont();

    this->pauseButton = new MainMenuPauseButton(0, 0, 0, 0, "pause_btn", "");
    this->pauseButton->setClickCallback(fastdelegate::MakeDelegate(osu->mainMenu, &MainMenu::onPausePressed));
    this->addBaseUIElement(this->pauseButton);

    this->background = new CBaseUIScrollView(0, 0, 0, 0, "spectator_bg");
    this->background->setDrawFrame(true);
    this->background->setDrawBackground(true);
    this->background->setBackgroundColor(0xdd000000);
    this->background->setHorizontalScrolling(false);
    this->background->setVerticalScrolling(false);
    this->addBaseUIElement(this->background);

    INIT_LABEL(this->spectating, "Spectating", true);
    this->background->getContainer()->addBaseUIElement(this->spectating);

    this->userCard = new UserCard(0);
    this->background->getContainer()->addBaseUIElement(this->userCard);

    INIT_LABEL(this->status, "...", false);
    this->background->getContainer()->addBaseUIElement(this->status);

    this->stop_btn = new UIButton(0, 0, 190, 40, "stop_spec_btn", "Stop spectating");
    this->stop_btn->grabs_clicks = true;
    this->stop_btn->setColor(0xff00ff00);
    this->stop_btn->setUseDefaultSkin();
    this->stop_btn->setClickCallback(fastdelegate::MakeDelegate(this, &SpectatorScreen::onStopSpectatingClicked));
    this->addBaseUIElement(this->stop_btn);

    // isVisible() is overridden
    this->bVisible = true;
}

// NOTE: We use this to control client state, even when the spectator screen isn't visible.
void SpectatorScreen::mouse_update(bool *propagate_clicks) {
    if(!bancho.is_spectating) return;

    // Control client state
    // XXX: should use map_md5 instead of map_id
    f32 download_progress = -1.f;
    auto user_info = get_user_info(bancho.spectated_player_id, true);
    if(user_info->map_id == -1 || user_info->map_id == 0) {
        if(osu->isInPlayMode()) {
            osu->getSelectedBeatmap()->stop(true);
            osu->songBrowser2->bHasSelectedAndIsPlaying = false;
        }
    } else if(user_info->mode == STANDARD && user_info->map_id != current_map_id) {
        auto beatmap = download_beatmap(user_info->map_id, user_info->map_md5, &download_progress);
        if(beatmap != NULL) {
            current_map_id = user_info->map_id;
            osu->rankingScreen->setVisible(false);
            osu->songBrowser2->onDifficultySelected(beatmap, false);
            osu->getSelectedBeatmap()->spectate();
        }
    }

    // Update spectator screen UI
    static u32 last_player_id = 0;
    if(bancho.spectated_player_id != last_player_id) {
        this->userCard->setID(bancho.spectated_player_id);
        last_player_id = bancho.spectated_player_id;
    }

    this->spectating->setText(UString::format("Spectating %s", user_info->name.toUtf8()));

    if(user_info->spec_action == LiveReplayBundle::Action::NONE) {
        this->status->setText(UString::format("%s is AFK", user_info->name.toUtf8()));
    } else if(user_info->spec_action == LiveReplayBundle::Action::SONG_SELECT) {
        this->status->setText(UString::format("%s is picking a map...", user_info->name.toUtf8()));
    } else if(user_info->spec_action == LiveReplayBundle::Action::WATCHING_OTHER) {
        this->status->setText(UString::format("%s is spectating someone else", user_info->name.toUtf8()));
    }

    if(user_info->mode != STANDARD) {
        this->status->setText(UString::format("%s is playing minigames", user_info->name.toUtf8()));
    } else if(user_info->map_id != -1 && user_info->map_id != 0) {
        if(user_info->map_id != current_map_id) {
            if(download_progress == -1.f) {
                auto error_str = UString::format("Failed to download Beatmap #%d :(", user_info->map_id);
                this->status->setText(error_str);

                static u32 last_failed_map = 0;
                if(user_info->map_id != last_failed_map) {
                    Packet packet;
                    packet.id = CANT_SPECTATE;
                    send_packet(packet);

                    last_failed_map = user_info->map_id;
                }
            } else {
                auto text = UString::format("Downloading map... %.2f%%", download_progress * 100.f);
                this->status->setText(text);
            }
        }
    }

    const float dpiScale = Osu::getUIScale();
    auto resolution = osu->getScreenSize();
    this->setPos(0, 0);
    this->setSize(resolution);

    this->pauseButton->setSize(30 * dpiScale, 30 * dpiScale);
    this->pauseButton->setPos(resolution.x - this->pauseButton->getSize().x * 2 - 10 * dpiScale,
                              this->pauseButton->getSize().y + 10 * dpiScale);
    this->pauseButton->setPaused(!osu->getSelectedBeatmap()->isPreviewMusicPlaying());

    this->background->setSize(resolution.x * 0.6, resolution.y * 0.6 - 110 * dpiScale);
    auto bgsize = this->background->getSize();
    this->background->setPos(resolution.x / 2.0 - bgsize.x / 2.0, resolution.y / 2.0 - bgsize.y / 2.0);

    {
        auto screen = engine->getScreenSize();
        bool is_widescreen = ((i32)(std::max(0, (i32)((screen.x - (screen.y * 4.f / 3.f)) / 2.f))) > 0);
        auto global_scale = is_widescreen ? (screen.x / 1366.f) : 1.f;

        this->spectating->setSizeToContent();
        this->spectating->setRelPos(bgsize.x / 2.f - this->spectating->getSize().x / 2.f,
                                    bgsize.y / 2.f - 100 * dpiScale);

        this->userCard->setSize(global_scale * 640 * 0.5, global_scale * 150 * 0.5);
        auto cardsize = this->userCard->getSize();
        this->userCard->setRelPos(bgsize.x / 2.f - cardsize.x / 2.f, bgsize.y / 2.f - cardsize.y / 2.f);

        this->status->setTextJustification(CBaseUILabel::TEXT_JUSTIFICATION_CENTERED);
        this->status->setRelPos(bgsize.x / 2.f, bgsize.y / 2.f + 100 * dpiScale);
    }
    this->background->setScrollSizeToContent();

    auto stop_pos = this->background->getPos();
    stop_pos.x += bgsize.x / 2.f - this->stop_btn->getSize().x / 2.f;
    stop_pos.y += bgsize.y + 20 * dpiScale;
    this->stop_btn->setPos(stop_pos);

    // Handle spectator screen UI input
    if(this->isVisible()) {
        OsuScreen::mouse_update(propagate_clicks);
    }
}

void SpectatorScreen::draw(Graphics *g) {
    if(!this->isVisible()) return;

    if(cv_draw_spectator_background_image.getBool()) {
        SongBrowser::drawSelectedBeatmapBackgroundImage(g, 1.0);
    }

    OsuScreen::draw(g);
}

bool SpectatorScreen::isVisible() { return bancho.is_spectating && !osu->isInPlayMode(); }

CBaseUIElement *SpectatorScreen::setVisible(bool visible) {
    engine->showMessageError("Programmer Error", "Idiot tried to control spectator screen visibility");
    abort();
    return this;
}

void SpectatorScreen::onKeyDown(KeyboardEvent &key) {
    if(!this->isVisible()) return;

    if(key.getKeyCode() == KEY_ESCAPE) {
        key.consume();
        this->onStopSpectatingClicked();
        return;
    }

    OsuScreen::onKeyDown(key);
}

void SpectatorScreen::onStopSpectatingClicked() { stop_spectating(); }
