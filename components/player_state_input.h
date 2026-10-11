#ifndef PLAYER_STATE_INPUT_H
#define PLAYER_STATE_INPUT_H
#include <joy/jmath.h>

#pragma pack(push, 1)  // 确保内存紧凑，方便未来直接用于网络序列化
struct PlayerStateInput {
    // 1. 移动状态（位掩码，支持组合键）
    // bit0=W(前), bit1=S(后), bit2=A(左), bit3=D(右)
    uint8_t move_bits;

    // 2. 精确视角（量化存储，节省带宽）
    // 如果只是2D俯视角游戏，这里可以存“朝向角度”或“面朝方向索引”
    int16_t yaw_quantized;   // 水平旋转（-180°~180° 映射到 int16）
    int16_t pitch_quantized; // 垂直旋转（-90°~90° 映射到 int16）

    // 3. 辅助构造（方便调试）
    PlayerStateInput() : move_bits(0), yaw_quantized(0), pitch_quantized(0) {}
    PlayerStateInput(uint8_t m, int16_t y = 0, int16_t p = 0)
        : move_bits(m), yaw_quantized(y), pitch_quantized(p) {
    }
};
#pragma pack(pop)

#endif
