// Copyright (c) 2024, kiwec, All rights reserved.
#include "Chat.h"

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "BeatmapInterface.h"
#include "CBaseUIButton.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUITextbox.h"
#include "ConVar.h"
#include "ChatLink.h"
#include "Engine.h"
#include "Font.h"
#include "Keyboard.h"
#include "Lobby.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "PauseMenu.h"
#include "PromptScreen.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "RoomScreen.h"
#include "Skin.h"
#include "SongBrowser/ScoreButton.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "SString.h"
#include "SpectatorScreen.h"
#include "UIButton.h"
#include "UIUserContextMenu.h"
#include "UserCard2.h"
#include "Timing.h"

#include <algorithm>
#include <cmath>
#include <regex>
#include <utility>

static McFont *chat_font = nullptr;

ChatChannel::ChatChannel(Chat *chat, UString name_arg) {
    this->chat = chat;
    this->name = std::move(name_arg);

    this->ui = new CBaseUIScrollView(0, 0, 0, 0, "");
    this->ui->setDrawFrame(false);
    this->ui->setDrawBackground(true);
    this->ui->setBackgroundColor(0xdd000000);
    this->ui->setHorizontalScrolling(false);
    this->ui->setDrawScrollbars(true);
    this->ui->sticky = true;

    if(chat != nullptr) {
        this->btn = new UIButton(0, 0, 0, 0, "button", this->name);
        this->btn->grabs_clicks = true;
        this->btn->setUseDefaultSkin();
        this->btn->setClickCallback(SA::MakeDelegate<&ChatChannel::onChannelButtonClick>(this));
        this->chat->button_container->addBaseUIElement(this->btn);
    }
}

ChatChannel::~ChatChannel() {
    SAFE_DELETE(this->ui);
    if(this->chat != nullptr) {
        this->chat->button_container->deleteBaseUIElement(this->btn);
    }
}

void ChatChannel::onChannelButtonClick(CBaseUIButton * /*btn*/) {
    if(this->chat == nullptr) return;
    soundEngine->play(osu->getSkin()->getClickButtonSound());
    this->chat->switchToChannel(this);
}

void ChatChannel::add_message(ChatMessage msg) {
    const float line_height = 20;
    const Color system_color = 0xffffff00;

    float x = 10;

    bool is_action = msg.text.startsWith("\001ACTION");
    if(is_action) {
        msg.text.erase(0, 7);
        if(msg.text.endsWith("\001")) {
            msg.text.erase(msg.text.length() - 1, 1);
        }
    }

    struct tm tm;
    localtime_x(&msg.tms, &tm);
    auto timestamp_str = UString::format("%02d:%02d ", tm.tm_hour, tm.tm_min);
    if(is_action) timestamp_str.append("*");
    float time_width = chat_font->getStringWidth(timestamp_str);
    auto *timestamp = new CBaseUILabel(x, this->y_total, time_width, line_height, "", timestamp_str);
    timestamp->setDrawFrame(false);
    timestamp->setDrawBackground(false);
    this->ui->getContainer()->addBaseUIElement(timestamp);
    x += time_width;

    bool is_system_message = msg.author_name.length() == 0;
    if(!is_system_message) {
        float name_width = chat_font->getStringWidth(msg.author_name);
        auto user_box = new UIUserLabel(msg.author_id, msg.author_name);
        user_box->setTextColor(0xff2596be);
        user_box->setPos(x, this->y_total);
        user_box->setSize(name_width, line_height);
        this->ui->getContainer()->addBaseUIElement(user_box);
        x += name_width;

        if(!is_action) {
            msg.text.insert(0, ": ");
        }
    }

    // regex101 format: (\[\[(.+?)\]\])|(\[((\S+):\/\/\S+) (.+?)\])|(https?:\/\/\S+)
    // This matches:
    // - Raw URLs      https://example.com
    // - Labeled URLs  [https://regex101.com useful website]
    // - Lobby invites [osump://0/ join my lobby plz]
    // - Wiki links    [[Chat Console]]
    //
    // Group 1) [[FAQ]]
    // Group 2) FAQ
    // Group 3) [https://regex101.com label]
    // Group 4) https://regex101.com
    // Group 5) https
    // Group 6) label
    // Group 7) https://example.com
    //
    // Groups 1, 2 only exist for wiki links
    // Groups 3, 4, 5, 6 only exist for labeled links
    // Group 7 only exists for raw links
    std::wregex url_regex(L"(\\[\\[(.+?)\\]\\])|(\\[((\\S+)://\\S+) (.+?)\\])|(https?://\\S+)");

    std::wstring msg_text = msg.text.wc_str();
    std::wsmatch match;
    std::vector<CBaseUILabel *> temp_text_fragments;
    std::wstring::const_iterator search_start = msg_text.cbegin();
    sSz text_idx = 0;
    while(std::regex_search(search_start, msg_text.cend(), match, url_regex)) {
        sSz match_pos;
        sSz match_len;
        UString link_url;
        UString link_label;
        if(match[7].matched) {
            // Raw link
            match_pos = match.position(7);
            match_len = match.length(7);
            link_url = match.str(7).c_str();
            link_label = match.str(7).c_str();
        } else if(match[3].matched) {
            // Labeled link
            match_pos = match.position(3);
            match_len = match.length(3);
            link_url = match.str(4).c_str();
            link_label = match.str(6).c_str();

            // Normalize invite links to osump://
            UString link_protocol = match.str(5).c_str();
            if(link_protocol == "osu") {
                // osu:// -> osump://
                link_url.insert(2, "mp");
            } else if(link_protocol == "http://osump") {
                // http://osump:// -> osump://
                link_url.erase(0, 7);
            }
        } else {
            // Wiki link
            match_pos = match.position(1);
            match_len = match.length(1);
            link_url = "https://osu.ppy.sh/wiki/";
            link_url.append(match.str(2).c_str());
            link_label = "wiki:";
            link_label.append(match.str(2).c_str());
        }

        sSz search_idx = search_start - msg_text.cbegin();
        auto url_start = search_idx + match_pos;
        auto preceding_text = msg.text.substr(text_idx, url_start - text_idx);
        if(preceding_text.length() > 0) {
            temp_text_fragments.push_back(new CBaseUILabel(0, 0, 0, 0, "", preceding_text));
        }

        auto link = new ChatLink(0, 0, 0, 0, link_url, link_label);
        temp_text_fragments.push_back(link);

        text_idx = url_start + match_len;
        search_start = msg_text.cbegin() + text_idx;
    }
    if(search_start != msg_text.cend()) {
        auto text = msg.text.substr(text_idx);
        temp_text_fragments.push_back(new CBaseUILabel(0, 0, 0, 0, "", text));
    }
    if(is_action) {
        // Only appending now to prevent this character from being included in a link
        msg.text.append(L"*");
        msg_text.append(L"*");
    }

    // We're offsetting the first fragment to account for the username + timestamp
    // Since first fragment will always be text, we don't care about the size being wrong
    float line_width = x;

    // We got a bunch of text fragments, now position them, and if we start a new line,
    // possibly divide them into more text fragments.
    for(auto fragment : temp_text_fragments) {
        UString text_str("");
        auto fragment_text = fragment->getText();

        for(int i = 0; i < fragment_text.length(); i++) {
            float char_width = chat_font->getGlyphMetrics(fragment_text[i]).advance_x;
            if(line_width + char_width + 20 >= this->ui->getSize().x) {
                auto *link_fragment = dynamic_cast<ChatLink *>(fragment);
                if(link_fragment == nullptr) {
                    auto *text = new CBaseUILabel(x, this->y_total, line_width - x, line_height, "", text_str);
                    text->setDrawFrame(false);
                    text->setDrawBackground(false);
                    if(is_system_message) {
                        text->setTextColor(system_color);
                    }
                    this->ui->getContainer()->addBaseUIElement(text);
                } else {
                    auto *link =
                        new ChatLink(x, this->y_total, line_width - x, line_height, fragment->getName(), text_str);
                    this->ui->getContainer()->addBaseUIElement(link);
                }

                x = 10;
                this->y_total += line_height;
                line_width = x;
                text_str = "";
            }

            text_str.append(fragment_text[i]);
            line_width += char_width;
        }

        auto *link_fragment = dynamic_cast<ChatLink *>(fragment);
        if(link_fragment == nullptr) {
            auto *text = new CBaseUILabel(x, this->y_total, line_width - x, line_height, "", text_str);
            text->setDrawFrame(false);
            text->setDrawBackground(false);
            if(is_system_message) {
                text->setTextColor(system_color);
            }
            this->ui->getContainer()->addBaseUIElement(text);
        } else {
            auto *link = new ChatLink(x, this->y_total, line_width - x, line_height, fragment->getName(), text_str);
            this->ui->getContainer()->addBaseUIElement(link);
        }

        x = line_width;
    }

    for(auto &fragment : temp_text_fragments) {
        SAFE_DELETE(fragment);
    }

    this->y_total += line_height;
    this->ui->setScrollSizeToContent();
}

