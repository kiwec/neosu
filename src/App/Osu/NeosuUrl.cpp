// Copyright (c) 2025, kiwec, All rights reserved.
#include "NeosuUrl.h"

#include "crypto.h"
#include "Bancho.h"
#include "ConVar.h"
#include "Engine.h"
#include "NetworkHandler.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Osu.h"
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

        osu->getOptionsMenu()->setLoginLoadingState(true);

        auto endpoint = params[3];

        auto code = env->urlEncode(params[4]);
        auto proof = env->urlEncode(crypto::conv::encode64(BanchoState::oauth_verifier, 32));
        auto url = UString::fmt("https://{}/connect/finish?code={}&proof={}", endpoint, code, proof);

        NetworkHandler::RequestOptions options;
        options.timeout = 30;
        options.connectTimeout = 5;
        options.userAgent = BanchoState::user_agent.toUtf8();
        options.followRedirects = true;

        networkHandler->httpRequestAsync(
            url,
            [](NetworkHandler::Response response) {
                if(response.success) {
                    cv::mp_oauth_token.setValue(response.body);
                    BanchoState::reconnect();
                } else {
                    osu->getOptionsMenu()->setLoginLoadingState(false);
                    osu->getNotificationOverlay()->addToast("Login failed.", ERROR_TOAST);
                }
            },
            options
        );

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
