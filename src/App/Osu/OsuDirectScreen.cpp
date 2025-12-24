#include "OsuDirectScreen.h"

#include "AnimationHandler.h"
#include "BackgroundImageHandler.h"
#include "BanchoApi.h"
#include "Bancho.h"
#include "BeatmapInterface.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "CBaseUITextbox.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "Font.h"
#include "Graphics.h"
#include "Logging.h"
#include "MainMenu.h"
#include "Mouse.h"
#include "NetworkHandler.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "OsuConVars.h"
#include "SongBrowser/SongBrowser.h"
#include "SString.h"
#include "UIButton.h"

#include <charconv>

class OnlineMapListing : public CBaseUIContainer {
   public:
    OnlineMapListing(OsuDirectScreen* parent, Downloader::BeatmapSetMetadata meta);

    void draw() override;

    // NOT inherited, called manually
    void onResolutionChange(vec2 newResolution);

    // Overriding click detection because buttons don't work well in scrollviews
    // Our custom behavior is "if clicked and cursor moved less than 5px"
    void onMouseDownInside(bool left = true, bool right = false) override;
    void onMouseUpInside(bool left = true, bool right = false) override;
    void onMouseUpOutside(bool left = true, bool right = false) override;
    void onMouseInside() override;
    void onMouseOutside() override;

   private:
    OsuDirectScreen* directScreen;
    McFont* font;

    Downloader::BeatmapSetMetadata meta;

    // Cache
    std::string full_title;
    f32 creator_width{0.f};

    f32 hover_anim{0.f};
    f32 click_anim{0.f};

    vec2 mousedown_coords{-999.f, -999.f};

    bool installed;
    bool downloading{false};
    bool download_failed{false};
};

OnlineMapListing::OnlineMapListing(OsuDirectScreen* parent, Downloader::BeatmapSetMetadata meta)
    : directScreen(parent), font(engine->getDefaultFont()), meta(std::move(meta)) {
    this->installed = db->getBeatmapSet(this->meta.set_id) != nullptr;
    this->onResolutionChange(osu->getVirtScreenSize());
}

void OnlineMapListing::onMouseDownInside(bool /*left*/, bool /*right*/) { this->mousedown_coords = mouse->getPos(); }

void OnlineMapListing::onMouseUpInside(bool /*left*/, bool /*right*/) {
    const f32 distance = vec::distance(mouse->getPos(), this->mousedown_coords);
    if(distance < 5.f) {
        this->click_anim = 1.f;
        anim::moveQuadInOut(&this->click_anim, 0.0f, 0.15f, 0.0f, true);

        if(this->installed) {
            // Select map, or go to song browser if already selected
            if(osu->getMapInterface()->getBeatmap()->getSetID() == this->meta.set_id) {
                this->setVisible(false);
                osu->toggleSongBrowser();
            } else {
                const auto set = db->getBeatmapSet(this->meta.set_id);
                if(!set) return;  // probably unreachable
                const auto& diffs = set->getDifficulties();
                if(diffs.empty()) return;  // surely unreachable
                osu->getSongBrowser()->onDifficultySelected(diffs[0].get(), false);
            }
        } else {
            this->downloading = !this->downloading;
            if(this->downloading) {
                this->directScreen->auto_select_set = this->meta.set_id;
            }
        }
    }

    this->mousedown_coords = {-999.f, -999.f};
}

void OnlineMapListing::onMouseUpOutside(bool /*left*/, bool /*right*/) { this->mousedown_coords = {-999.f, -999.f}; }

void OnlineMapListing::onMouseInside() { anim::moveQuadInOut(&this->hover_anim, 0.25f, 0.15f, 0.0f, true); }
void OnlineMapListing::onMouseOutside() { anim::moveQuadInOut(&this->hover_anim, 0.f, 0.15f, 0.0f, true); }

void OnlineMapListing::onResolutionChange(vec2 /*newResolution*/) {
    this->full_title = fmt::format("{} - {}", this->meta.artist, this->meta.title);

    this->creator_width = this->font->getStringWidth(this->meta.creator);
}