void ChatChannel::updateLayout(vec2 pos, vec2 size) {
    this->ui->freeElements();
    this->ui->setPos(pos);
    this->ui->setSize(size);
    this->y_total = 7;

    for(const auto &msg : this->messages) {
        this->add_message(msg);
    }
}

Chat::Chat() : OsuScreen() {
    chat_font = resourceManager->getFont("FONT_DEFAULT");

    this->ticker = new ChatChannel(nullptr, "");
    this->ticker->ui->setVerticalScrolling(false);
    this->ticker->ui->setDrawScrollbars(false);

    this->button_container = new CBaseUIContainer(0, 0, 0, 0, "");

    this->join_channel_btn = new UIButton(0, 0, 0, 0, "button", "+");
    this->join_channel_btn->setUseDefaultSkin();
    this->join_channel_btn->setColor(0xffd9d948);
    this->join_channel_btn->setSize(this->button_height + 2, this->button_height + 2);
    this->join_channel_btn->setClickCallback(SA::MakeDelegate<&Chat::askWhatChannelToJoin>(this));
    this->button_container->addBaseUIElement(this->join_channel_btn);

    this->input_box = new CBaseUITextbox(0, 0, 0, 0, "");
    this->input_box->setDrawFrame(false);
    this->input_box->setDrawBackground(true);
    this->input_box->setBackgroundColor(0xdd000000);
    this->addBaseUIElement(this->input_box);

    this->user_list = new CBaseUIScrollView(0, 0, 0, 0, "");
    this->user_list->setDrawFrame(false);
    this->user_list->setDrawBackground(true);
    this->user_list->setBackgroundColor(0xcc000000);
    this->user_list->setHorizontalScrolling(false);
    this->user_list->setDrawScrollbars(true);
    this->user_list->setVisible(false);
    this->addBaseUIElement(this->user_list);

    this->updateLayout(osu->getVirtScreenSize());
}

Chat::~Chat() {
    for(auto &chan : this->channels) {
        SAFE_DELETE(chan);
    }
    SAFE_DELETE(this->button_container);
    SAFE_DELETE(this->ticker);
}

