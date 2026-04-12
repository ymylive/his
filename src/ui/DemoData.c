/**
 * @file DemoData.c
 * @brief 演示数据模块实现 - 提供演示数据重置和种子文件路径解析功能
 *
 * 本文件实现了将预设的种子数据文件复制到运行时数据目录的功能，
 * 用于将系统恢复到初始演示状态。
 */

#include "ui/DemoData.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief 演示数据文件路径封装结构体
 *
 * 用于将运行时数据文件路径统一存储在数组中，便于批量处理。
 */
typedef struct DemoDataFilePath {
    const char *runtime_path;  /**< 运行时数据文件路径 */
} DemoDataFilePath;

/**
 * @brief 安全复制字符串
 * @param destination 目标缓冲区
 * @param capacity    目标缓冲区容量
 * @param source      源字符串（可为 NULL）
 * @return 1 表示成功，0 表示失败（参数无效）
 */
static int DemoData_copy_text(char *destination, size_t capacity, const char *source) {
    if (destination == 0 || capacity == 0) {
        return 0;
    }

    if (source == 0) {
        destination[0] = '\0';
        return 1;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
    return 1;
}

/**
 * @brief 查找路径中最后一个目录分隔符的位置
 *
 * 同时支持正斜杠(/)和反斜杠(\)，适配 Unix 和 Windows 路径。
 *
 * @param path 文件路径字符串
 * @return 最后一个分隔符的指针，未找到时返回 NULL
 */
static const char *DemoData_find_last_separator(const char *path) {
    const char *last_slash = 0;
    const char *last_backslash = 0;

    if (path == 0) {
        return 0;
    }

    last_slash = strrchr(path, '/');
    last_backslash = strrchr(path, '\\');
    if (last_slash == 0) {
        return last_backslash;
    }
    if (last_backslash == 0) {
        return last_slash;
    }
    return last_slash > last_backslash ? last_slash : last_backslash;
}

Result DemoData_resolve_seed_path(
    const char *runtime_path,
    char *seed_path,
    size_t seed_path_capacity
) {
    const char *separator = 0;
    size_t prefix_length = 0;
    const char *file_name = 0;
    int written = 0;

    if (runtime_path == 0 || seed_path == 0 || seed_path_capacity == 0) {
        return Result_make_failure("demo data path arguments missing");
    }

    /* 查找路径中最后一个目录分隔符 */
    separator = DemoData_find_last_separator(runtime_path);
    if (separator == 0) {
        return Result_make_failure("demo data runtime path invalid");
    }

    /* 计算目录前缀长度和文件名 */
    prefix_length = (size_t)(separator - runtime_path);
    file_name = separator + 1;
    /* 构造种子路径：在父目录下插入 "demo_seed" 子目录 */
    written = snprintf(
        seed_path,
        seed_path_capacity,
        "%.*s/demo_seed/%s",
        (int)prefix_length,
        runtime_path,
        file_name
    );
    if (written < 0 || (size_t)written >= seed_path_capacity) {
        seed_path[0] = '\0';
        return Result_make_failure("demo data seed path too long");
    }

    return Result_make_success("demo data seed path ready");
}

/**
 * @brief 复制文件内容
 *
 * 以二进制模式读取源文件，写入目标文件。使用 4096 字节缓冲区分块传输。
 *
 * @param source_path      源文件路径
 * @param destination_path 目标文件路径
 * @return Result          成功或失败的结果
 */
static Result DemoData_copy_file(const char *source_path, const char *destination_path) {
    FILE *source = 0;
    FILE *destination = 0;
    char buffer[4096];
    size_t read_bytes = 0;

    if (source_path == 0 || destination_path == 0) {
        return Result_make_failure("demo data copy arguments missing");
    }

    source = fopen(source_path, "rb");
    if (source == 0) {
        return Result_make_failure("demo seed file missing");
    }

    destination = fopen(destination_path, "wb");
    if (destination == 0) {
        fclose(source);
        return Result_make_failure("demo runtime file not writable");
    }

    while ((read_bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, read_bytes, destination) != read_bytes) {
            fclose(source);
            fclose(destination);
            return Result_make_failure("demo runtime file write failed");
        }
    }

    fclose(source);
    fclose(destination);
    return Result_make_success("demo data file copied");
}

/**
 * @brief 重置所有演示数据
 *
 * 遍历所有数据文件路径，为每个文件推导对应的种子文件路径，
 * 然后将种子文件复制覆盖运行时文件，实现数据重置。
 */
Result DemoData_reset(const MenuApplicationPaths *paths, char *buffer, size_t capacity) {
    /* 将所有数据文件路径收集到数组中，便于统一遍历处理 */
    DemoDataFilePath files[] = {
        { paths != 0 ? paths->user_path : 0 },
        { paths != 0 ? paths->patient_path : 0 },
        { paths != 0 ? paths->department_path : 0 },
        { paths != 0 ? paths->doctor_path : 0 },
        { paths != 0 ? paths->registration_path : 0 },
        { paths != 0 ? paths->visit_path : 0 },
        { paths != 0 ? paths->examination_path : 0 },
        { paths != 0 ? paths->ward_path : 0 },
        { paths != 0 ? paths->bed_path : 0 },
        { paths != 0 ? paths->admission_path : 0 },
        { paths != 0 ? paths->medicine_path : 0 },
        { paths != 0 ? paths->dispense_record_path : 0 },
        { paths != 0 ? paths->prescription_path : 0 },
        { paths != 0 ? paths->inpatient_order_path : 0 },
        { paths != 0 ? paths->nursing_record_path : 0 },
        { paths != 0 ? paths->round_record_path : 0 }
    };
    char seed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    size_t index = 0;

    if (buffer != 0 && capacity > 0) {
        buffer[0] = '\0';
    }

    if (paths == 0) {
        return Result_make_failure("demo data paths missing");
    }

    /* 逐一处理每个数据文件：推导种子路径 -> 复制种子文件到运行时路径 */
    for (index = 0; index < sizeof(files) / sizeof(files[0]); index++) {
        /* 跳过未配置的可选路径 */
        if (files[index].runtime_path == 0 || files[index].runtime_path[0] == '\0') {
            continue;
        }
        /* 第一步：推导种子文件路径 */
        Result result = DemoData_resolve_seed_path(
            files[index].runtime_path,
            seed_path,
            sizeof(seed_path)
        );
        if (result.success == 0) {
            DemoData_copy_text(buffer, capacity, result.message);
            return result;
        }

        /* 第二步：将种子文件复制到运行时路径 */
        result = DemoData_copy_file(seed_path, files[index].runtime_path);
        if (result.success == 0) {
            if (buffer != 0 && capacity > 0) {
                snprintf(buffer, capacity, "%s: %s", result.message, seed_path);
            }
            return result;
        }
    }

    if (buffer != 0 && capacity > 0) {
        DemoData_copy_text(buffer, capacity, "演示数据已重置");
    }

    return Result_make_success("demo data reset complete");
}
