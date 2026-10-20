#ifndef ENEMY_COUNTER_HUD_PASS_H
#define ENEMY_COUNTER_HUD_PASS_H

#include <stdio.h>
#include <string.h>
#include <joy/joy.h>
#include "../components/enemy_component.h"
#include "../components/health_component.h"
#include "../components/global_state_component.h"
#include "../systems/spawn_def.h"   // level_enemy_total

// 右上角常驻 HUD：剩余敌人 / 本关总数。
//   位置在暂停按钮正下方、右对齐，避开视野中心；左上角只留给 F1 调试 HUD。
//   剩余 = 尚未刷出的配额（enemy_quota_remaining）+ 场上存活；
//   总数 = 本关预设敌人总数（spawn_def.h 的 LEVEL_ENEMY_TOTALS，双端一致）。
// 纯展示，不参与帧同步；单机/Host/客户端都显示，独立服务端不显示。
static void enemy_counter_hud_pass(entt::registry& ecs, jr2d_p r2d, font_p font)
{
    if (!r2d || !font) return;
    if (!ecs.ctx().contains<GlobalState>()) return;

    auto& gs = ecs.ctx().get<GlobalState>();
    const int total = level_enemy_total(gs.level);

    int alive = 0;
    auto view = ecs.view<EnemyComponent, HealthComponent>();
    for (auto e : view) {
        if (view.get<HealthComponent>(e).hp > 0) alive++;
    }

    // 权威端：配额 + 存活 = 还需要击杀的数量。
    // 客户端若与服务器在同一 tick 推进补兵，配额同样有效；配额异常时至少回退到存活数。
    int left = gs.enemy_quota_remaining + alive;
    if (left < 0) left = 0;
    if (left > total) left = total;

    char buf[64];
    snprintf(buf, sizeof(buf), "剩余 %d / %d", left, total);

    int lw = 0, lh = 0;
    r2d_get_logical_presentation(r2d, &lw, &lh, NULL);
    if (lw <= 0 || lh <= 0) return;
    const float s = (float)lh / 320.0f;   // 只用高度缩放，和游戏内其它 HUD 一致

    // 剩余数量少时用朱砂色，提醒快清完了
    jcolor_t text_c = (left <= 10) ? jcolor_t{ 190, 50, 40, 255 }
                                   : jcolor_t{ 40, 40, 45, 255 };

    int tw = 0, th = 0;
    if (!jtext_measurex(r2d, font, buf, (int)strlen(buf), text_c, &tw, &th)) return;

    const float pad_x = 8.0f * s;
    const float pad_y = 3.0f * s;
    const float w = (float)tw + pad_x * 2.0f;
    const float h = (float)th + pad_y * 2.0f;

    // 暂停按钮：右上角 36x36，边距 8；敌人纸片放在它正下方，右对齐
    const float margin   = 8.0f * s;
    const float pause_sz = 36.0f * s;
    const float x = (float)lw - w - margin;
    const float y = margin + pause_sz + 8.0f * s;

    // 半透明纸底 + 淡墨边 + 左侧朱砂条（不挡视野）
    r2d_set_color(r2d, 0, 0, 0, 25);
    shape_draw_rectangle(r2d, "fill", { x + 1.5f * s, y + 1.5f * s, w, h });
    r2d_set_color(r2d, 250, 248, 242, 150);
    shape_draw_rectangle(r2d, "fill", { x, y, w, h });
    r2d_set_color(r2d, 70, 70, 80, 150);
    shape_draw_rectangle(r2d, "line", { x, y, w, h });
    r2d_set_color(r2d, 200, 60, 50, 200);
    shape_draw_rectangle(r2d, "fill", { x, y, 3.0f * s, h });

    jtext_printx(r2d, font, buf, (int)strlen(buf), text_c, x + pad_x, y + pad_y);
}

#endif