void Chat::draw() {
    this->drawTicker();

    const bool isAnimating = anim->isAnimating(&this->fAnimation);
    if(!this->bVisible && !isAnimating) return;

    if(isAnimating) {
        // XXX: Setting BLEND_MODE_PREMUL_ALPHA is not enough, transparency is still incorrect
        osu->getSliderFrameBuffer()->enable();
        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_ALPHA);
    }

    g->setColor(argb(100, 0, 10, 50));
    g->fillRect(this->button_container->getPos().x, this->button_container->getPos().y,
                this->button_container->getSize().x, this->button_container->getSize().y);
    this->button_container->draw();

    if(this->selected_channel == nullptr) {
        f32 chat_h = round(this->getSize().y * 0.3f);
        f32 chat_y = this->getSize().y - chat_h;
        f32 chat_w = this->isSmallChat() ? round(this->getSize().x * 0.6) : this->getSize().x;
        g->setColor(argb(150, 0, 0, 0));
        g->fillRect(0, chat_y, chat_w, chat_h);
    } else {
        OsuScreen::draw();
        this->selected_channel->ui->draw();
    }

    if(isAnimating) {
        osu->getSliderFrameBuffer()->disable();

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
        g->push3DScene(McRect(0, 0, this->getSize().x, this->getSize().y));
        {
            g->rotate3DScene(-(1.0f - this->fAnimation) * 90, 0, 0);
            g->translate3DScene(0, -(1.0f - this->fAnimation) * this->getSize().y * 1.25f,
                                -(1.0f - this->fAnimation) * 700);

            osu->getSliderFrameBuffer()->setColor(argb(this->fAnimation, 1.0f, 1.0f, 1.0f));
            osu->getSliderFrameBuffer()->draw(0, 0);
        }
        g->pop3DScene();
    }
}

void Chat::drawTicker() {
    if(!cv::chat_ticker.getBool()) return;

    f64 time_elapsed = engine->getTime() - this->ticker_tms;
    if(this->ticker_tms == 0.0 || time_elapsed > 6.0) return;

    f32 a = std::clamp(6.0 - time_elapsed, 0.0, 1.0);
    auto ticker_size = this->ticker->ui->getSize();
    if(!anim->isAnimating(&this->fAnimation)) {
        this->fAnimation = 0.f;
        if(this->isVisible()) return;  // don't draw ticker while chat is visible
    }

    // XXX: Setting BLEND_MODE_PREMUL_ALPHA is not enough, transparency is still incorrect
    osu->getSliderFrameBuffer()->enable();
    g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_ALPHA);
    this->ticker->ui->draw();
    osu->getSliderFrameBuffer()->disable();

    g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
    g->push3DScene(McRect(0, 0, ticker_size.x, ticker_size.y));
    {
        g->rotate3DScene(this->fAnimation * 90, 0, 0);
        osu->getSliderFrameBuffer()->setColor(argb(a * (1.f - this->fAnimation), 1.f, 1.f, 1.f));
        osu->getSliderFrameBuffer()->draw(0, 0);
    }
    g->pop3DScene();
}

void Chat::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    if(this->user_list->isVisible()) {
        // Request presence & stats for on-screen user cards
        for(auto elm : this->user_list->getContainer()->getElements()) {
            auto *card = (UserCard2 *)elm;
            bool is_above_bottom = card->getPos().y <= this->user_list->getPos().y + this->user_list->getSize().y;
            bool is_below_top = card->getPos().y + card->getSize().y >= this->user_list->getPos().y;
            if(is_above_bottom && is_below_top) {
                BANCHO::User::enqueue_presence_request(card->info);
                BANCHO::User::enqueue_stats_request(card->info);
            }
        }
    }

    OsuScreen::mouse_update(propagate_clicks);

    // XXX: don't let mouse click through the buttons area
    this->button_container->mouse_update(propagate_clicks);
    if(this->selected_channel) {
        this->selected_channel->ui->mouse_update(propagate_clicks);
    }

    // HACKHACK: MOUSE3 handling
    static bool was_M3_down = false;
    bool is_M3_down = mouse->isMiddleDown();
    if(is_M3_down != was_M3_down) {
        if(is_M3_down) {
            auto mpos = mouse->getPos();

            // Try to close hovered channel
            for(auto &chan : this->channels) {
                if(chan->btn->getRect().contains(mpos)) {
                    this->leave(chan->name);
                    break;
                }
            }
        }

        was_M3_down = is_M3_down;
    }

    // Focus without placing the cursor at the end of the field
    this->input_box->focus(false);
}

