/**
 * @file Bed.c
 * @brief 床位领域模型实现文件
 *
 * 实现床位(Bed)的业务操作函数，包括可分配性检查、
 * 分配患者、释放床位、标记维护和恢复可用等操作。
 * 所有操作均包含前置条件校验，确保床位状态转换的合法性。
 */

#include "domain/Bed.h"

#include <string.h>

/**
 * @brief 安全拷贝字符串（内部辅助函数）
 *
 * 将源字符串安全地拷贝到指定容量的目标缓冲区，确保不会发生缓冲区溢出。
 * 如果源字符串为空指针，则将目标缓冲区清空。
 *
 * @param destination 目标缓冲区指针
 * @param capacity    目标缓冲区容量（字节数）
 * @param source      源字符串指针
 */
static void Bed_copy_text(char *destination, size_t capacity, const char *source) {
    /* 目标缓冲区为空或容量为0，无法写入，直接返回 */
    if (destination == 0 || capacity == 0) {
        return;
    }

    /* 源字符串为空，将目标缓冲区置为空字符串 */
    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    /* 使用 strncpy 安全拷贝，预留最后一个字节用于终止符 */
    strncpy(destination, source, capacity - 1);
    /* 确保字符串以 '\0' 结尾，防止溢出 */
    destination[capacity - 1] = '\0';
}

/**
 * @brief 检查床位是否可分配给患者
 *
 * 床位必须处于"空闲"状态且当前无关联的入院记录才视为可分配。
 *
 * @param bed 指向床位的常量指针
 * @return int 可分配返回1，不可分配返回0
 */
int Bed_is_assignable(const Bed *bed) {
    /* 床位指针为空或状态非空闲，不可分配 */
    if (bed == 0 || bed->status != BED_STATUS_AVAILABLE) {
        return 0;
    }

    /* 检查当前入院记录ID是否为空（无关联入院记录才可分配） */
    return bed->current_admission_id[0] == '\0';
}

/**
 * @brief 为床位分配患者（入院占用）
 *
 * 将床位状态设为"已占用"，记录入院ID和占用时间，清空释放时间。
 *
 * @param bed          指向床位的指针
 * @param admission_id 入院记录ID
 * @param occupied_at  占用时间字符串
 * @return int 成功返回1，失败返回0
 */
int Bed_assign(Bed *bed, const char *admission_id, const char *occupied_at) {
    /* 参数校验：床位指针、入院ID、占用时间均不能为空或空字符串 */
    /* 业务校验：床位必须处于可分配状态 */
    if (bed == 0 || admission_id == 0 || admission_id[0] == '\0' ||
        occupied_at == 0 || occupied_at[0] == '\0' || !Bed_is_assignable(bed)) {
        return 0;
    }

    /* 将床位状态设为已占用 */
    bed->status = BED_STATUS_OCCUPIED;
    /* 记录当前关联的入院记录ID */
    Bed_copy_text(bed->current_admission_id, sizeof(bed->current_admission_id), admission_id);
    /* 记录占用时间 */
    Bed_copy_text(bed->occupied_at, sizeof(bed->occupied_at), occupied_at);
    /* 清空释放时间（正在占用中，尚未释放） */
    bed->released_at[0] = '\0';
    return 1;
}

/**
 * @brief 释放床位（出院/转床）
 *
 * 将床位状态恢复为"空闲"，清除入院关联信息和占用时间，记录释放时间。
 *
 * @param bed         指向床位的指针
 * @param released_at 释放时间字符串
 * @return int 成功返回1，失败返回0
 */
int Bed_release(Bed *bed, const char *released_at) {
    /* 参数校验：床位指针和释放时间非空，且释放时间不为空字符串 */
    /* 状态校验：仅"已占用"状态的床位才可释放 */
    if (bed == 0 || released_at == 0 || released_at[0] == '\0' ||
        bed->status != BED_STATUS_OCCUPIED) {
        return 0;
    }

    /* 恢复床位状态为空闲 */
    bed->status = BED_STATUS_AVAILABLE;
    /* 清除关联的入院记录ID */
    bed->current_admission_id[0] = '\0';
    /* 清除占用时间 */
    bed->occupied_at[0] = '\0';
    /* 记录释放时间 */
    Bed_copy_text(bed->released_at, sizeof(bed->released_at), released_at);
    return 1;
}

/**
 * @brief 将床位标记为维护状态
 *
 * 已占用的床位不能进入维护状态（需先释放）。
 * 进入维护状态后清除所有关联信息和时间记录。
 *
 * @param bed 指向床位的指针
 * @return int 成功返回1，失败返回0
 */
int Bed_mark_maintenance(Bed *bed) {
    /* 校验：床位指针有效且非占用状态（占用中的床位不可维护） */
    if (bed == 0 || bed->status == BED_STATUS_OCCUPIED) {
        return 0;
    }

    /* 将床位状态设为维护中 */
    bed->status = BED_STATUS_MAINTENANCE;
    /* 清除所有关联信息 */
    bed->current_admission_id[0] = '\0';
    bed->occupied_at[0] = '\0';
    bed->released_at[0] = '\0';
    return 1;
}

/**
 * @brief 将床位从维护状态恢复为可用状态
 *
 * 仅当床位处于维护状态时才可恢复为空闲。
 *
 * @param bed 指向床位的指针
 * @return int 成功返回1，失败返回0
 */
int Bed_mark_available(Bed *bed) {
    /* 校验：床位指针有效且处于维护状态 */
    if (bed == 0 || bed->status != BED_STATUS_MAINTENANCE) {
        return 0;
    }

    /* 恢复床位状态为空闲可用 */
    bed->status = BED_STATUS_AVAILABLE;
    return 1;
}