void OnlineMapListing::draw() {
    // XXX: laggy/slow

    f32 download_progress = 0.f;
    if(this->downloading) {
        // TODO: downloads will not finish if player leaves this screen
        Downloader::download_beatmapset(this->meta.set_id, &download_progress);
        if(download_progress == -1.f) {
            // TODO: display error toast
            this->downloading = false;
            this->download_failed = true;
            download_progress = 0.f;
        } else if(download_progress < 1.f) {
            // To show we're downloading, always draw at least 5%
            download_progress = std::max(0.05f, download_progress);
        } else {
            this->downloading = false;
            download_progress = 0.f;

            std::string mapset_path = fmt::format(NEOSU_MAPS_PATH "/{}/", this->meta.set_id);
            const auto set = db->addBeatmapSet(mapset_path, this->meta.set_id);
            if(set) {
                this->installed = true;

                if(this->directScreen->auto_select_set == this->meta.set_id) {
                    const auto& diffs = set->getDifficulties();
                    if(diffs.empty()) return;  // surely unreachable
                    osu->getSongBrowser()->onDifficultySelected(diffs[0].get(), false);
                }
            } else {
                // TODO: Sometimes, download fails but returns progress == 1.f!
                this->download_failed = true;
            }
        }
    }

    const f32 padding = 5.f;
    const f32 x = this->getPos().x;
    const f32 y = this->getPos().y;
    const f32 width = this->getSize().x;
    const f32 height = this->getSize().y;

    const f32 alpha = std::min(0.25f + this->hover_anim + this->click_anim, 1.f);

    // Download progress
    const f32 download_width = width * download_progress;
    g->setColor(rgb(100, 255, 100));
    g->setAlpha(alpha);
    g->fillRect(x, y, download_width, height);

    // Background
    Color color = rgb(255, 255, 255);
    if(this->installed)
        color = rgb(0, 255, 0);
    else if(this->download_failed)
        color = rgb(255, 0, 0);
    g->setColor(color);
    g->setAlpha(alpha);
    g->fillRect(x + download_width, y, width - download_width, height);

    g->pushClipRect(McRect(x, y, width, height));
    g->pushTransform();
    {
        g->translate(x, y);

        // TODO: mapset background image
        // Requires to save them in a cache folder; while we're at it,
        // we should have a global cache folder for avatars/server_icons/map_bgs so we can
        // respect XDG cache directory on linux instead of using ./cache (or currently ./avatars)

        g->setColor(0xffffffff);
        g->translate(padding, padding + this->font->getHeight());
        this->font->drawString(this->full_title);

        g->pushTransform();
        {
            g->translate(width - (this->creator_width + 2 * padding), 0);
            this->font->drawString(this->meta.creator);
        }
        g->popTransform();

        // TODO: map difficulties (with their own hover text)
    }
    g->popTransform();
    g->popClipRect();
}

OsuDirectScreen::OsuDirectScreen() {
    this->title = new CBaseUILabel(0, 0, 0, 0, "", "Online Beatmaps");
    this->title->setDrawFrame(false);
    this->title->setDrawBackground(false);
    this->addBaseUIElement(this->title);

    this->search_bar = new CBaseUITextbox();
    this->search_bar->setBackgroundColor(0xaa000000);
    this->addBaseUIElement(this->search_bar);

    this->newest_btn = new UIButton(0, 0, 0, 0, "", "Newest maps");
    this->newest_btn->setColor(0xff88FF00);
    this->newest_btn->setClickCallback([this]() {
        this->reset();
        this->search("Newest", 0);
    });
    this->addBaseUIElement(this->newest_btn);

    this->best_rated_btn = new UIButton(0, 0, 0, 0, "", "Best maps");
    this->best_rated_btn->setColor(0xffFF006A);
    this->best_rated_btn->setClickCallback([this]() {
        this->reset();
        this->search("Top Rated", 0);
    });
    this->addBaseUIElement(this->best_rated_btn);

    this->results = new CBaseUIScrollView();
    this->results->setBackgroundColor(0xaa000000);
    this->results->setHorizontalScrolling(false);
    this->results->setVerticalScrolling(true);
    this->results->grabs_clicks = false;
    this->addBaseUIElement(this->results);

    this->onResolutionChange(osu->getVirtScreenSize());
}

CBaseUIContainer* OsuDirectScreen::setVisible(bool visible) {
    if(visible) {
        if(!db->isFinished() || db->isCancelled()) {
            // Ensure database is loaded (same as Lobby screen)
            osu->getSongBrowser()->refreshBeatmaps(true);
        }
    }

    ScreenBackable::setVisible(visible);

    if(visible) {
        this->search_bar->clear();
        this->newest_btn->click();
        this->search_bar->focus();
    }

    return this;
}

bool OsuDirectScreen::isVisible() { return this->bVisible && !osu->getSongBrowser()->isVisible(); }

void OsuDirectScreen::draw() {
    if(!this->isVisible()) return;

    osu->getBackgroundImageHandler()->draw(osu->getMapInterface()->getBeatmap());
    ScreenBackable::draw();

    // TODO: loading indicator while stuff is loading
    // TODO: message if no maps were found or server errored
}

void OsuDirectScreen::mouse_update(bool* propagate_clicks) {
    if(!this->isVisible()) return;
    if(!BanchoState::is_online() || !db->isFinished() || db->isCancelled()) return this->onBack();
    ScreenBackable::mouse_update(propagate_clicks);

    if(this->search_bar->hitEnter()) {
        if(this->current_query == this->search_bar->getText().utf8View() && this->current_page == -1) {
            // We're already searching for the current query, don't cancel the request
            // HACK: (this->current_page == -1) not cleanest way to detect if we're in the middle of a request
            return;
        }

        this->reset();
        this->search(this->search_bar->getText().utf8View(), 0);
        return;
    }

    // Fetch next results page once we reached the bottom
    if(this->results->isAtBottom() && this->last_search_time + 1.0 < engine->getTime()) {
        static uSz pagination_request_id = 0;
        if(pagination_request_id == this->request_id) {
            // We're already requesting the next page
            // HACK: not cleanest way to detect if we're in the middle of a request
            return;
        }

        this->search(this->current_query, this->current_page + 1);
        pagination_request_id = this->request_id;
    }
}

