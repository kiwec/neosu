// Copyright (c) 2024, kiwec, All rights reserved.
#include "UIAvatar.h"

#include "ThumbnailManager.h"
#include "Bancho.h"
#include "Engine.h"
#include "Environment.h"
#include "Osu.h"
#include "UI.h"
#include "Graphics.h"
#include "UIUserContextMenu.h"
#include "MakeDelegateWrapper.h"

UIAvatar::UIAvatar(i32 player_id, float xPos, float yPos, float xSize, float ySize)
    : CBaseUIButton(xPos, yPos, xSize, ySize, "avatar", "") {
    this->player_id_for_endpoint = {
        player_id, fmt::format("{}/avatars/{}/{}", env->getCacheDir(), BanchoState::endpoint, player_id)};

    this->setClickCallback(SA::MakeDelegate<&UIAvatar::onAvatarClicked>(this));

    // add to load queue
    osu->getThumbnailManager()->request_image(this->player_id_for_endpoint);
}

UIAvatar::~UIAvatar() {
    // remove from load queue
    if(ThumbnailManager *am = osu && osu->getThumbnailManager() ? osu->getThumbnailManager().get() : nullptr) {
        am->discard_image(this->player_id_for_endpoint);
    }
}

void UIAvatar::draw_avatar(float alpha) {
    if(!this->on_screen) return;  // Comment when you need to debug on_screen logic

    auto *avatar_image = osu->getThumbnailManager()->try_get_image(this->player_id_for_endpoint);
    if(avatar_image) {
        g->pushTransform();
        g->setColor(Color(0xffffffff).setA(alpha));

        g->scale(this->getSize().x / avatar_image->getWidth(), this->getSize().y / avatar_image->getHeight());
        g->translate(this->getPos().x + this->getSize().x / 2.0f, this->getPos().y + this->getSize().y / 2.0f);
        g->drawImage(avatar_image);
        g->popTransform();
    }

    // For debugging purposes
    // if(on_screen) {
    //     g->pushTransform();
    //     g->setColor(0xff00ff00);
    //     g->drawQuad((int)this->getPos().x, (int)this->getPos().y, (int)this->getSize().x, (int)this->getSize().y);
    //     g->popTransform();
    // } else {
    //     g->pushTransform();
    //     g->setColor(0xffff0000);
    //     g->drawQuad((int)this->getPos().x, (int)this->getPos().y, (int)this->getSize().x, (int)this->getSize().y);
    //     g->popTransform();
    // }
}

void UIAvatar::onAvatarClicked(CBaseUIButton * /*btn*/) {
    if(osu->isInPlayMode()) {
        // Don't want context menu to pop up while playing a map
        return;
    }

    ui->getUserActions()->open(this->player_id_for_endpoint.first);
}
