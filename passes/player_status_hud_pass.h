#ifndef PLAYER_STATUS_HUD_PASS_H
#define PLAYER_STATUS_HUD_PASS_H

#include <joy/joy.h>
#include "../components/player_component.h"
#include "../components/transform_component.h"
#include "../components/action_state_component.h"
#include "../components/health_component.h"
#include "../asset_manager.h"

static void player_status_hud_pass(entt::registry& ecs, uint32_t local_conv,
        jr2d_p r2d, font_p font)
{
        const char* st = "--";
        const char* dirstr = "-";
        float px = 0.0f, py = 0.0f;
        float hp = -1.0f, max_hp = (float)PLAYER_MAX_HP;
        int respawn = 0;
        bool is_dead = false;
        bool found = false;

        ecs.view<PlayerComponent, TransformComponent, ActionStateComponent>().each(
                [&](auto e, PlayerComponent& pc, TransformComponent& t, ActionStateComponent& ac)
                {
                        if ((uint32_t)pc.conv != local_conv) return;
                        px = t.position.x;
                        py = t.position.y;
                        if (ecs.all_of<HealthComponent>(e)) {
                                auto& hc = ecs.get<HealthComponent>(e);
                                hp = (float)hc.hp;
                                max_hp = (float)hc.max_hp;
                                is_dead = hc.dead;
                                respawn = hc.respawn_timer;
                        }
                        found = true;
                        switch (ac.state) {
                        case ActionStateComponent::IDLE:   st = "IDLE";   break;
                        case ActionStateComponent::WALK:   st = "WALK";   break;
                        case ActionStateComponent::ATTACK: st = "ATTACK"; break;
                        default:                           st = "?";      break;
                        }
                        switch (ac.dir) {
                        case 0: dirstr = "D"; break;
                        case 1: dirstr = "L"; break;
                        case 2: dirstr = "R"; break;
                        case 3: dirstr = "U"; break;
                        default: dirstr = "-"; break;
                        }
                });

        char buf[128];
        if (found) {
                if (is_dead)
                        snprintf(buf, sizeof(buf), "HP: 0/%.0f  POS:(%.1f, %.1f)  ST:DEAD 重生%.1fs",
                                max_hp, px, py, respawn > 0 ? respawn / 20.0f : 0.0f);
                else if (hp >= 0.0f)
                        snprintf(buf, sizeof(buf), "HP: %.0f/%.0f  POS:(%.1f, %.1f)  ST:%s %s",
                                hp, max_hp, px, py, st, dirstr);
                else
                        snprintf(buf, sizeof(buf), "HP: --  POS:(%.1f, %.1f)  ST:%s %s",
                                px, py, st, dirstr);
        }
        else {
                snprintf(buf, sizeof(buf), "HP: --  POS:(--, --)  ST: --");
        }

        jcolor_t c = { 40, 40, 45, 235 };
        // 透明调试 HUD：不铺底，用 1px 白色描边保证亮/暗背景都可读。
        // 位置收紧到 (8,20)，字号由调用方传 font12（小号），不再用 0.5 缩放。
        jcolor_t shadow = { 255, 255, 255, 170 };
        jtext_printf_scaled(r2d, font, buf, (int)strlen(buf), shadow,
                            9.0f / 480.0f, 21.0f / 320.0f, 1.0f);
        jtext_printf_scaled(r2d, font, buf, (int)strlen(buf), c,
                            8.0f / 480.0f, 20.0f / 320.0f, 1.0f);
}

#endif