void Chat::handle_command(const UString &msg) {
    if(msg == "/clear") {
        this->selected_channel->messages.clear();
        this->updateLayout(osu->getVirtScreenSize());
        return;
    }

    if(msg == "/close" || msg == "/p" || msg == "/part") {
        this->leave(this->selected_channel->name);
        return;
    }

    if(msg == "/help" || msg == "/keys") {
        env->openURLInDefaultBrowser("https://osu.ppy.sh/wiki/en/Client/Interface/Chat_console");
        return;
    }

    if(msg == "/np") {
        auto map = osu->getMapInterface()->beatmap;
        if(map == nullptr) {
            this->addSystemMessage("You are not listening to anything.");
            return;
        }

        UString song_name = UString::format("%s - %s [%s]", map->getArtist().c_str(), map->getTitle().c_str(),
                                            map->getDifficultyName().c_str());
        UString song_link = UString::format("[https://osu.%s/beatmaps/%d %s]", BanchoState::endpoint.c_str(),
                                            map->getID(), song_name.toUtf8());

        UString np_msg;
        if(osu->isInPlayMode()) {
            np_msg = UString::format("\001ACTION is playing %s", song_link.toUtf8());

            auto mods = osu->getScore()->mods;
            if(mods.speed != 1.f) {
                auto speed_modifier = UString::format(" x%.1f", mods.speed);
                np_msg.append(speed_modifier);
            }
            auto mod_string = ScoreButton::getModsStringForDisplay(mods);
            if(mod_string.length() > 0) {
                np_msg.append(" (+");
                np_msg.append(mod_string);
                np_msg.append(")");
            }

            np_msg.append("\001");
        } else {
            np_msg = UString::format("\001ACTION is listening to %s\001", song_link.toUtf8());
        }

        this->send_message(np_msg);
        return;
    }

    if(msg.startsWith("/addfriend ")) {
        auto friend_name = msg.substr(11);
        auto user = BANCHO::User::find_user(friend_name);
        if(!user) {
            this->addSystemMessage(UString::format("User '%s' not found. Are they online?", friend_name.toUtf8()));
            return;
        }

        if(user->is_friend()) {
            this->addSystemMessage(UString::format("You are already friends with %s!", friend_name.toUtf8()));
        } else {
            Packet packet;
            packet.id = FRIEND_ADD;
            packet.write<i32>(user->user_id);
            BANCHO::Net::send_packet(packet);

            BANCHO::User::friends.push_back(user->user_id);

            this->addSystemMessage(UString::format("You are now friends with %s.", friend_name.toUtf8()));
        }

        return;
    }

    if(msg.startsWith("/bb ")) {
        this->addChannel("BanchoBot", true);
        this->send_message(msg.substr(4));
        return;
    }

    if(msg == "/away") {
        this->away_msg = "";
        this->addSystemMessage("Away message removed.");
        return;
    }
    if(msg.startsWith("/away ")) {
        this->away_msg = msg.substr(6);
        this->addSystemMessage(UString::format("Away message set to '%s'.", this->away_msg.toUtf8()));
        return;
    }

    if(msg.startsWith("/delfriend ")) {
        auto friend_name = msg.substr(11);
        auto user = BANCHO::User::find_user(friend_name);
        if(!user) {
            this->addSystemMessage(UString::format("User '%s' not found. Are they online?", friend_name.toUtf8()));
            return;
        }

        if(user->is_friend()) {
            Packet packet;
            packet.id = FRIEND_REMOVE;
            packet.write<i32>(user->user_id);
            BANCHO::Net::send_packet(packet);

            auto it = std::ranges::find(BANCHO::User::friends, user->user_id);
            if(it != BANCHO::User::friends.end()) {
                BANCHO::User::friends.erase(it);
            }

            this->addSystemMessage(UString::format("You are no longer friends with %s.", friend_name.toUtf8()));
        } else {
            this->addSystemMessage(UString::format("You aren't friends with %s!", friend_name.toUtf8()));
        }

        return;
    }

    if(msg.startsWith("/me ")) {
        auto new_text = msg.substr(3);
        new_text.insert(0, "\001ACTION");
        new_text.append("\001");
        this->send_message(new_text);
        return;
    }

    if(msg.startsWith("/chat ") || msg.startsWith("/msg ") || msg.startsWith("/query ")) {
        auto username = msg.substr(msg.find(L" "));
        this->addChannel(username, true);
        return;
    }

    if(msg.startsWith("/invite ")) {
        if(!BanchoState::is_in_a_multi_room()) {
            this->addSystemMessage("You are not in a multiplayer room!");
            return;
        }

        auto username = msg.substr(8);
        auto invite_msg =
            UString::format("\001ACTION has invited you to join [osump://%d/%s %s]\001", BanchoState::room.id,
                            BanchoState::room.password.toUtf8(), BanchoState::room.name.toUtf8());

        Packet packet;

        packet.id = SEND_PRIVATE_MESSAGE;

        packet.write_string((char *)BanchoState::get_username().c_str());
        packet.write_string((char *)invite_msg.toUtf8());
        packet.write_string((char *)username.toUtf8());
        packet.write<i32>(BanchoState::get_uid());
        BANCHO::Net::send_packet(packet);

        this->addSystemMessage(UString::format("%s has been invited to the game.", username.toUtf8()));
        return;
    }

    if(msg.startsWith("/j ") || msg.startsWith("/join ")) {
        auto channel = msg.substr(msg.find(L" "));
        this->join(channel);
        return;
    }

    if(msg.startsWith("/p ") || msg.startsWith("/part ")) {
        auto channel = msg.substr(msg.find(L" "));
        this->leave(channel);
        return;
    }

    this->addSystemMessage("This command is not supported.");
}

