#include "OsuDirectScreen.h"

#include "BanchoApi.h"
#include "Bancho.h"
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
    OnlineMapListing(Downloader::BeatmapSetMetadata meta);

    void draw() override;

    // Overriding click detection because buttons don't work well in scrollviews
    // Our custom behavior is "if clicked and cursor moved less than 5px"
    void onMouseDownInside(bool left = true, bool right = false) override;
    void onMouseUpInside(bool left = true, bool right = false) override;
    void onMouseUpOutside(bool left = true, bool right = false) override;

   private:
    bool installed;
    bool downloading{false};
    Downloader::BeatmapSetMetadata meta;
    vec2 mousedown_coords{-999.f, -999.f};
};

OnlineMapListing::OnlineMapListing(Downloader::BeatmapSetMetadata meta) : meta(std::move(meta)) {
    this->installed = db->getBeatmapSet(this->meta.set_id) != nullptr;
}

void OnlineMapListing::onMouseDownInside(bool /*left*/, bool /*right*/) { this->mousedown_coords = mouse->getPos(); }

void OnlineMapListing::onMouseUpInside(bool /*left*/, bool /*right*/) {
    const f32 distance = vec::distance(mouse->getPos(), this->mousedown_coords);
    if(distance < 5.f && !this->installed) {
        this->downloading = !this->downloading;
    }

    this->mousedown_coords = {-999.f, -999.f};
}

void OnlineMapListing::onMouseUpOutside(bool /*left*/, bool /*right*/) { this->mousedown_coords = {-999.f, -999.f}; }

void OnlineMapListing::draw() {
    // TODO: loading indicator while stuff is loading

    const auto font = resourceManager->getFont("FONT_DEFAULT");
    const f32 padding = 5.f;
    const f32 x = this->getPos().x;
    const f32 y = this->getPos().y;
    const f32 width = this->getSize().x;
    const f32 height = this->getSize().y;

    // TODO: correct color
    // TODO: hover anim
    g->setColor(argb(100, 0, 10, 50));
    g->fillRect(x, y, width, height);

    g->pushClipRect(McRect(x, y, width, height));
    g->pushTransform();
    g->translate(x, y);

    // TODO: mapset background image

    // XXX: slow
    auto full_title = fmt::format("{} - {}", this->meta.artist, this->meta.title);
    g->setColor(0xffffffff);
    g->translate(padding, padding + font->getHeight());
    font->drawString(full_title);

    // XXX: slow
    g->pushTransform();
    f32 creator_width = font->getStringWidth(this->meta.creator);
    g->translate(width - (creator_width + 2 * padding), 0);
    font->drawString(this->meta.creator);
    g->popTransform();

    // TODO: show if it's installed somehow (gray out?)
    // TODO: map difficulties (with their own hover text...)

    if(this->downloading) {
        // TODO: downloads will not finish if player leaves this screen

        f32 progress = -1.f;
        Downloader::download_beatmapset(this->meta.set_id, &progress);
        if(progress == -1.f) {
            // TODO: how to display download error?
            this->downloading = false;
        } else if(progress < 1.f) {
            // TODO: draw download progress %
        } else {
            std::string mapset_path = fmt::format(NEOSU_MAPS_PATH "/{}/", this->meta.set_id);
            db->addBeatmapSet(mapset_path, this->meta.set_id);
            this->downloading = false;
            this->installed = true;
        }
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
    this->addBaseUIElement(this->results);

    this->onResolutionChange(osu->getVirtScreenSize());
}

CBaseUIContainer* OsuDirectScreen::setVisible(bool visible) {
    if(visible) {
        if(!BanchoState::is_online()) {
            osu->getOptionsMenu()->askForLoginDetails();
            return this;
        }

        if(db->getProgress() == 0.0) {
            // Ensure database is loaded (same as Lobby screen)
            // TODO: what happens if we cancel the load? same in lobby...
            osu->getSongBrowser()->refreshBeatmaps(true);
        }
    }

    ScreenBackable::setVisible(visible);
    osu->getMainMenu()->setVisible(!visible);

    if(visible) {
        this->search_bar->clear();
        this->newest_btn->click();
        this->search_bar->focus();
    }

    return this;
}

void OsuDirectScreen::mouse_update(bool* propagate_clicks) {
    // TODO: there's some bug where we can't select anything or click anything except back button...
    // TODO: scroll results on mouse wheel
    ScreenBackable::mouse_update(propagate_clicks);

    if(!this->isVisible()) return;

    if(this->search_bar->hitEnter()) {
        if(this->current_query == this->search_bar->getText().toUtf8() && this->current_page == -1) {
            // We're already searching for the current query, don't cancel the request
            // HACK: (this->current_page == -1) not cleanest way to detect if we're in the middle of a request
            return;
        }

        this->reset();
        this->search(this->search_bar->getText().toUtf8(), 0);
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

void OsuDirectScreen::onBack() { this->setVisible(false); }

void OsuDirectScreen::onResolutionChange(vec2 newResolution) {
    // TODO: proper sizing & dpi scaling

    ScreenBackable::onResolutionChange(newResolution);

    const f32 scale = osu->getUIScale();
    f32 x = 50.f;
    f32 y = 30.f;

    // Screen title
    this->title->setFont(osu->getTitleFont());
    this->title->setSizeToContent(0, 0);
    this->title->setRelPos(x, y);
    y += this->title->getSize().y;

    const f32 results_width = 1000.f * scale;
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
    this->results->setSize(results_width, 600.f * scale);

    this->update_pos();

    // Finally, update the contents of this->results
    {
        const f32 LISTING_MARGIN = 10.f * scale;

        f32 y = LISTING_MARGIN;
        for(auto& listing : this->results->container->getElements()) {
            listing->setRelPos(LISTING_MARGIN, y);
            listing->setSize(results_width - 2 * LISTING_MARGIN, 75.f * scale);
            y += listing->getSize().y + LISTING_MARGIN;
        }
        this->results->container->update_pos();
    }
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

                    debugLogLambda("- {}", meta.osz_filename);
                    this->results->container->addBaseUIElement(new OnlineMapListing(meta));
                }

                this->current_page = page;
                this->onResolutionChange(osu->getVirtScreenSize());
            } else {
                // TODO: handle failure
            }
        },
        options);
}
