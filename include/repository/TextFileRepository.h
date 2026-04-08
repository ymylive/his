/**
 * @file TextFileRepository.h
 * @brief 文本文件仓储基础层头文件
 *
 * 提供基于文本文件的通用数据存储功能，是所有具体仓储（如 PatientRepository、
 * DoctorRepository 等）的底层存储引擎。
 *
 * 主要功能：
 * - 文件初始化与路径管理
 * - 确保存储文件及其父目录存在
 * - 逐行遍历文件内容（支持跳过表头）
 * - 追加写入单行数据
 * - 全量覆盖保存文件内容（通过临时文件原子替换）
 *
 * 文件存储格式：每行一条记录，字段间以管道符 '|' 分隔，
 * 第一行通常为表头（列名）。
 */

#ifndef HIS_REPOSITORY_TEXT_FILE_REPOSITORY_H
#define HIS_REPOSITORY_TEXT_FILE_REPOSITORY_H

#include "common/Result.h"

/** 文件路径的最大长度（含终止符） */
#define TEXT_FILE_REPOSITORY_PATH_CAPACITY 512

/** 单行数据的最大长度（含终止符） */
#define TEXT_FILE_REPOSITORY_LINE_CAPACITY 1024

/**
 * @brief 文本文件仓储结构体
 *
 * 封装了文件路径和初始化状态，作为所有具体仓储的底层存储组件。
 */
typedef struct TextFileRepository {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY]; /**< 数据文件的完整路径 */
    int initialized;                                /**< 文件是否已确认存在（0=未确认，1=已确认） */
} TextFileRepository;

/**
 * @brief 行处理回调函数类型
 *
 * 在遍历文件每一行时被调用。返回 failure 结果将终止遍历。
 *
 * @param line    当前行的文本内容（已剥离换行符）
 * @param context 调用方传入的上下文指针
 * @return 返回 success 继续遍历；返回 failure 终止遍历
 */
typedef Result (*TextFileRepositoryLineHandler)(const char *line, void *context);

/**
 * @brief 初始化文本文件仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；路径为空或过长时返回 failure
 */
Result TextFileRepository_init(TextFileRepository *repository, const char *path);

/**
 * @brief 确保数据文件存在
 *
 * 如果文件不存在，则自动创建父目录和空文件。
 * 内部会缓存状态，重复调用不会重复创建。
 *
 * @param repository 仓储实例指针
 * @return 成功返回 success；创建失败时返回 failure
 */
Result TextFileRepository_ensure_file_exists(const TextFileRepository *repository);

/**
 * @brief 遍历文件中所有非空行
 *
 * 跳过空行，对每一个非空行调用 line_handler 回调。
 * 不会跳过表头行。
 *
 * @param repository   仓储实例指针
 * @param line_handler 行处理回调函数
 * @param context      传递给回调的上下文指针
 * @return 遍历完成返回 success；读取错误或回调返回 failure 时返回 failure
 */
Result TextFileRepository_for_each_non_empty_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
);

/**
 * @brief 遍历文件中的数据行（跳过表头）
 *
 * 跳过空行和第一个非空行（视为表头），对后续每一行调用 line_handler 回调。
 *
 * @param repository   仓储实例指针
 * @param line_handler 行处理回调函数
 * @param context      传递给回调的上下文指针
 * @return 遍历完成返回 success；读取错误或回调返回 failure 时返回 failure
 */
Result TextFileRepository_for_each_data_line(
    const TextFileRepository *repository,
    TextFileRepositoryLineHandler line_handler,
    void *context
);

/**
 * @brief 向文件末尾追加一行数据
 *
 * 如果 line 末尾没有换行符，会自动补充。
 *
 * @param repository 仓储实例指针
 * @param line       要追加的行文本
 * @return 成功返回 success；写入失败时返回 failure
 */
Result TextFileRepository_append_line(
    const TextFileRepository *repository,
    const char *line
);

/**
 * @brief 全量覆盖保存文件内容
 *
 * 使用临时文件写入后原子替换原文件，确保写入过程中断电等异常不会损坏数据。
 *
 * @param repository 仓储实例指针
 * @param content    要写入的完整文件内容
 * @return 成功返回 success；写入或替换失败时返回 failure
 */
Result TextFileRepository_save_file(
    const TextFileRepository *repository,
    const char *content
);

#endif
