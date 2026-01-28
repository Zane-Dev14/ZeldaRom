/**
 * @file sluggula.c
 * @ingroup Enemies
 *
 * @brief Sluggula enemy
 */
#include "enemy.h"
#include "physics.h"

void sub_08023E10(Entity*);
void sub_08023E54(Entity*);
void sub_08023E9C(Entity*);

extern void (*const Sluggula_Functions[])(Entity*);
extern void (*const gUnk_080CBDD4[])(Entity*);
extern void (*const gUnk_080CBDEC[])(Entity*);

extern const s8 gUnk_080CBDF4[];
extern const s8 gUnk_080CBDF7[];
extern const s8 gUnk_080CBDFF[];

void Sluggula(Entity* this) {
    if (this->type == 1) {
        EnemyFunctionHandler(this, Sluggula_Functions);

        EnemySetFXOffset(this, 0, 1, -0x10);
    } else if (this->type == 0) {
        if (this->z.HALF.HI == 0) {
            u32 idx = sub_080012DC(this);
            if (idx != 0) {
                gUnk_080012C8[idx](this);
                return;
            }
        }
        gUnk_080CBDD4[GetNextFunction(this)](this);

        EnemySetFXOffset(this, 0, 1, -0x10);
    } else {
        sub_08023E10(this);
    }
}

void Sluggula_OnTick(Entity* this) {
    gUnk_080CBDEC[this->action](this);
}

void Sluggula_OnCollision(Entity* this) {
    if (this->confusedTime)
        EnemyCreateFX(this, FX_STARS);

    if (this->type == 1) {
        EnemyFunctionHandlerAfterCollision(this, Sluggula_Functions);
    } else {
        EnemyFunctionHandlerAfterCollision(this, gUnk_080CBDD4);
    }
}

void Sluggula_OnGrabbed(Entity* this) {
    /* ... */
}

void sub_08023C5C(Entity* this) {
    sub_0804A720(this);
    this->action = 1;
    this->timer = (Random() & 0x30) + 180;
    this->direction = DirectionRound(Random());
    this->animationState = this->direction >> 3;
    InitializeAnimation(this, this->animationState);
}

void sub_08023C8C(Entity* this) {
    ProcessMovement0(this);
    GetNextFrame(this);
    sub_08023E54(this);
    if (--this->timer == 0) {
        if (this->frame != 1) {
            this->timer = 8;
        } else {
            this->timer = (Random() & 0x30) + 180;
            sub_08023E9C(this);
            this->animationState = this->direction >> 3;
            InitializeAnimation(this, this->animationState);
        }
    }
}
/*
void sub_08023CE0(Entity* this) {
    switch (this->action) {
        case 0:
            sub_0804A720(this);
            if (this->type2 == 1) {
                this->action = 2;
                if (this->timer == 0) {
                    this->timer = 1;
                }
                this->spriteSettings.draw = 3;
            } else {
                this->action = 1;
            }
            this->z.HALF.HI = -0x80;
            this->spriteRendering.b3 = 1;
            this->spriteOrientation.flipY = 1;
            InitializeAnimation(this, 6);
            break;
        case 1:
            if (PlayerInRange(this, 1, 0x20)) {
                this->action = 2;
                if (this->timer == 0) {
                    this->timer = 8;
                }
                this->spriteSettings.draw = 3;
            }
            break;
        case 2:
            if (this->timer != 0) {
                if (--this->timer == 0) {
                    EnqueueSFX(SFX_12D);
                    InitializeAnimation(this, 4);
                }
            } else if (!GravityUpdate(this, Q_8_8(24))) {
                this->action = 3;
                this->spriteSettings.draw = 1;
                EnqueueSFX(SFX_WATER_SPLASH);
                UpdateSpriteForCollisionLayer(this);
            }
            break;
        default:
            GetNextFrame(this);
            if (this->frame) {
                if (this->frame & ANIM_DONE) {
                    Entity* entity = CreateEnemy(DARK_NUT, 0);
                    if (entity != NULL) {
                        EnemyCopyParams(this, entity);
                        DeleteThisEntity();
                    }
                } else {
                    this->y.HALF.HI += gUnk_080CBDF4[this->frame - 1];
                    this->frame = 0;
                }
            }
            break;
    }
}*/
#define ROOMFLAG_MAZE_SPAWN_STAGE  0x10

