#include "vt.h"
#include "z_link_puppet.h"
#include <objects/gameplay_keep/gameplay_keep.h>
#include <string.h>

#define FLAGS (ACTOR_FLAG_0 | ACTOR_FLAG_3 | ACTOR_FLAG_4)

void LinkPuppet_Init(Actor* thisx, GlobalContext* globalCtx);
void LinkPuppet_Destroy(Actor* thisx, GlobalContext* globalCtx);
void LinkPuppet_Update(Actor* thisx, GlobalContext* globalCtx);
void LinkPuppet_Draw(Actor* thisx, GlobalContext* globalCtx);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_HIT5,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK1,
        { 0xFFCFFFFF, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 15, 50, 0, { 0, 0, 0 } },
};

const ActorInit Link_Puppet_InitVars = {
    ACTOR_LINK_PUPPET,
    ACTORCAT_NPC,
    FLAGS,
    OBJECT_LINK_BOY,
    sizeof(LinkPuppet),
    (ActorFunc)LinkPuppet_Init,
    (ActorFunc)LinkPuppet_Destroy,
    (ActorFunc)LinkPuppet_Update,
    (ActorFunc)LinkPuppet_Draw,
    NULL,
};

static Vec3s D_80854730 = { -57, 3377, 0 };

extern func_80833338(Player* this);

typedef enum {
    PUPPET_DMGEFF_NONE,
    PUPPET_DMGEFF_NORMAL,
    PUPPET_DMGEFF_ICE,
    PUPPET_DMGEFF_FIRE,
    PUPPET_DMGEFF_THUNDER,
    PUPPET_DMGEFF_KNOCKBACK,
    PUPPET_DMGEFF_STUN,
} PuppetDamageEffect;

static DamageTable sDamageTable[] = {
    /* Deku nut      */ DMG_ENTRY(0, PUPPET_DMGEFF_STUN),
    /* Deku stick    */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Slingshot     */ DMG_ENTRY(1, PUPPET_DMGEFF_NONE),
    /* Explosive     */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Boomerang     */ DMG_ENTRY(0, PUPPET_DMGEFF_NONE),
    /* Normal arrow  */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Hammer swing  */ DMG_ENTRY(2, PUPPET_DMGEFF_KNOCKBACK),
    /* Hookshot      */ DMG_ENTRY(0, PUPPET_DMGEFF_NONE),
    /* Kokiri sword  */ DMG_ENTRY(1, PUPPET_DMGEFF_NONE),
    /* Master sword  */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Giant's Knife */ DMG_ENTRY(4, PUPPET_DMGEFF_NONE),
    /* Fire arrow    */ DMG_ENTRY(2, PUPPET_DMGEFF_FIRE),
    /* Ice arrow     */ DMG_ENTRY(2, PUPPET_DMGEFF_ICE),
    /* Light arrow   */ DMG_ENTRY(2, PUPPET_DMGEFF_THUNDER),
    /* Unk arrow 1   */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Unk arrow 2   */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Unk arrow 3   */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Fire magic    */ DMG_ENTRY(2, PUPPET_DMGEFF_FIRE),
    /* Ice magic     */ DMG_ENTRY(0, PUPPET_DMGEFF_ICE),
    /* Light magic   */ DMG_ENTRY(3, PUPPET_DMGEFF_THUNDER),
    /* Shield        */ DMG_ENTRY(0, PUPPET_DMGEFF_NONE),
    /* Mirror Ray    */ DMG_ENTRY(0, PUPPET_DMGEFF_NONE),
    /* Kokiri spin   */ DMG_ENTRY(1, PUPPET_DMGEFF_NONE),
    /* Giant spin    */ DMG_ENTRY(4, PUPPET_DMGEFF_NONE),
    /* Master spin   */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Kokiri jump   */ DMG_ENTRY(2, PUPPET_DMGEFF_NONE),
    /* Giant jump    */ DMG_ENTRY(8, PUPPET_DMGEFF_NONE),
    /* Master jump   */ DMG_ENTRY(4, PUPPET_DMGEFF_NONE),
    /* Unknown 1     */ DMG_ENTRY(0, PUPPET_DMGEFF_NONE),
    /* Unblockable   */ DMG_ENTRY(0, PUPPET_DMGEFF_NONE),
    /* Hammer jump   */ DMG_ENTRY(4, PUPPET_DMGEFF_KNOCKBACK),
    /* Unknown 2     */ DMG_ENTRY(0, PUPPET_DMGEFF_NONE),
};

