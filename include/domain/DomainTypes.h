/**
 * @file DomainTypes.h
 * @brief 领域模型公共类型定义头文件
 *
 * 本文件定义了医院信息系统(HIS)中所有领域模型共用的常量，
 * 包括各类字符串字段的最大容量（缓冲区大小）。
 * 所有领域实体的字符数组长度均引用此处定义的宏常量。
 */

#ifndef HIS_DOMAIN_DOMAIN_TYPES_H
#define HIS_DOMAIN_DOMAIN_TYPES_H

/** ID 字段的最大字符容量（如患者ID、医生ID等） */
#define HIS_DOMAIN_ID_CAPACITY 16

/** 名称字段的最大字符容量（如姓名、药品名等） */
#define HIS_DOMAIN_NAME_CAPACITY 64

/** 职称字段的最大字符容量 */
#define HIS_DOMAIN_TITLE_CAPACITY 64

/** 位置/地址字段的最大字符容量 */
#define HIS_DOMAIN_LOCATION_CAPACITY 64

/** 普通文本字段的最大字符容量（如备注、用法等） */
#define HIS_DOMAIN_TEXT_CAPACITY 128

/** 大文本字段的最大字符容量（如病历摘要、医嘱内容等） */
#define HIS_DOMAIN_LARGE_TEXT_CAPACITY 256

/** 时间字段的最大字符容量（如日期时间字符串） */
#define HIS_DOMAIN_TIME_CAPACITY 32

/** 联系方式字段的最大字符容量（如电话号码、身份证号等） */
#define HIS_DOMAIN_CONTACT_CAPACITY 32

#endif
