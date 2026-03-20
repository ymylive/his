#include "repository/InpatientOrderRepository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

typedef struct InpatientOrderRepositoryLoadContext {
    LinkedList *orders;
} InpatientOrderRepositoryLoadContext;

static int InpatientOrderRepository_has_text(const char *text) {
    return text != 0 && text[0] != '\0';
}

static void InpatientOrderRepository_copy_text(
    char *destination,
    size_t capacity,
    const char *source
) {
    if (destination == 0 || capacity == 0) {
        return;
    }

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

static Result InpatientOrderRepository_parse_int(const char *field, int *out_value) {
    char *end_pointer = 0;
    long value = 0;

    if (field == 0 || out_value == 0 || field[0] == '\0') {
        return Result_make_failure("order integer missing");
    }

    value = strtol(field, &end_pointer, 10);
    if (end_pointer == field || end_pointer == 0 || *end_pointer != '\0') {
        return Result_make_failure("order integer invalid");
    }

    *out_value = (int)value;
    return Result_make_success("order integer parsed");
}

static Result InpatientOrderRepository_validate(const InpatientOrder *order) {
    if (order == 0) {
        return Result_make_failure("inpatient order missing");
    }

    if (!InpatientOrderRepository_has_text(order->order_id) ||
        !InpatientOrderRepository_has_text(order->admission_id) ||
        !InpatientOrderRepository_has_text(order->order_type) ||
        !InpatientOrderRepository_has_text(order->content) ||
        !InpatientOrderRepository_has_text(order->ordered_at)) {
        return Result_make_failure("inpatient order required field missing");
    }

    if (!RepositoryUtils_is_safe_field_text(order->order_id) ||
        !RepositoryUtils_is_safe_field_text(order->admission_id) ||
        !RepositoryUtils_is_safe_field_text(order->order_type) ||
        !RepositoryUtils_is_safe_field_text(order->content) ||
        !RepositoryUtils_is_safe_field_text(order->ordered_at) ||
        !RepositoryUtils_is_safe_field_text(order->executed_at) ||
        !RepositoryUtils_is_safe_field_text(order->cancelled_at)) {
        return Result_make_failure("inpatient order field contains reserved character");
    }

    if (order->status < INPATIENT_ORDER_STATUS_PENDING ||
        order->status > INPATIENT_ORDER_STATUS_CANCELLED) {
        return Result_make_failure("inpatient order status invalid");
    }

    if (order->status == INPATIENT_ORDER_STATUS_PENDING &&
        (InpatientOrderRepository_has_text(order->executed_at) ||
         InpatientOrderRepository_has_text(order->cancelled_at))) {
        return Result_make_failure("pending inpatient order has close time");
    }

    if (order->status == INPATIENT_ORDER_STATUS_EXECUTED &&
        (!InpatientOrderRepository_has_text(order->executed_at) ||
         InpatientOrderRepository_has_text(order->cancelled_at))) {
        return Result_make_failure("executed inpatient order invalid");
    }

    if (order->status == INPATIENT_ORDER_STATUS_CANCELLED &&
        (!InpatientOrderRepository_has_text(order->cancelled_at) ||
         InpatientOrderRepository_has_text(order->executed_at))) {
        return Result_make_failure("cancelled inpatient order invalid");
    }

    return Result_make_success("inpatient order valid");
}

static Result InpatientOrderRepository_format_line(
    const InpatientOrder *order,
    char *line,
    size_t line_capacity
) {
    int written = 0;
    Result result = InpatientOrderRepository_validate(order);

    if (result.success == 0) {
        return result;
    }

    if (line == 0 || line_capacity == 0) {
        return Result_make_failure("order line buffer missing");
    }

    written = snprintf(
        line,
        line_capacity,
        "%s|%s|%s|%s|%s|%d|%s|%s",
        order->order_id,
        order->admission_id,
        order->order_type,
        order->content,
        order->ordered_at,
        (int)order->status,
        order->executed_at,
        order->cancelled_at
    );
    if (written < 0 || (size_t)written >= line_capacity) {
        return Result_make_failure("order line too long");
    }

    return Result_make_success("order formatted");
}

static Result InpatientOrderRepository_parse_line(
    const char *line,
    InpatientOrder *order
) {
    char mutable_line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    char *fields[INPATIENT_ORDER_REPOSITORY_FIELD_COUNT];
    size_t field_count = 0;
    int status_value = 0;
    Result result;

    if (line == 0 || order == 0) {
        return Result_make_failure("order line missing");
    }

    InpatientOrderRepository_copy_text(mutable_line, sizeof(mutable_line), line);
    result = RepositoryUtils_split_pipe_line(
        mutable_line,
        fields,
        INPATIENT_ORDER_REPOSITORY_FIELD_COUNT,
        &field_count
    );
    if (result.success == 0) {
        return result;
    }

    result = RepositoryUtils_validate_field_count(
        field_count,
        INPATIENT_ORDER_REPOSITORY_FIELD_COUNT
    );
    if (result.success == 0) {
        return result;
    }

    memset(order, 0, sizeof(*order));
    InpatientOrderRepository_copy_text(order->order_id, sizeof(order->order_id), fields[0]);
    InpatientOrderRepository_copy_text(
        order->admission_id,
        sizeof(order->admission_id),
        fields[1]
    );
    InpatientOrderRepository_copy_text(
        order->order_type,
        sizeof(order->order_type),
        fields[2]
    );
    InpatientOrderRepository_copy_text(order->content, sizeof(order->content), fields[3]);
    InpatientOrderRepository_copy_text(order->ordered_at, sizeof(order->ordered_at), fields[4]);

    result = InpatientOrderRepository_parse_int(fields[5], &status_value);
    if (result.success == 0) {
        return result;
    }
    order->status = (InpatientOrderStatus)status_value;

    InpatientOrderRepository_copy_text(
        order->executed_at,
        sizeof(order->executed_at),
        fields[6]
    );
    InpatientOrderRepository_copy_text(
        order->cancelled_at,
        sizeof(order->cancelled_at),
        fields[7]
    );

    return InpatientOrderRepository_validate(order);
}

static Result InpatientOrderRepository_ensure_header(
    const InpatientOrderRepository *repository
) {
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    FILE *file = 0;
    Result result;

    if (repository == 0) {
        return Result_make_failure("order repository missing");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "r");
    if (file == 0) {
        return Result_make_failure("failed to inspect order repository");
    }

    while (fgets(line, sizeof(line), file) != 0) {
        RepositoryUtils_strip_line_endings(line);
        if (RepositoryUtils_is_blank_line(line)) {
            continue;
        }

        fclose(file);
        if (strcmp(line, INPATIENT_ORDER_REPOSITORY_HEADER) != 0) {
            return Result_make_failure("order repository header mismatch");
        }

        return Result_make_success("order header ready");
    }

    fclose(file);
    return TextFileRepository_append_line(
        &repository->storage,
        INPATIENT_ORDER_REPOSITORY_HEADER
    );
}

static Result InpatientOrderRepository_collect_line(const char *line, void *context) {
    InpatientOrderRepositoryLoadContext *load_context =
        (InpatientOrderRepositoryLoadContext *)context;
    InpatientOrder *order = 0;
    Result result;

    if (load_context == 0 || load_context->orders == 0) {
        return Result_make_failure("order load context missing");
    }

    order = (InpatientOrder *)malloc(sizeof(*order));
    if (order == 0) {
        return Result_make_failure("failed to allocate order");
    }

    result = InpatientOrderRepository_parse_line(line, order);
    if (result.success == 0) {
        free(order);
        return result;
    }

    if (!LinkedList_append(load_context->orders, order)) {
        free(order);
        return Result_make_failure("failed to append order");
    }

    return Result_make_success("order loaded");
}

static int InpatientOrderRepository_validate_list(const LinkedList *orders) {
    const LinkedListNode *outer = 0;

    if (orders == 0) {
        return 0;
    }

    outer = orders->head;
    while (outer != 0) {
        const LinkedListNode *inner = outer->next;
        const InpatientOrder *order = (const InpatientOrder *)outer->data;
        Result result = InpatientOrderRepository_validate(order);

        if (result.success == 0) {
            return 0;
        }

        while (inner != 0) {
            const InpatientOrder *other = (const InpatientOrder *)inner->data;
            if (strcmp(order->order_id, other->order_id) == 0) {
                return 0;
            }

            inner = inner->next;
        }

        outer = outer->next;
    }

    return 1;
}

static InpatientOrder *InpatientOrderRepository_find_in_list(
    LinkedList *orders,
    const char *order_id
) {
    LinkedListNode *current = 0;

    if (orders == 0 || order_id == 0) {
        return 0;
    }

    current = orders->head;
    while (current != 0) {
        InpatientOrder *order = (InpatientOrder *)current->data;
        if (strcmp(order->order_id, order_id) == 0) {
            return order;
        }

        current = current->next;
    }

    return 0;
}

Result InpatientOrderRepository_init(
    InpatientOrderRepository *repository,
    const char *path
) {
    Result result;

    if (repository == 0) {
        return Result_make_failure("order repository missing");
    }

    result = TextFileRepository_init(&repository->storage, path);
    if (result.success == 0) {
        return result;
    }

    return InpatientOrderRepository_ensure_header(repository);
}

Result InpatientOrderRepository_append(
    const InpatientOrderRepository *repository,
    const InpatientOrder *order
) {
    LinkedList orders;
    InpatientOrder *copy = 0;
    Result result = InpatientOrderRepository_validate(order);

    if (result.success == 0) {
        return result;
    }

    result = InpatientOrderRepository_load_all(repository, &orders);
    if (result.success == 0) {
        return result;
    }

    if (InpatientOrderRepository_find_in_list(&orders, order->order_id) != 0) {
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("order id already exists");
    }

    copy = (InpatientOrder *)malloc(sizeof(*copy));
    if (copy == 0) {
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("failed to allocate order copy");
    }

    *copy = *order;
    if (!LinkedList_append(&orders, copy)) {
        free(copy);
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("failed to append order copy");
    }

    result = InpatientOrderRepository_save_all(repository, &orders);
    InpatientOrderRepository_clear_loaded_list(&orders);
    return result;
}

Result InpatientOrderRepository_find_by_id(
    const InpatientOrderRepository *repository,
    const char *order_id,
    InpatientOrder *out_order
) {
    LinkedList orders;
    InpatientOrder *order = 0;
    Result result;

    if (!InpatientOrderRepository_has_text(order_id) || out_order == 0) {
        return Result_make_failure("order query arguments missing");
    }

    result = InpatientOrderRepository_load_all(repository, &orders);
    if (result.success == 0) {
        return result;
    }

    order = InpatientOrderRepository_find_in_list(&orders, order_id);
    if (order == 0) {
        InpatientOrderRepository_clear_loaded_list(&orders);
        return Result_make_failure("order not found");
    }

    *out_order = *order;
    InpatientOrderRepository_clear_loaded_list(&orders);
    return Result_make_success("order found");
}

Result InpatientOrderRepository_load_all(
    const InpatientOrderRepository *repository,
    LinkedList *out_orders
) {
    InpatientOrderRepositoryLoadContext context;
    Result result;

    if (out_orders == 0) {
        return Result_make_failure("order output list missing");
    }

    LinkedList_init(out_orders);
    context.orders = out_orders;

    result = InpatientOrderRepository_ensure_header(repository);
    if (result.success == 0) {
        return result;
    }

    result = TextFileRepository_for_each_data_line(
        &repository->storage,
        InpatientOrderRepository_collect_line,
        &context
    );
    if (result.success == 0) {
        InpatientOrderRepository_clear_loaded_list(out_orders);
        return result;
    }

    return Result_make_success("orders loaded");
}

Result InpatientOrderRepository_save_all(
    const InpatientOrderRepository *repository,
    const LinkedList *orders
) {
    const LinkedListNode *current = 0;
    FILE *file = 0;
    Result result;

    if (repository == 0 || orders == 0) {
        return Result_make_failure("order save arguments missing");
    }

    if (!InpatientOrderRepository_validate_list(orders)) {
        return Result_make_failure("order list invalid");
    }

    result = TextFileRepository_ensure_file_exists(&repository->storage);
    if (result.success == 0) {
        return result;
    }

    file = fopen(repository->storage.path, "w");
    if (file == 0) {
        return Result_make_failure("failed to rewrite order repository");
    }

    if (fprintf(file, "%s\n", INPATIENT_ORDER_REPOSITORY_HEADER) < 0) {
        fclose(file);
        return Result_make_failure("failed to write order header");
    }

    current = orders->head;
    while (current != 0) {
        char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];

        result = InpatientOrderRepository_format_line(
            (const InpatientOrder *)current->data,
            line,
            sizeof(line)
        );
        if (result.success == 0) {
            fclose(file);
            return result;
        }

        if (fprintf(file, "%s\n", line) < 0) {
            fclose(file);
            return Result_make_failure("failed to write order line");
        }

        current = current->next;
    }

    fclose(file);
    return Result_make_success("orders saved");
}

void InpatientOrderRepository_clear_loaded_list(LinkedList *orders) {
    LinkedList_clear(orders, free);
}
