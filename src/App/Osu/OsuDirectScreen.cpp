#include "OsuDirectScreen.h"

#include "BanchoApi.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "CBaseUITextbox.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "Graphics.h"
#include "Mouse.h"
#include "NetworkHandler.h"
#include "NotificationOverlay.h"
#include "Osu.h"
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
    f32 x = this->getPos().x;
    f32 y = this->getPos().y;
    const f32 width = this->getSize().x;
    const f32 height = this->getSize().y;

    // TODO: correct color
    // TODO: hover anim
    g->setColor(argb(100, 0, 10, 50));
    g->fillRect(x, y, width, height);

    // TODO: mapset background image

    g->pushClipRect(McRect(x, y, width, height));
    // TODO: map title, artist, creator
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
    this->newest_btn->setClickCallback([this]() {
        this->reset();
        this->search("Newest", 0);
    });
    this->addBaseUIElement(this->newest_btn);

    this->best_rated_btn = new UIButton(0, 0, 0, 0, "", "Best maps");
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
        if(db->getProgress() == 0.0) {
            // Ensure database is loaded (same as Lobby screen)
            osu->getSongBrowser()->refreshBeatmaps(true);
        }

        this->search_bar->clear();
        this->newest_btn->click();
    }

    ScreenBackable::setVisible(visible);
    return this;
}

void OsuDirectScreen::mouse_update(bool* propagate_clicks) {
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
    ScreenBackable::onResolutionChange(newResolution);

    f32 scale = osu->getUIScale();
    f32 x = 0;  // TODO
    f32 y = 0;  // TODO

    // Screen title
    this->title->setFont(osu->getTitleFont());
    this->title->setSizeToContent(0, 0);
    this->title->setRelPos(x, y);
    y += this->title->getSize().y;

    // Search bar & buttons
    this->search_bar->setRelPos(x, y);
    this->search_bar->setSize(200.0 * scale, 10.0 * scale);
    x += this->search_bar->getSize().x;
    const f32 BUTTONS_MARGIN = 10.f * scale;
    x += BUTTONS_MARGIN;
    this->newest_btn->setRelPos(x, y);
    x += BUTTONS_MARGIN;
    this->best_rated_btn->setRelPos(x, y);
    y += this->search_bar->getSize().y;

    // Results list
    // TODO: center & size properly
    this->results->setRelPos(x, y);
    this->results->setSize(400.f, 400.f);

    this->update_pos();

    // Finally, update the contents of this->results
    {
        const f32 LISTING_MARGIN = 10.f * scale;

        f32 y = LISTING_MARGIN;
        for(auto& listing : this->results->container->getElements()) {
            listing->setRelPos(LISTING_MARGIN, y);
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
    std::string url = fmt::format("/web/osu-search.php?m=0&r={}&q={}&p={}", filter, NeoNet::urlEncode(query), page);
    BANCHO::Api::append_auth_params(url);

    NeoNet::RequestOptions options;
    options.timeout = 5;
    options.connect_timeout = 5;
    options.user_agent = "osu!";

    const auto current_request_id = ++this->request_id;
    this->current_query = query;
    this->last_search_time = engine->getTime();

    networkHandler->httpRequestAsync(
        url,
        [current_request_id, page, this](const NeoNet::Response& response) {
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

                for(i32 i = 1; i < set_lines.size(); i++) {
                    const auto meta = Downloader::parse_beatmapset_metadata(set_lines[i]);
                    if(meta.set_id == 0) continue;

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
