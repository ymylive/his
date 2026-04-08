/**
 * @file Department.h
 * @brief 科室领域模型头文件
 *
 * 定义了科室(Department)的数据结构。
 * 科室是医院的组织单元，医生隶属于特定科室，
 * 患者挂号时需要选择就诊科室。
 */

#ifndef HIS_DOMAIN_DEPARTMENT_H
#define HIS_DOMAIN_DEPARTMENT_H

#include "domain/DomainTypes.h"

/**
 * @brief 科室信息结构体
 *
 * 存储科室的基本信息，包括名称、位置和描述。
 */
typedef struct Department {
    char department_id[HIS_DOMAIN_ID_CAPACITY];       /* 科室唯一标识ID */
    char name[HIS_DOMAIN_NAME_CAPACITY];               /* 科室名称（如内科、外科等） */
    char location[HIS_DOMAIN_LOCATION_CAPACITY];       /* 科室所在位置（如门诊楼3层） */
    char description[HIS_DOMAIN_TEXT_CAPACITY];         /* 科室描述信息 */
} Department;

#endif