void Chat::onKeyDown(KeyboardEvent &key) {
    if(!this->bVisible) return;

    if(keyboard->isAltDown()) {
        i32 tab_select = -1;
        if(key.getKeyCode() == KEY_1) tab_select = 0;
        if(key.getKeyCode() == KEY_2) tab_select = 1;
        if(key.getKeyCode() == KEY_3) tab_select = 2;
        if(key.getKeyCode() == KEY_4) tab_select = 3;
        if(key.getKeyCode() == KEY_5) tab_select = 4;
        if(key.getKeyCode() == KEY_6) tab_select = 5;
        if(key.getKeyCode() == KEY_7) tab_select = 6;
        if(key.getKeyCode() == KEY_8) tab_select = 7;
        if(key.getKeyCode() == KEY_9) tab_select = 8;
        if(key.getKeyCode() == KEY_0) tab_select = 9;

        if(tab_select != -1) {
            if(tab_select >= this->channels.size()) {
                key.consume();
                return;
            }

            key.consume();
            this->switchToChannel(this->channels[tab_select]);
            return;
        }
    }

    if(key.getKeyCode() == KEY_PAGEUP) {
        if(this->selected_channel != nullptr) {
            key.consume();
            this->selected_channel->ui->scrollY(this->getSize().y - this->input_box_height);
            return;
        }
    }

    if(key.getKeyCode() == KEY_PAGEDOWN) {
        if(this->selected_channel != nullptr) {
            key.consume();
            this->selected_channel->ui->scrollY(-(this->getSize().y - this->input_box_height));
            return;
        }
    }

    // Escape: close chat
    if(key.getKeyCode() == KEY_ESCAPE) {
        if(this->isVisibilityForced()) return;

        key.consume();
        this->user_wants_chat = false;
        this->updateVisibility();
        return;
    }

    // Return: send message
    if(key.getKeyCode() == KEY_ENTER || key.getKeyCode() == KEY_NUMPAD_ENTER) {
        key.consume();
        if(this->selected_channel != nullptr && this->input_box->getText().length() > 0) {
            if(this->input_box->getText()[0] == L'/') {
                this->handle_command(this->input_box->getText());
            } else {
                this->send_message(this->input_box->getText());
            }

            soundEngine->play(osu->getSkin()->getMessageSentSound());
            this->input_box->clear();
        }
        this->tab_completion_prefix = "";
        this->tab_completion_match = "";
        return;
    }

    // Ctrl+W: Close current channel
    if(keyboard->isControlDown() && key.getKeyCode() == KEY_W) {
        key.consume();
        if(this->selected_channel != nullptr) {
            this->leave(this->selected_channel->name);
        }
        return;
    }

    // Ctrl+Tab: Switch channels
    // KEY_TAB doesn't work on Linux
    if(keyboard->isControlDown() && (key.getKeyCode() == 65056 || key.getKeyCode() == KEY_TAB)) {
        key.consume();
        if(this->selected_channel == nullptr) return;
        int chan_index = this->channels.size();
        for(auto chan : this->channels) {
            if(chan == this->selected_channel) {
                break;
            }
            chan_index++;
        }

        if(keyboard->isShiftDown()) {
            // Ctrl+Shift+Tab: Go to previous channel
            auto new_chan = this->channels[(chan_index - 1) % this->channels.size()];
            this->switchToChannel(new_chan);
        } else {
            // Ctrl+Tab: Go to next channel
            auto new_chan = this->channels[(chan_index + 1) % this->channels.size()];
            this->switchToChannel(new_chan);
        }

        soundEngine->play(osu->getSkin()->getClickButtonSound());

        return;
    }

    // TAB: Complete nickname
    // KEY_TAB doesn't work on Linux
    if(key.getKeyCode() == 65056 || key.getKeyCode() == KEY_TAB) {
        key.consume();

        auto text = this->input_box->getText();
        i32 username_start_idx = text.findLast(" ", 0, this->input_box->iCaretPosition) + 1;
        i32 username_end_idx = this->input_box->iCaretPosition;
        i32 username_len = username_end_idx - username_start_idx;

        if(this->tab_completion_prefix.length() == 0) {
            this->tab_completion_prefix = text.substr(username_start_idx, username_len);
        } else {
            username_start_idx = this->input_box->iCaretPosition - this->tab_completion_match.length();
            username_len = username_end_idx - username_start_idx;
        }

        auto user = BANCHO::User::find_user_starting_with(this->tab_completion_prefix, this->tab_completion_match);
        if(user) {
            this->tab_completion_match = user->name;

            // Remove current username, add new username
            this->input_box->sText.erase(this->input_box->iCaretPosition - username_len, username_len);
            this->input_box->iCaretPosition -= username_len;
            this->input_box->sText.insert(this->input_box->iCaretPosition, this->tab_completion_match);
            this->input_box->iCaretPosition += this->tab_completion_match.length();
            this->input_box->setText(this->input_box->sText);
            this->input_box->updateTextPos();
            this->input_box->tickCaret();

            Sound *sounds[] = {osu->getSkin()->getTyping1Sound(), osu->getSkin()->getTyping2Sound(),
                               osu->getSkin()->getTyping3Sound(), osu->getSkin()->getTyping4Sound()};
            soundEngine->play(sounds[rand() % 4]);
        }

        return;
    }

    // Typing in chat: capture keypresses
    if(!keyboard->isAltDown()) {
        this->tab_completion_prefix = "";
        this->tab_completion_match = "";
        this->input_box->onKeyDown(key);
        key.consume();
        return;
    }
}

void Chat::onKeyUp(KeyboardEvent &key) {
    if(!this->bVisible || key.isConsumed()) return;

    this->input_box->onKeyUp(key);
    key.consume();
}

void Chat::onChar(KeyboardEvent &key) {
    if(!this->bVisible || key.isConsumed()) return;

    this->input_box->onChar(key);
    key.consume();
}

void Chat::mark_as_read(ChatChannel *chan) {
    if(!this->bVisible) return;

    // XXX: Only mark as read after 500ms
    chan->read = true;

    UString url{"web/osu-markasread.php?"};
    url.append(UString::fmt("channel={}", env->urlEncode(chan->name.toUtf8())));
    BANCHO::Net::append_auth_params(url);

    APIRequest request;

    request.type = MARK_AS_READ;
    request.path = url;

    BANCHO::Net::send_api_request(request);
}

void Chat::switchToChannel(ChatChannel *chan) {
    this->selected_channel = chan;
    if(!chan->read) {
        this->mark_as_read(this->selected_channel);
    }

    // Update button colors
    this->updateButtonLayout(this->getSize());
}

void Chat::openChannel(const UString &channel_name) {
    this->addChannel(channel_name, true);
    this->user_wants_chat = true;
    this->updateVisibility();
}

void Chat::addChannel(const UString &channel_name, bool switch_to) {
    for(auto chan : this->channels) {
        if(chan->name == channel_name) {
            if(switch_to) {
                this->switchToChannel(chan);
            }
            return;
        }
    }

    auto *chan = new ChatChannel(this, channel_name);
    this->channels.push_back(chan);

    if(this->selected_channel == nullptr && this->channels.size() == 1) {
        this->switchToChannel(chan);
    } else if(channel_name == "#multiplayer" || channel_name == "#lobby") {
        this->switchToChannel(chan);
    } else if(switch_to) {
        this->switchToChannel(chan);
    }

    this->updateLayout(osu->getVirtScreenSize());

    if(this->isVisible()) {
        soundEngine->play(osu->getSkin()->getExpandSound());
    }
}

