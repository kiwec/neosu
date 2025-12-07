// Copyright (c) 2025, kiwec, All rights reserved.
#include "NeosuUrl.h"

#include "crypto.h"
#include "Bancho.h"
#include "OsuConVars.h"
#include "Engine.h"
#include "NetworkHandler.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Environment.h"
#include "SString.h"
#include "Logging.h"

void handle_neosu_url(const char *url) {
    if(strstr(url, "neosu://login/") == url) {
        // Disable autologin, in case there's an error while logging in
        // Will be reenabled after the login succeeds
        cv::mp_autologin.setValue(false);

        auto params = SString::split(url, '/');
        if(params.size() != 5) {
            debugLog("Expected 5 login parameters, got {}!", params.size());
            return;
        }

        BanchoState::update_online_status(OnlineStatus::LOGIN_IN_PROGRESS);

        auto endpoint = params[3];

        auto code = env->urlEncode(params[4]);
        auto proof = env->urlEncode(crypto::conv::encode64(BanchoState::oauth_verifier));
        auto url = fmt::format("https://{}/connect/finish?code={}&proof={}", endpoint, code, proof);

        NeoNet::RequestOptions options;
        options.timeout = 30;
        options.connect_timeout = 5;
        options.user_agent = BanchoState::user_agent.toUtf8();
        options.follow_redirects = true;

        networkHandler->httpRequestAsync(
            url,
            [](NeoNet::Response response) {
                if(response.success) {
                    cv::mp_oauth_token.setValue(response.body);
                    BanchoState::reconnect();
                } else {
                    BanchoState::update_online_status(OnlineStatus::LOGGED_OUT);
                    osu->getNotificationOverlay()->addToast(ULITERAL("Login failed."), ERROR_TOAST);
                }
            },
            options);

        return;
    }

    if(!strcmp(url, "neosu://run")) {
        // nothing to do
        return;
    }

    if(strstr(url, "neosu://join_lobby/") == url) {
        // TODO @kiwec: lobby id
        return;
    }

    if(strstr(url, "neosu://select_map/") == url) {
        // TODO @kiwec: beatmapset + md5 combo
        return;
    }

    if(strstr(url, "neosu://spectate/") == url) {
        // TODO @kiwec: user id
        return;
    }

    if(strstr(url, "neosu://watch_replay/") == url) {
        // TODO @kiwec: replay md5
        return;
    }
}
