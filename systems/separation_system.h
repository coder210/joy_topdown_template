#ifndef SEPARATION_SYSTEM_H
#define SEPARATION_SYSTEM_H

#include "../components/enemy_component.h"
#include "../components/health_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include <joy/jmath.h>

struct Context;

// 敌人之间的分离：防止多个敌人追到相近位置时完全叠在一起。
//
// 为什么需要：
//   movement_system 只移动玩家；敌人只在 pathfinding_system 里沿 A* 路径移动，
//   且到达格点时直接吸附到格点。多个敌人追同一个玩家格子时会吸附到同一格，
//   而 collision_system 只处理"玩家/敌人 vs 墙"，没有敌人之间的碰撞 → 直接重叠。
//
// 做法：对所有存活敌人两两做 AABB 相交检测，沿"最小渗透轴"把重叠量对半推开。
//   - 纯定点数（fp_t）+ 对称推开，两端同状态同顺序 → 结果逐位一致（帧同步安全）。
//   - 只改 LogicPositionComponent（逻辑位置），不引入任何跨端不一致的状态。
//   - 放在 movement 之后、collision 之前：万一被推进墙里，随后的 collision_system
//     会按最小渗透方向再把它推出来。
//
// 说明：同时进入攻击状态（不再走 pathfinding）的敌人被推开后会稳定停住，
//       不会与路径吸引力来回抖动；仍在追击的敌人可能有很小幅度的收敛/推开来回，
//       但不会再完全重叠。
//
// 注：存活敌人数由 MAINTAIN_ENEMY_COUNT 控制（约 10 个），两两检测 O(n²) 可忽略。
static void separation_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    (void)ctx;
    auto view = registry.view<EnemyComponent, LogicPositionComponent, LogicRectComponent>();

    // 收集到固定数组（view 遍历顺序确定，两端一致），避免嵌套循环里反复查组件
    const int MAX_E = 256;
    entt::entity ents[MAX_E];
    fp_t px[MAX_E], py[MAX_E], pw[MAX_E], ph[MAX_E];
    int n = 0;

    for (auto e : view) {
        if (n >= MAX_E) break;
        // 已死亡但尚未被销毁的敌人不参与分离（避免把尸体推开）
        if (registry.all_of<HealthComponent>(e) && registry.get<HealthComponent>(e).dead) continue;

        auto& p = view.get<LogicPositionComponent>(e);
        auto& r = view.get<LogicRectComponent>(e);
        ents[n] = e;
        px[n] = p.x;
        py[n] = p.y;
        pw[n] = r.width;
        ph[n] = r.height;
        n++;
    }

    const fp_t two = fp_from_int(2);

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            fp_t ax0 = px[i], ay0 = py[i];
            fp_t ax1 = fp_add(ax0, pw[i]), ay1 = fp_add(ay0, ph[i]);
            fp_t bx0 = px[j], by0 = py[j];
            fp_t bx1 = fp_add(bx0, pw[j]), by1 = fp_add(by0, ph[j]);

            // 两 AABB 的重叠量；任一轴 <= 0 表示不相交（仅贴边不算）
            fp_t ox = fp_sub(fp_min(ax1, bx1), fp_max(ax0, bx0));
            fp_t oy = fp_sub(fp_min(ay1, by1), fp_max(ay0, by0));
            if (ox <= fp_zero() || oy <= fp_zero()) continue;

            // 沿穿透更浅的轴推开，各推一半 → 双方对等分离
            if (ox < oy) {
                fp_t half = fp_div(ox, two);
                fp_t acx = fp_add(ax0, fp_div(pw[i], two));
                fp_t bcx = fp_add(bx0, fp_div(pw[j], two));
                if (bcx >= acx) { px[i] = fp_sub(px[i], half); px[j] = fp_add(px[j], half); }
                else            { px[i] = fp_add(px[i], half); px[j] = fp_sub(px[j], half); }
            } else {
                fp_t half = fp_div(oy, two);
                fp_t acy = fp_add(ay0, fp_div(ph[i], two));
                fp_t bcy = fp_add(by0, fp_div(ph[j], two));
                if (bcy >= acy) { py[i] = fp_sub(py[i], half); py[j] = fp_add(py[j], half); }
                else            { py[i] = fp_add(py[i], half); py[j] = fp_sub(py[j], half); }
            }
        }
    }

    // 写回逻辑位置
    for (int k = 0; k < n; k++) {
        auto& p = registry.get<LogicPositionComponent>(ents[k]);
        p.x = px[k];
        p.y = py[k];
    }
}

#endif // SEPARATION_SYSTEM_H
