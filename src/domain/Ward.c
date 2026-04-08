/**
 * @file Ward.c
 * @brief 病房领域模型实现文件
 *
 * 实现病房(Ward)的业务操作函数，包括容量查询、
 * 床位占用/释放、病房关闭/开放等操作。
 * 所有操作均包含前置条件校验，确保数据一致性。
 */

#include "domain/Ward.h"

/**
 * @brief 检查病房是否还有剩余床位容量
 *
 * 校验条件：病房指针有效、病房处于开放状态、
 * 总容量大于0、已占用数合法且未达到上限。
 *
 * @param ward 指向病房的常量指针
 * @return int 有剩余容量返回1，无剩余返回0
 */
int Ward_has_capacity(const Ward *ward) {
    /* 病房指针为空或病房非开放状态，不可接收患者 */
    if (ward == 0 || ward->status != WARD_STATUS_ACTIVE) {
        return 0;
    }

    /* 总容量或已占用数异常时视为无可用容量 */
    if (ward->capacity <= 0 || ward->occupied_beds < 0) {
        return 0;
    }

    /* 已占用数小于总容量时，表示有剩余床位 */
    return ward->occupied_beds < ward->capacity;
}

/**
 * @brief 占用一个床位
 *
 * 病房已占用床位数加1。仅当病房有剩余容量时才可操作。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0
 */
int Ward_occupy_bed(Ward *ward) {
    /* 校验病房指针有效且有剩余容量 */
    if (ward == 0 || !Ward_has_capacity(ward)) {
        return 0;
    }

    /* 已占用床位数加1 */
    ward->occupied_beds++;
    return 1;
}

/**
 * @brief 释放一个床位
 *
 * 病房已占用床位数减1。仅当已占用数大于0时才可操作。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0
 */
int Ward_release_bed(Ward *ward) {
    /* 校验病房指针有效且有已占用的床位可释放 */
    if (ward == 0 || ward->occupied_beds <= 0) {
        return 0;
    }

    /* 已占用床位数减1 */
    ward->occupied_beds--;
    return 1;
}

/**
 * @brief 关闭病房
 *
 * 仅当病房处于开放状态且无患者占用床位时才可关闭。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0
 */
int Ward_close(Ward *ward) {
    /* 校验：病房必须为开放状态且所有床位均已空闲 */
    if (ward == 0 || ward->status != WARD_STATUS_ACTIVE || ward->occupied_beds != 0) {
        return 0;
    }

    /* 将病房状态设为关闭 */
    ward->status = WARD_STATUS_CLOSED;
    return 1;
}

/**
 * @brief 重新开放病房
 *
 * 仅当病房处于关闭状态时才可重新开放。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0
 */
int Ward_open(Ward *ward) {
    /* 校验：病房必须处于关闭状态才可重新开放 */
    if (ward == 0 || ward->status != WARD_STATUS_CLOSED) {
        return 0;
    }

    /* 将病房状态设为开放 */
    ward->status = WARD_STATUS_ACTIVE;
    return 1;
}
