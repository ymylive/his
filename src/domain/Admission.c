/**
 * @file Admission.c
 * @brief 入院记录领域模型实现文件
 *
 * 实现入院记录(Admission)的业务操作函数，
 * 包括检查住院状态和办理出院。
 * 所有操作均包含前置条件校验，确保状态转换的合法性。
 */

#include "domain/Admission.h"

#include <string.h>

/**
 * @brief 安全拷贝时间字符串（内部辅助函数）
 *
 * 将源时间字符串安全地拷贝到目标缓冲区，确保不会发生缓冲区溢出。
 * 如果源字符串为空指针，则将目标缓冲区清空。
 *
 * @param destination 目标缓冲区指针
 * @param source      源时间字符串指针
 */
static void Admission_copy_time(char *destination, const char *source) {
    /* 目标缓冲区为空，无法写入，直接返回 */
    if (destination == 0) {
        return;
    }

    /* 源字符串为空，将目标缓冲区置为空字符串 */
    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    /* 使用 strncpy 安全拷贝，预留最后一个字节用于终止符 */
    strncpy(destination, source, HIS_DOMAIN_TIME_CAPACITY - 1);
    /* 确保字符串以 '\0' 结尾，防止溢出 */
    destination[HIS_DOMAIN_TIME_CAPACITY - 1] = '\0';
}

/**
 * @brief 检查入院记录是否处于住院中状态
 *
 * @param admission 指向入院记录的常量指针
 * @return int 住院中返回1，已出院或参数无效返回0
 */
int Admission_is_active(const Admission *admission) {
    /* 指针非空且状态为住院中时返回1 */
    return admission != 0 && admission->status == ADMISSION_STATUS_ACTIVE;
}

/**
 * @brief 办理出院
 *
 * 将入院记录状态从"住院中"变更为"已出院"，并记录出院时间。
 * 仅当入院记录处于住院中状态时才可办理出院。
 *
 * @param admission     指向入院记录的指针
 * @param discharged_at 出院时间字符串
 * @return int 成功返回1，失败返回0
 */
int Admission_discharge(Admission *admission, const char *discharged_at) {
    /* 校验：入院记录必须处于住院中状态，且出院时间非空 */
    if (!Admission_is_active(admission) || discharged_at == 0 || discharged_at[0] == '\0') {
        return 0;
    }

    /* 将状态更新为已出院 */
    admission->status = ADMISSION_STATUS_DISCHARGED;
    /* 记录出院时间 */
    Admission_copy_time(admission->discharged_at, discharged_at);
    return 1;
}