void LinkPuppet_Init(Actor* thisx, GlobalContext* globalCtx) {
    LinkPuppet* this = (LinkPuppet*)thisx;

    this->actor.room = -1;
    this->actor.targetMode = 1;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 90.0f);

    SkelAnime_InitLink(globalCtx, &this->linkSkeleton, gPlayerSkelHeaders[((void)0, gSaveContext.linkAge)],
                       gPlayerAnim_link_normal_wait, 9, this->linkSkeleton.jointTable, this->linkSkeleton.morphTable,
                       PLAYER_LIMB_MAX);

    Collider_InitCylinder(globalCtx, &this->collider);
    Collider_SetCylinder(globalCtx, &this->collider, &this->actor, &sCylinderInit);

    this->actor.colChkInfo.damageTable = sDamageTable;
}

void LinkPuppet_Destroy(Actor* thisx, GlobalContext* globalCtx) {
    LinkPuppet* this = (LinkPuppet*)thisx;

    Collider_DestroyCylinder(globalCtx, &this->collider);
}

void LinkPuppet_Update(Actor* thisx, GlobalContext* globalCtx) {
    LinkPuppet* this = (LinkPuppet*)thisx;

    Actor_SetFocus(this, 60.0f);

    Actor_UpdateBgCheckInfo(globalCtx, &this->actor, 15.0f, 30.0f, 60.0f, 0x1D);

    if (this->damageTimer > 0) {
        this->damageTimer--;
    }

    if (this->packet.didDamage > 0 && GET_PLAYER(globalCtx)->invincibilityTimer <= 0 &&
        !Player_InBlockingCsMode(globalCtx, GET_PLAYER(globalCtx))) {

        if (this->packet.didDamage == PUPPET_DMGEFF_NORMAL) {
            Player_InflictDamage(globalCtx, -16);
            func_80837C0C(globalCtx, GET_PLAYER(globalCtx), 0, 0, 0, 0, 0);
            GET_PLAYER(globalCtx)->invincibilityTimer = 18;
        } else if (this->packet.didDamage == PUPPET_DMGEFF_ICE) {
            GET_PLAYER(globalCtx)->stateFlags1 &= ~(PLAYER_STATE1_10 | PLAYER_STATE1_11);
            func_80837C0C(globalCtx, GET_PLAYER(globalCtx), 3, 0.0f, 0.0f, 0, 20);
            GET_PLAYER(globalCtx)->invincibilityTimer = 18;
        } else if (this->packet.didDamage == PUPPET_DMGEFF_FIRE) {
            for (int i = 0; i < 18; i++) {
                GET_PLAYER(globalCtx)->flameTimers[i] = Rand_S16Offset(0, 200);
            }
            GET_PLAYER(globalCtx)->isBurning = true;
            func_80837C0C(gGlobalCtx, GET_PLAYER(globalCtx), 0, 0, 0, 0, 0);
            GET_PLAYER(globalCtx)->invincibilityTimer = 18;
        } else if (this->packet.didDamage == PUPPET_DMGEFF_THUNDER) {
            func_80837C0C(globalCtx, GET_PLAYER(globalCtx), 4, 0.0f, 0.0f, 0, 20);
            GET_PLAYER(globalCtx)->invincibilityTimer = 18;
        } else if (this->packet.didDamage == PUPPET_DMGEFF_KNOCKBACK) {
            func_8002F71C(globalCtx, &this->actor, 100.0f * 0.04f + 4.0f, this->actor.world.rot.y, 8.0f);
            GET_PLAYER(globalCtx)->invincibilityTimer = 28;
        } else if (this->packet.didDamage == PUPPET_DMGEFF_STUN) {
            GET_PLAYER(globalCtx)->actor.freezeTimer = 40;
            Actor_SetColorFilter(&GET_PLAYER(globalCtx)->actor, 0, 0xFF, 0, 40);
        }

        this->packet.didDamage = 0;
    }

    if (this->collider.base.acFlags & AC_HIT && this->damageTimer <= 0) {
        this->collider.base.acFlags &= ~AC_HIT;
        gPacket.didDamage = this->actor.colChkInfo.damageEffect;

        if (gPacket.didDamage == PUPPET_DMGEFF_NORMAL) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_CUTBODY);
        } else if (gPacket.didDamage == PUPPET_DMGEFF_STUN) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EN_GOMA_JR_FREEZE);
            Actor_SetColorFilter(&this->actor, 0, 0xFF, 0, 40);
        }

        this->damageTimer = 18;
    }

    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(globalCtx, &globalCtx->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(globalCtx, &globalCtx->colChkCtx, &this->collider.base);

    if (globalCtx->sceneNum == this->packet.scene_id) {
        this->actor.world.pos = this->packet.posRot.pos;
        this->actor.shape.rot = this->packet.posRot.rot;
    } else {
        this->actor.world.pos.x = -32000.0f;
        this->actor.world.pos.y = -32000.0f;
        this->actor.world.pos.z = -32000.0f;
    }

    if (this->packet.jointTable != NULL) {
        this->linkSkeleton.jointTable = this->packet.jointTable;
    }
}