void Chat::addMessage(UString channel_name, const ChatMessage &msg, bool mark_unread) {
    auto user = BANCHO::User::get_user_info(msg.author_id);
    bool chatter_is_moderator = (user->privileges & Privileges::MODERATOR);
    chatter_is_moderator |= (msg.author_id == 0);  // system message

    auto ignore_list = SString::split(cv::chat_ignore_list.getString(), ' ');
    auto msg_words = msg.text.split(" ");
    if(!chatter_is_moderator) {
        for(auto &word : msg_words) {
            for(auto &ignored : ignore_list) {
                if(ignored == "") continue;

                word.lowerCase();
                if(word.toUtf8() == SString::to_lower(ignored)) {
                    // Found a word we don't want - don't print the message
                    return;
                }
            }
        }
    }

    bool should_highlight = false;
    auto highlight_list = SString::split(cv::chat_highlight_words.getString(), ' ');
    for(auto &word : msg_words) {
        if(chatter_is_moderator && word == "@everyone") {
            should_highlight = true;
            break;
        }

        for(auto &highlight : highlight_list) {
            if(highlight == "") continue;

            word.lowerCase();
            if(word.toUtf8() == SString::to_lower(highlight)) {
                should_highlight = true;
                break;
            }
        }
    }
    if(should_highlight) {
        // TODO: highlight message
        auto notif = UString::fmt("{} mentioned you in {}", msg.author_name.toUtf8(), channel_name);
        osu->getNotificationOverlay()->addToast(
            notif, CHAT_TOAST, [channel_name] { osu->getChat()->openChannel(channel_name); }, ToastElement::TYPE::CHAT);
    }

    bool is_pm = msg.author_id > 0 && channel_name[0] != '#' && msg.author_name.toUtf8() != BanchoState::get_username();
    if(is_pm) {
        // If it's a PM, the channel title should be the one who sent the message
        channel_name = msg.author_name;

        if(cv::chat_notify_on_dm.getBool()) {
            auto notif = UString::format("%s sent you a message", msg.author_name.toUtf8());
            osu->getNotificationOverlay()->addToast(
                notif, CHAT_TOAST, [channel_name] { osu->getChat()->openChannel(channel_name); },
                ToastElement::TYPE::CHAT);
        }
        if(cv::chat_ping_on_mention.getBool()) {
            // Yes, osu! really does use "match-start.wav" for when you get pinged
            // XXX: Use it as fallback, allow neosu-targeting skins to have custom ping sound
            soundEngine->play(osu->getSkin()->getMatchStartSound());
        }
    }

    bool mentioned = SString::contains_ncase(msg.text.toUtf8(), BanchoState::get_username());
    mentioned &= msg.author_id != BanchoState::get_uid();

    if(mentioned && cv::chat_notify_on_mention.getBool()) {
        auto notif = UString::format("You were mentioned in %s", channel_name.toUtf8());
        osu->getNotificationOverlay()->addToast(
            notif, CHAT_TOAST, [channel_name] { osu->getChat()->openChannel(channel_name); }, ToastElement::TYPE::CHAT);
    }
    if(mentioned && cv::chat_ping_on_mention.getBool()) {
        // Yes, osu! really does use "match-start.wav" for when you get pinged
        // XXX: Use it as fallback, allow neosu-targeting skins to have custom ping sound
        soundEngine->play(osu->getSkin()->getMatchStartSound());
    }

    this->addChannel(channel_name);
    for(auto chan : this->channels) {
        if(chan->name != channel_name) continue;
        chan->messages.push_back(msg);
        chan->add_message(msg);

        if(mark_unread) {
            chan->read = false;
            if(chan == this->selected_channel) {
                this->mark_as_read(chan);

                // Update ticker
                if(msg.author_name.toUtf8() != BanchoState::get_username()) {
                    auto screen = osu->getVirtScreenSize();
                    this->ticker_tms = engine->getTime();
                    this->ticker->messages.clear();
                    this->ticker->messages.push_back(msg);
                    this->ticker->add_message(msg);
                    this->updateTickerLayout(screen);
                }
            } else {
                this->updateButtonLayout(this->getSize());
            }
        }

        if(chan->messages.size() > 100) {
            chan->messages.erase(chan->messages.begin());
        }

        break;
    }

    if(is_pm && this->away_msg.length() > 0) {
        Packet packet;
        packet.id = SEND_PRIVATE_MESSAGE;

        packet.write_string((char *)BanchoState::get_username().c_str());
        packet.write_string((char *)this->away_msg.toUtf8());
        packet.write_string((char *)msg.author_name.toUtf8());
        packet.write<i32>(BanchoState::get_uid());
        BANCHO::Net::send_packet(packet);

        // Server doesn't echo the message back
        this->addMessage(channel_name,
                         ChatMessage{
                             .tms = time(nullptr),
                             .author_id = BanchoState::get_uid(),
                             .author_name = BanchoState::get_username().c_str(),
                             .text = this->away_msg,
                         },
                         false);
    }
}

void Chat::addSystemMessage(UString msg) {
    this->addMessage(this->selected_channel->name, ChatMessage{
                                                       .tms = time(nullptr),
                                                       .author_id = 0,
                                                       .author_name = "",
                                                       .text = std::move(msg),
                                                   });
}

