#include "CapeAndScarf.h"

#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/SohGui/UIWidgets.hpp"
#include "soh/SohGui/SohGui.hpp"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/cvar_prefixes.h"
#include "z64player.h"
#include "overlays/actors/ovl_En_Ganon_Mant/z_en_ganon_mant.h"

extern PlayState* gPlayState;
}

#define CVAR_CAPE_STYLE    CVAR_COSMETIC("DefaultCapeType")
#define CVAR_CAPE_LENGTH   CVAR_COSMETIC("Link.Cape.Length.Value")
#define CVAR_CAPE_SHOULDER CVAR_COSMETIC("Link.Cape.Shoulder.Value")
#define CVAR_CAPE_BACKPUSH CVAR_COSMETIC("Link.Cape.Backpush.Value")
#define CVAR_CAPE_SWAY     CVAR_COSMETIC("Link.Cape.Sway.Value")
#define CVAR_CAPE_GRAVITY  CVAR_COSMETIC("Link.Cape.Gravity.Value")

static const char* capeTypes[4] = { "None", "Cape", "Scarf", "Hips" };
static EnGanonMant* sCape = nullptr;

static void SpawnLinkCape(Player* player) {
    if (sCape != nullptr || gPlayState == nullptr || player == nullptr) {
        return;
    }
    sCape = (EnGanonMant*)Actor_SpawnAsChild(&gPlayState->actorCtx, &player->actor, gPlayState,
                                             ACTOR_EN_GANON_MANT, 0.0f, 0.0f, 0.0f, 0, 0, 0, 1);
}

static void UpdateLinkCape() {
    if (gPlayState == nullptr) {
        return;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }

    int32_t style = CVarGetInteger(CVAR_CAPE_STYLE, CAPE_NONE);

    if (style == CAPE_NONE) {
        if (sCape != nullptr) {
            Actor_Kill(&sCape->actor);
            sCape = nullptr;
        }
        return;
    }

    // Lazy spawn on scene entry and on mid-scene None -> style toggles.
    if (sCape == nullptr) {
        SpawnLinkCape(player);
        if (sCape == nullptr) {
            return;
        }
    }

    EnGanonMant* cape = sCape;

    // Tuning (backSwayMagnitude is intentionally hard-coded to 0; the "Side Sway"
    cape->backPush = CVarGetFloat(CVAR_CAPE_BACKPUSH, -9.0f);
    cape->backSwayMagnitude = 0.0f;
    cape->sideSwayMagnitude = CVarGetFloat(CVAR_CAPE_SWAY, 0.0f);
    cape->minDist = CVarGetFloat(CVAR_CAPE_SHOULDER, 10.0f);
    cape->gravity = CVarGetFloat(CVAR_CAPE_GRAVITY, -2.5f);

    cape->linkJointLength = CVarGetFloat(CVAR_CAPE_LENGTH, 3.5f);

    cape->actor.world.pos = player->actor.world.pos;

    switch (style) {
        case CAPE_CAPE:
            cape->rightForearmPos = player->bodyPartsPos[PLAYER_BODYPART_R_SHOULDER];
            cape->leftForearmPos = player->bodyPartsPos[PLAYER_BODYPART_L_SHOULDER];
            break;
        case CAPE_SCARF:
            cape->rightForearmPos = player->bodyPartsPos[PLAYER_BODYPART_R_SHOULDER];
            cape->leftForearmPos = player->bodyPartsPos[PLAYER_BODYPART_HEAD];
            break;
        case CAPE_HIPS:
            cape->rightForearmPos = player->bodyPartsPos[PLAYER_BODYPART_L_SHIN];
            cape->leftForearmPos = player->bodyPartsPos[PLAYER_BODYPART_TORSO];
            break;
        default:
            return;
    }

    cape->rightForearmPos.y += 2.0f;
    cape->leftForearmPos.y += 2.0f;
    cape->minY = player->actor.world.pos.y - 0.1f;
}

void RegisterCapeAndScarf() {
    COND_HOOK(OnPlayerUpdate, true, [] { UpdateLinkCape(); });

    COND_ID_HOOK(OnActorDestroy, ACTOR_EN_GANON_MANT, true, [](void* refActor) {
        if (sCape != nullptr && refActor == (void*)&sCape->actor) {
            sCape = nullptr;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterCapeAndScarf);


void DrawCapeAndScarfOptions() {
    ImGui::Text("Cape and Scarf");

    UIWidgets::CVarCombobox("Type", CVAR_CAPE_STYLE, capeTypes,
                            UIWidgets::ComboboxOptions()
                                .DefaultIndex(CAPE_NONE)
                                .Color(THEME_COLOR)
                                .LabelPosition(UIWidgets::LabelPositions::Near)
                                .ComponentAlignment(UIWidgets::ComponentAlignments::Right));

    if (UIWidgets::CVarSliderFloat("Length", CVAR_CAPE_LENGTH,
                                   UIWidgets::FloatSliderOptions()
                                       .Format("%.1f")
                                       .Min(0.5f)
                                       .Max(9.5f)
                                       .DefaultValue(3.5f)
                                       .Step(0.1f)
                                       .Size(ImVec2(300.0f, 0.0f))
                                       .Color(THEME_COLOR))) {
        CVarSetInteger(CVAR_COSMETIC("Link.Cape.Length.Changed"), 1);
    }

    if (UIWidgets::CVarSliderFloat("Shoulder Width", CVAR_CAPE_SHOULDER,
                                   UIWidgets::FloatSliderOptions()
                                       .Format("%.1f")
                                       .Min(1.0f)
                                       .Max(20.0f)
                                       .DefaultValue(10.0f)
                                       .Step(0.1f)
                                       .Size(ImVec2(300.0f, 0.0f))
                                       .Color(THEME_COLOR))) {
        CVarSetInteger(CVAR_COSMETIC("Link.Cape.Shoulder.Changed"), 1);
    }

    if (UIWidgets::CVarSliderFloat("Back Push", CVAR_CAPE_BACKPUSH,
                                   UIWidgets::FloatSliderOptions()
                                       .Format("%.1f")
                                       .Min(-10.0f)
                                       .Max(0.0f)
                                       .DefaultValue(-9.0f)
                                       .Step(0.1f)
                                       .Size(ImVec2(300.0f, 0.0f))
                                       .Color(THEME_COLOR))) {
        CVarSetInteger(CVAR_COSMETIC("Link.Cape.Backpush.Changed"), 1);
    }

    if (UIWidgets::CVarSliderFloat("Side Sway Magnitude", CVAR_CAPE_SWAY,
                                   UIWidgets::FloatSliderOptions()
                                       .Format("%.1f")
                                       .Min(-20.0f)
                                       .Max(0.0f)
                                       .DefaultValue(0.0f)
                                       .Step(0.1f)
                                       .Size(ImVec2(300.0f, 0.0f))
                                       .Color(THEME_COLOR))) {
        CVarSetInteger(CVAR_COSMETIC("Link.Cape.Sway.Changed"), 1);
    }

    if (UIWidgets::CVarSliderFloat("Gravity Force", CVAR_CAPE_GRAVITY,
                                   UIWidgets::FloatSliderOptions()
                                       .Format("%.1f")
                                       .Min(-15.0f)
                                       .Max(-0.5f)
                                       .DefaultValue(-2.5f)
                                       .Step(0.1f)
                                       .Size(ImVec2(300.0f, 0.0f))
                                       .Color(THEME_COLOR))) {
        CVarSetInteger(CVAR_COSMETIC("Link.Cape.Gravity.Changed"), 1);
    }

    UIWidgets::Separator(true, true, 2.0f, 2.0f);
}