Vec3f FEET_POS[] = {
    { 200.0f, 300.0f, 0.0f },
    { 200.0f, 200.0f, 0.0f },
};

extern Gfx** sPlayerDListGroups[];
extern Gfx* D_80125D28[];

s32 Puppet_OverrideLimbDraw(GlobalContext* globalCtx, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    LinkPuppet* this = (LinkPuppet*)thisx;

    if (limbIndex == PLAYER_LIMB_SHEATH) {

        Gfx** dLists = &sPlayerDListGroups[this->packet.sheathType][(void)0, gSaveContext.linkAge];
        if ((this->packet.sheathType == 18) || (this->packet.sheathType == 19)) {
            dLists += this->packet.shieldType * 4;
        }
        *dList = ResourceMgr_LoadGfxByName(dLists[0]);

    } else if (limbIndex == PLAYER_LIMB_L_HAND) {

        Gfx** dLists = &sPlayerDListGroups[this->packet.leftHandType][(void)0, gSaveContext.linkAge];
        if ((this->packet.leftHandType == 4) && this->packet.biggoron_broken) {
            dLists += 4;
        }
        *dList = ResourceMgr_LoadGfxByName(dLists[0]);

    } else if (limbIndex == PLAYER_LIMB_R_HAND) {

        Gfx** dLists = &sPlayerDListGroups[this->packet.rightHandType][(void)0, gSaveContext.linkAge];
        if (this->packet.rightHandType == 10) {
            dLists += this->packet.shieldType * 4;
        }
        *dList = ResourceMgr_LoadGfxByName(dLists[0]);
    }

    return false;
}

void Puppet_PostLimbDraw(GlobalContext* globalCtx, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    LinkPuppet* this = (LinkPuppet*)thisx;

    Vec3f* vec = &FEET_POS[((void)0, gSaveContext.linkAge)];
    Actor_SetFeetPos(&this->actor, limbIndex, PLAYER_LIMB_L_FOOT, vec, PLAYER_LIMB_R_FOOT, vec);
}

void LinkPuppet_Draw(Actor* thisx, GlobalContext* globalCtx) {
    LinkPuppet* this = (LinkPuppet*)thisx;

    u8 sp12C[2];

    sp12C[0] = 0;
    sp12C[1] = 0;

    func_8008F470(globalCtx, this->linkSkeleton.skeleton, this->linkSkeleton.jointTable, this->linkSkeleton.dListCount,
                  0, this->packet.tunicType, this->packet.bootsType, this->packet.faceType, Puppet_OverrideLimbDraw, Puppet_PostLimbDraw, this);
}