void Chat::removeChannel(const UString &channel_name) {
    ChatChannel *chan = nullptr;
    for(auto c : this->channels) {
        if(c->name == channel_name) {
            chan = c;
            break;
        }
    }

    if(chan == nullptr) return;

    auto it = std::ranges::find(this->channels, chan);
    this->channels.erase(it);
    if(this->selected_channel == chan) {
        this->selected_channel = nullptr;
        if(!this->channels.empty()) {
            this->switchToChannel(this->channels[0]);
        }
    }

    delete chan;
    this->updateButtonLayout(this->getSize());
}

void Chat::updateLayout(vec2 newResolution) {
    this->updateTickerLayout(newResolution);

    // We don't want to update while the chat is hidden, to avoid lagspikes during gameplay
    if(!this->bVisible) {
        this->layout_update_scheduled = true;
        return;
    }

    // In the lobby and in multi rooms don't take the full horizontal width to allow for cleaner UI designs.
    if(this->isSmallChat()) {
        newResolution.x = std::round(newResolution.x * 0.6f);
    }

    this->setSize(newResolution);

    const float chat_w = newResolution.x;
    const float chat_h = std::round(newResolution.y * 0.3f) - this->input_box_height;
    const float chat_y = newResolution.y - (chat_h + this->input_box_height);
    for(auto chan : this->channels) {
        chan->updateLayout(vec2{0.f, chat_y}, vec2{chat_w, chat_h});
    }

    this->input_box->setPos(vec2{0.f, chat_y + chat_h});
    this->input_box->setSize(vec2{chat_w, this->input_box_height});

    if(this->selected_channel == nullptr && !this->channels.empty()) {
        this->selected_channel = this->channels[0];
        this->selected_channel->read = true;
    }

    this->updateButtonLayout(newResolution);
    this->updateButtonLayout(newResolution);  // idk
    this->layout_update_scheduled = false;
}

void Chat::updateButtonLayout(vec2 screen) {
    const float initial_x = 2;
    float total_x = initial_x;

    std::ranges::sort(this->channels, [](ChatChannel *a, ChatChannel *b) { return a->name < b->name; });

    // Look, I really tried. But for some reason setPos() doesn't work until we change
    // the screen resolution once. So I'll just compute absolute position manually.
    float button_container_height = this->button_height + 2;
    for(auto chan : this->channels) {
        UIButton *btn = chan->btn;
        float button_width = chat_font->getStringWidth(btn->getText()) + 20;

        // Wrap channel buttons
        if(total_x + button_width > screen.x - 20) {
            total_x = initial_x;
            button_container_height += this->button_height;
        }

        total_x += button_width;
    }

    const float chat_y = std::round(screen.y * 0.7f);
    float total_y = 0.f;
    total_x = initial_x;
    for(auto chan : this->channels) {
        UIButton *btn = chan->btn;
        float button_width = chat_font->getStringWidth(btn->getText()) + 20;

        // Wrap channel buttons
        if(total_x + button_width > screen.x - 20) {
            total_x = initial_x;
            total_y += this->button_height;
        }

        btn->setPos(total_x, chat_y - button_container_height + total_y);
        // Buttons are drawn a bit smaller than they should, so we up the size here
        btn->setSize(button_width + 2, this->button_height + 2);

        if(this->selected_channel->name == btn->getText()) {
            btn->setColor(0xffc9c9c9);
        } else {
            if(chan->read) {
                btn->setColor(0xff20265e);
            } else {
                btn->setColor(0xff677abc);
            }
        }

        total_x += button_width;
    }

    this->join_channel_btn->setPos(total_x, chat_y - button_container_height + total_y);
    this->button_container->setPos(0, chat_y - button_container_height);
    this->button_container->setSize(screen.x, button_container_height);

    // Update user list here, since we'll size it based on chat console height (including buttons)
    this->updateUserList();
}

void Chat::updateTickerLayout(vec2 screen) {
    this->ticker->updateLayout(vec2{0.f, 0.f}, screen);

    f32 h = this->ticker->ui->getScrollSize().y + 5;
    this->ticker->ui->setPos(vec2{0.f, screen.y - h});
    this->ticker->ui->setSize(vec2{screen.x, h});
}

void Chat::updateUserList() {
    // We don't want to update while the chat is hidden, to avoid lagspikes during gameplay
    if(!this->bVisible) {
        this->layout_update_scheduled = true;
        return;
    }

    // XXX: don't use SongBrowser::getUIScale
    auto card_size = vec2{SongBrowser::getUIScale(320), SongBrowser::getUIScale(75)};

    auto size = this->getSize();
    size.y = this->button_container->getPos().y;
    this->user_list->setSize(size);

    const f32 MARGIN = 10.f;
    const f32 INITIAL_MARGIN = MARGIN * 1.5;
    f32 total_x = INITIAL_MARGIN;
    f32 total_y = INITIAL_MARGIN;

    // XXX: Optimize so fps doesn't halve when F9 is open

    std::vector<UserInfo *> sorted_users;
    for(auto pair : BANCHO::User::online_users) {
        if(pair.second->user_id > 0) {
            sorted_users.push_back(pair.second);
        }
    }
    std::ranges::sort(sorted_users, SString::alnum_comp, [](const UserInfo *ui) { return ui->name.toUtf8(); });

    // Intentionally not calling this->user_list->clear(), because that would affect scroll position/animation
    this->user_list->getContainer()->freeElements();

    for(auto user : sorted_users) {
        if(total_x + card_size.x + MARGIN > size.x) {
            total_x = INITIAL_MARGIN;
            total_y += card_size.y + MARGIN;
        }

        auto card = new UserCard2(user->user_id);
        card->setSize(card_size);
        card->setPos(total_x, total_y);
        card->setVisible(false);

        this->user_list->getContainer()->addBaseUIElement(card);

        // Only display the card if we have presence data
        // (presence data is only fetched *after* UserCard2 is initialized)
        if(user->has_presence) {
            card->setVisible(true);
            total_x += card_size.x + MARGIN * 1.5;  // idk why margin is bogged
        }
    }
    this->user_list->setScrollSizeToContent();
}

