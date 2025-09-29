#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.

#include "CBaseUIButton.h"

class ConVar;

class UIAvatar;
struct UserInfo;

class UserCard2 final : public CBaseUIButton {
    NOCOPY_NOMOVE(UserCard2)
   public:
    UserCard2(i32 user_id);
    ~UserCard2() override;

    void draw() override;

    UserInfo* info = nullptr;
    UIAvatar* avatar = nullptr;
};