void OsuDirectScreen::onBack() {
    this->setVisible(false);
    osu->getMainMenu()->setVisible(true);
}

void OsuDirectScreen::onResolutionChange(vec2 newResolution) {
    this->setSize(osu->getVirtScreenSize());  // HACK: don't forget this or else nothing works!
    ScreenBackable::onResolutionChange(newResolution);

    const f32 scale = osu->getUIScale();
    f32 x = 50.f;
    f32 y = 30.f;

    // Screen title
    this->title->setFont(osu->getTitleFont());
    this->title->setSizeToContent(0, 0);
    this->title->setRelPos(x, y);
    y += this->title->getSize().y;

    const f32 results_width = std::min(newResolution.x - 10.f * scale, 1000.f * scale);
    const f32 x_start = osu->getVirtScreenWidth() / 2.f - results_width / 2.f;
    x = x_start;
    y += 50.f * scale;

    // Search bar & buttons
    this->search_bar->setRelPos(x, y);
    this->search_bar->setSize(400.0 * scale, 40.0 * scale);
    x += this->search_bar->getSize().x;
    const f32 BUTTONS_MARGIN = 10.f * scale;
    x += BUTTONS_MARGIN;
    this->newest_btn->setRelPos(x, y);
    this->newest_btn->setSize(150.f * scale, this->search_bar->getSize().y);
    x += this->newest_btn->getSize().x + BUTTONS_MARGIN;
    this->best_rated_btn->setRelPos(x, y);
    this->best_rated_btn->setSize(150.f * scale, this->search_bar->getSize().y);
    y += this->search_bar->getSize().y;

    // Results list
    x = x_start;
    y += 10.f * scale;
    this->results->setRelPos(x, y);
    this->results->setSize(results_width, newResolution.y - (y + 100.f * scale));
    {
        const f32 LISTING_MARGIN = 10.f * scale;

        f32 y = LISTING_MARGIN;
        for(auto* listing :
            reinterpret_cast<const std::vector<OnlineMapListing*>&>(this->results->container->getElements())) {
            listing->setRelPos(LISTING_MARGIN, y);
            listing->setSize(results_width - 2 * LISTING_MARGIN, 75.f * scale);
            y += listing->getSize().y + LISTING_MARGIN;

            // Update font stuff
            listing->onResolutionChange(newResolution);
        }
        this->results->setScrollSizeToContent();
        this->results->container->update_pos();  // sigh...
    }

    this->update_pos();
}

void OsuDirectScreen::reset() {
    // "Cancel" current request and immediately allow a new one
    this->request_id++;

    // Clear search results
    this->results->freeElements();
    this->current_page = -1;

    // De-focus search bar (since we only reset() on user action)
    this->search_bar->stealFocus();
}

void OsuDirectScreen::search(std::string_view query, i32 page) {
    assert(page >= 0);

    const i32 filter = RankingStatusFilter::ALL;
    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    std::string url = fmt::format("{}osu.{}/web/osu-search.php?m=0&r={}&q={}&p={}", scheme, BanchoState::endpoint,
                                  filter, NeoNet::urlEncode(query), page);
    BANCHO::Api::append_auth_params(url);

    NeoNet::RequestOptions options;
    options.timeout = 5;
    options.connect_timeout = 5;
    options.follow_redirects = true;
    options.user_agent = "osu!";

    debugLog("Searching for maps matching \"{}\" (page {})", query, page);
    const auto current_request_id = ++this->request_id;
    this->current_query = query;
    this->last_search_time = engine->getTime();

    networkHandler->httpRequestAsync(
        url,
        [func = __FUNCTION__, current_request_id, page, this](const NeoNet::Response& response) {
            if(current_request_id != this->request_id) {
                // Request was "cancelled"
                return;
            }

            if(response.success) {
                auto set_lines = SString::split(response.body, '\n');
                if(!set_lines[0].empty() && set_lines[0].back() == '\r') {
                    set_lines[0].remove_suffix(1);  // remove CR if it somehow had one
                }

                i32 nb_results{0};
                auto [ptr, ec] =
                    std::from_chars(set_lines[0].data(), set_lines[0].data() + set_lines[0].size(), nb_results);

                if(nb_results <= 0 || ec != std::errc()) {
                    // HACK: reached end of results (or errored), prevent further requests
                    this->last_search_time = 9999999.9;

                    if(nb_results == -1 && set_lines.size() >= 2) {
                        // Relay server's error message to the player
                        osu->getNotificationOverlay()->addToast(set_lines[1], ERROR_TOAST);
                    }

                    return;
                }

                debugLogLambda("Received {} maps", nb_results);
                for(i32 i = 1; i < set_lines.size(); i++) {
                    const auto meta = Downloader::parse_beatmapset_metadata(set_lines[i]);
                    if(meta.set_id == 0) continue;

                    this->results->container->addBaseUIElement(new OnlineMapListing(this, meta));
                }

                this->current_page = page;
                this->onResolutionChange(osu->getVirtScreenSize());
            } else {
                // TODO: handle failure
            }
        },
        options);
}