void Chat::join(const UString &channel_name) {
    // XXX: Open the channel immediately, without letting the user send messages in it.
    //      Would require a way to signal if a channel is joined or not.
    //      Would allow to keep open the tabs of the channels we got kicked out of.
    Packet packet;
    packet.id = CHANNEL_JOIN;
    packet.write_string(channel_name.toUtf8());
    BANCHO::Net::send_packet(packet);
}

void Chat::leave(const UString &channel_name) {
    bool send_leave_packet = channel_name[0] == '#';
    if(channel_name == "#lobby") send_leave_packet = false;
    if(channel_name == "#multiplayer") send_leave_packet = false;

    if(send_leave_packet) {
        Packet packet;
        packet.id = CHANNEL_PART;
        packet.write_string(channel_name.toUtf8());
        BANCHO::Net::send_packet(packet);
    }

    this->removeChannel(channel_name);

    soundEngine->play(osu->getSkin()->getCloseChatTabSound());
}

void Chat::send_message(const UString &msg) {
    Packet packet;
    packet.id = this->selected_channel->name[0] == '#' ? SEND_PUBLIC_MESSAGE : SEND_PRIVATE_MESSAGE;

    packet.write_string((char *)BanchoState::get_username().c_str());
    packet.write_string((char *)msg.toUtf8());
    packet.write_string((char *)this->selected_channel->name.toUtf8());
    packet.write<i32>(BanchoState::get_uid());
    BANCHO::Net::send_packet(packet);

    // Server doesn't echo the message back
    this->addMessage(this->selected_channel->name,
                     ChatMessage{
                         .tms = time(nullptr),
                         .author_id = BanchoState::get_uid(),
                         .author_name = BanchoState::get_username().c_str(),
                         .text = msg,
                     },
                     false);
}

void Chat::onDisconnect() {
    for(auto chan : this->channels) {
        delete chan;
    }
    this->channels.clear();

    for(const auto &chan : BanchoState::chat_channels) {
        delete chan.second;
    }
    BanchoState::chat_channels.clear();

    this->selected_channel = nullptr;
    this->updateLayout(osu->getVirtScreenSize());

    this->updateVisibility();
}

void Chat::onResolutionChange(vec2 newResolution) { this->updateLayout(newResolution); }

bool Chat::isSmallChat() {
    if(osu->getRoom() == nullptr || osu->getLobby() == nullptr || osu->getSongBrowser() == nullptr) return false;
    bool sitting_in_room =
        osu->getRoom()->isVisible() && !osu->getSongBrowser()->isVisible() && !BanchoState::is_playing_a_multi_map();
    bool sitting_in_lobby = osu->getLobby()->isVisible();
    return sitting_in_room || sitting_in_lobby;
}

bool Chat::isVisibilityForced() {
    bool is_forced = this->isSmallChat();
    if(is_forced != this->visibility_was_forced) {
        // Chat width changed: update the layout now
        this->visibility_was_forced = is_forced;
        this->updateLayout(osu->getVirtScreenSize());
    }
    return is_forced;
}

void Chat::updateVisibility() {
    bool can_skip = osu->getMapInterface()->isInSkippableSection();
    bool is_spectating = cv::mod_autoplay.getBool() || (cv::mod_autopilot.getBool() && cv::mod_relax.getBool()) ||
                         osu->getMapInterface()->is_watching || BanchoState::spectating;
    bool is_clicking_circles = osu->isInPlayMode() && !can_skip && !is_spectating && !osu->getPauseMenu()->isVisible();
    if(BanchoState::is_playing_a_multi_map() && !BanchoState::room.all_players_loaded) {
        is_clicking_circles = false;
    }
    is_clicking_circles &= cv::chat_auto_hide.getBool();
    bool force_hide = osu->getOptionsMenu()->isVisible() || osu->getModSelector()->isVisible() || is_clicking_circles;
    if(!BanchoState::is_online()) force_hide = true;

    if(force_hide) {
        this->setVisible(false);
    } else if(this->isVisibilityForced()) {
        this->setVisible(true);
    } else {
        this->setVisible(this->user_wants_chat);
    }
}

CBaseUIContainer *Chat::setVisible(bool visible) {
    if(visible == this->bVisible) return this;

    soundEngine->play(osu->getSkin()->getClickButtonSound());

    if(visible && BanchoState::get_uid() <= 0) {
        osu->getOptionsMenu()->askForLoginDetails();
        return this;
    }

    this->bVisible = visible;
    if(visible) {
        osu->getOptionsMenu()->setVisible(false);
        anim->moveQuartOut(&this->fAnimation, 1.0f, 0.25f * (1.0f - this->fAnimation), true);

        if(this->selected_channel != nullptr && !this->selected_channel->read) {
            this->mark_as_read(this->selected_channel);
        }

        if(this->layout_update_scheduled) {
            this->updateLayout(osu->getVirtScreenSize());
        }
    } else {
        anim->moveQuadOut(&this->fAnimation, 0.0f, 0.25f * this->fAnimation, true);
    }

    return this;
}

bool Chat::isMouseInChat() {
    if(!this->isVisible()) return false;
    if(this->selected_channel == nullptr) return false;
    return this->input_box->isMouseInside() || this->selected_channel->ui->isMouseInside();
}

void Chat::askWhatChannelToJoin(CBaseUIButton * /*btn*/) {
    // XXX: Could display nicer UI with full channel list (chat_channels in Bancho.cpp)
    osu->getPromptScreen()->prompt("Type in the channel you want to join (e.g. '#osu'):",
                                   SA::MakeDelegate<&Chat::join>(this));
}