#include "flags.h"
void sub_08023CE0(Entity* this) {
    Entity* entity;
    int i;

    /* pixel offsets (small spread, safe) */
    static const s8 offsets[9][2] = {
        {  0,   0 },   /* Darknut (center) */

        { -16, -16 },  /* Wizzrobes */
        {  16, -16 },
        { -16,  16 },
        {  16,  16 },
        {   0,  24 },

        { -24,   0 },  /* Bow Moblins */
        {  24,   0 },
        {   0, -24 },
    };
    switch (this->action) {
        case 0:
            sub_0804A720(this);
            if (this->type2 == 1) {
                this->action = 2;
                if (this->timer == 0) {
                    this->timer = 1;
                }
                this->spriteSettings.draw = 3;
            } else {
                this->action = 1;
            }
            this->z.HALF.HI = -0x80;
            this->spriteRendering.b3 = 1;
            this->spriteOrientation.flipY = 1;
            InitializeAnimation(this, 6);
            break;

        case 1:
            if (PlayerInRange(this, 1, 0x20)) {
                this->action = 2;
                if (this->timer == 0) {
                    this->timer = 8;
                }
                this->spriteSettings.draw = 3;
            }
            break;

        case 2:
            if (this->timer != 0) {
                if (--this->timer == 0) {
                    EnqueueSFX(SFX_12D);
                    InitializeAnimation(this, 4);
                }
            } else if (!GravityUpdate(this, Q_8_8(24))) {
                this->action = 3;
                this->spriteSettings.draw = 1;
                EnqueueSFX(SFX_WATER_SPLASH);
                UpdateSpriteForCollisionLayer(this);
            }
            break;
        default:
            GetNextFrame(this);
            if (this->frame) {
                if (this->frame & ANIM_DONE) {
                    Entity* entity;
                    int i;

                    /* ---- Stage 0: Darknut ---- */
                    if (!CheckRoomFlag(ROOMFLAG_MAZE_SPAWN_STAGE)) {
                        entity = CreateEnemy(DARK_NUT, 0);
                        if (entity) {
                            EnemyCopyParams(this, entity);
                            SetRoomFlag(ROOMFLAG_MAZE_SPAWN_STAGE);
                        }
                    }
                    /* ---- Stage 1: 6 Wizzrobes ---- */
                    else if (!CheckRoomFlag(ROOMFLAG_MAZE_SPAWN_STAGE + 1)) {
                        for (i = 0; i < 6; i++) {
                            entity = CreateEnemy(WIZZROBE_FIRE, 0);
                            if (entity) {
                                EnemyCopyParams(this, entity);
                                entity->x.HALF.HI += offsets[i + 1][0];
                                entity->y.HALF.HI += offsets[i + 1][1];
                            }
                        }
                        SetRoomFlag(ROOMFLAG_MAZE_SPAWN_STAGE + 1);
                    }
                    /* ---- Stage 2: 3 Bow Moblins ---- */
                    else if (!CheckRoomFlag(ROOMFLAG_MAZE_SPAWN_STAGE + 2)) {
                        for (i = 0; i < 3; i++) {
                            entity = CreateEnemy(BOW_MOBLIN, 0);
                            if (entity) {
                                EnemyCopyParams(this, entity);
                                entity->x.HALF.HI += offsets[i + 7][0];
                                entity->y.HALF.HI += offsets[i + 7][1];
                            }
                        }
                        SetRoomFlag(ROOMFLAG_MAZE_SPAWN_STAGE + 2);
                    }

                    DeleteThisEntity();
                } else {
                    this->y.HALF.HI += gUnk_080CBDF4[this->frame - 1];
                    this->frame = 0;
                }
            }
            break;




    }
}

void sub_08023E10(Entity* this) {
    if (this->action == 0) {
        this->action = 1;
        this->spritePriority.b0 = 6;
        InitializeAnimation(this, 5);
    }

    GetNextFrame(this);
    if (this->frame & ANIM_DONE)
        DeleteEntity(this);
}

void sub_08023E54(Entity* this) {
    Entity* e;
    int i;
    static const s8 offsets[5][2] = {
        { -16, -16 },
        {  16, -16 },
        { -16,  16 },
        {  16,  16 },
        {   0,   0 },
    };


    SetLocalFlag(0x60);

    for (i = 0; i < 1; i++) {
        e = CreateEnemy(WIZZROBE_FIRE, 0);
        if (e != NULL) {
            PositionRelative(
                this,
                e,
                Q_16_16(offsets[i][0]),
                             Q_16_16(offsets[i][1])
            );
        }
    }
}


extern Entity* gUnk_020000B0;

void sub_08023E9C(Entity* this) {
    u32 uVar3 = Random();
    if (!sub_08049FA0(this) && (uVar3 & 1)) {
        this->direction = DirectionRoundUp(sub_08049EE4(this));
    } else if (sub_08049FDC(this, 1) && (uVar3 & 6)) {
        u32 uVar3 = GetFacingDirection(this, gUnk_020000B0) - this->direction;
        if (uVar3 != 0 && ((uVar3 + 4) & 0x1f) > 8) {
            if ((uVar3 & 0x1f) < 0x10) {
                this->direction = DirectionRound(this->direction + 8);
            } else {
                this->direction = DirectionRound(this->direction - 8);
            }
        }
    } else {
        this->direction = DirectionRound(this->direction + gUnk_080CBDFF[(s32)(uVar3 >> 4) % 3]);
    }
}

// clang-format off
void (*const Sluggula_Functions[])(Entity*) = {
    Sluggula_OnTick,
    Sluggula_OnCollision,
    GenericKnockback,
    GenericDeath,
    GenericConfused,
    Sluggula_OnGrabbed,
};

void (*const gUnk_080CBDD4[])(Entity*) = {
    sub_08023CE0,
    Sluggula_OnCollision,
    GenericKnockback,
    GenericDeath,
    GenericConfused,
    Sluggula_OnGrabbed,
};

void (*const gUnk_080CBDEC[])(Entity*) = {
    sub_08023C5C,
    sub_08023C8C,
};

const s8 gUnk_080CBDF4[] = {
    1, 2, 6,
};

const s8 gUnk_080CBDF7[] = {
    0, 2, -2, 2, 0, -2, 2, 2
};

const s8 gUnk_080CBDFF[] = {
    0, -8, 8,
};
// clang-format on
