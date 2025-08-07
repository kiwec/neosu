#pragma once
// Copyright (c) 2018, PG, All rights reserved.
#include "CBaseUIButton.h"

class ConVar;

class UIAvatar;

class UserCard : public CBaseUIButton {
   public:
    UserCard(i32 user_id);
    ~UserCard() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void updateUserStats();
    void setID(u32 new_id);

    i32 user_id = 0;

   private:
    UIAvatar *avatar = NULL;

    float fPP;
    float fAcc;
    int iLevel;
    float fPercentToNextLevel;

    float fPPDelta;
    float fPPDeltaAnim;
};
