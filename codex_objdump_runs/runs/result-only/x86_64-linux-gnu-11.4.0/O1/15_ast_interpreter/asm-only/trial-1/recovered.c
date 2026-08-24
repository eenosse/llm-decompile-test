#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VALUE_NUMBER = 0,
    VALUE_BOOLEAN = 1,
    VALUE_STRING = 2,
    VALUE_NIL = 3
} ValueKind;

typedef struct {
    uint32_t kind;
    uint32_t padding;
    union {
        double number;
        int32_t boolean;
        struct {
            char *data;
            uint64_t length;
        } string;
    } as;
} Value;

typedef struct Node Node;

typedef enum {
    NODE_LITERAL = 0,
    NODE_IDENTIFIER = 1,
    NODE_UNARY = 2,
    NODE_BINARY = 3,
    NODE_CALL = 4,
    NODE_CONDITIONAL = 5,
    NODE_BLOCK = 6,
    NODE_ASSIGNMENT = 7
} NodeKind;

typedef enum {
    OP_ADD = 0,
    OP_SUBTRACT = 1,
    OP_MULTIPLY = 2,
    OP_DIVIDE = 3,
    OP_LESS = 4,
    OP_GREATER = 5,
    OP_EQUAL = 6,
    OP_AND = 7,
    OP_OR = 8,
    OP_NEGATE = 9,
    OP_NOT = 10,
    OP_CONCATENATE = 11
} Operator;

struct Node {
    uint32_t kind;
    uint32_t serial;
    union {
        Value literal;

        struct {
            int32_t operation;
            uint32_t padding;
            Node *first;
            Node *second;
        } operator_node;

        struct {
            Node *condition;
            Node *when_true;
            Node *when_false;
        } conditional;

        struct {
            char name[16];
            Node *arguments[4];
            uint8_t argument_count;
        } call;

        struct {
            Node *items[8];
            uint8_t count;
        } block;

        struct {
            char name[16];
            Node *expression;
        } named;
    } data;
};

typedef struct {
    char name[16];
    Value value;
} Binding;

typedef struct {
    Binding bindings[16];
    uint32_t count;
    uint32_t evaluations;
} Context;

static uint32_t next_node_serial;

static Value nil_value(void)
{
    Value value = {0};
    value.kind = VALUE_NIL;
    return value;
}

static Node *new_node(NodeKind kind)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    node->kind = (uint32_t)kind;
    node->serial = next_node_serial++;
    return node;
}

static Node *number_node(double number)
{
    Node *node = new_node(NODE_LITERAL);
    node->data.literal.kind = VALUE_NUMBER;
    node->data.literal.as.number = number;
    return node;
}

static Node *string_node(const char *text)
{
    Node *node = new_node(NODE_LITERAL);
    node->data.literal.kind = VALUE_STRING;
    node->data.literal.as.string.data = strdup(text);
    node->data.literal.as.string.length = strlen(text);
    return node;
}

static Node *identifier_node(const char *name)
{
    Node *node = new_node(NODE_IDENTIFIER);
    strncpy(node->data.named.name, name, 15);
    return node;
}

static Node *unary_node(Operator operation, Node *operand)
{
    Node *node = new_node(NODE_UNARY);
    node->data.operator_node.operation = operation;
    node->data.operator_node.first = operand;
    return node;
}

static Node *binary_node(Operator operation, Node *left, Node *right)
{
    Node *node = new_node(NODE_BINARY);
    node->data.operator_node.operation = operation;
    node->data.operator_node.first = left;
    node->data.operator_node.second = right;
    return node;
}

static Node *call_node(const char *name, Node *argument)
{
    Node *node = new_node(NODE_CALL);
    strncpy(node->data.call.name, name, 15);
    node->data.call.arguments[0] = argument;
    node->data.call.argument_count = 1;
    return node;
}

static Node *conditional_node(Node *condition, Node *when_true,
                              Node *when_false)
{
    Node *node = new_node(NODE_CONDITIONAL);
    node->data.conditional.condition = condition;
    node->data.conditional.when_true = when_true;
    node->data.conditional.when_false = when_false;
    return node;
}

static Node *assignment_node(const char *name, Node *expression)
{
    Node *node = new_node(NODE_ASSIGNMENT);
    strncpy(node->data.named.name, name, 15);
    node->data.named.expression = expression;
    return node;
}

static int node_count(const Node *node)
{
    int count = 1;
    unsigned int i;

    switch (node->kind) {
    case NODE_UNARY:
        count += node_count(node->data.operator_node.first);
        break;

    case NODE_BINARY:
        count += node_count(node->data.operator_node.first);
        count += node_count(node->data.operator_node.second);
        break;

    case NODE_CALL:
        for (i = 0; i < node->data.call.argument_count; ++i)
            count += node_count(node->data.call.arguments[i]);
        break;

    case NODE_CONDITIONAL:
        count += node_count(node->data.conditional.condition);
        count += node_count(node->data.conditional.when_true);
        if (node->data.conditional.when_false != NULL)
            count += node_count(node->data.conditional.when_false);
        break;

    case NODE_BLOCK:
        for (i = 0; i < node->data.block.count; ++i)
            count += node_count(node->data.block.items[i]);
        break;

    case NODE_ASSIGNMENT:
        count += node_count(node->data.named.expression);
        break;

    default:
        break;
    }

    return count;
}

static void destroy_node(Node *node)
{
    unsigned int i;

    switch (node->kind) {
    case NODE_LITERAL:
        if (node->data.literal.kind == VALUE_STRING)
            free(node->data.literal.as.string.data);
        break;

    case NODE_UNARY:
        destroy_node(node->data.operator_node.first);
        break;

    case NODE_BINARY:
        destroy_node(node->data.operator_node.first);
        destroy_node(node->data.operator_node.second);
        break;

    case NODE_CALL:
        for (i = 0; i < node->data.call.argument_count; ++i)
            destroy_node(node->data.call.arguments[i]);
        break;

    case NODE_CONDITIONAL:
        destroy_node(node->data.conditional.condition);
        destroy_node(node->data.conditional.when_true);
        if (node->data.conditional.when_false != NULL)
            destroy_node(node->data.conditional.when_false);
        break;

    case NODE_BLOCK:
        for (i = 0; i < node->data.block.count; ++i)
            destroy_node(node->data.block.items[i]);
        break;

    case NODE_ASSIGNMENT:
        destroy_node(node->data.named.expression);
        break;

    default:
        break;
    }

    free(node);
}

static Binding *lookup_binding(Context *context, const char *name)
{
    uint32_t i;

    for (i = 0; i < context->count; ++i) {
        if (strcmp(context->bindings[i].name, name) == 0)
            return &context->bindings[i];
    }

    if (context->count == 16)
        return NULL;

    i = context->count;
    strncpy(context->bindings[i].name, name, 15);
    context->bindings[i].value = nil_value();
    context->count++;
    return &context->bindings[i];
}

static int value_is_true(const Value *value)
{
    switch (value->kind) {
    case VALUE_NUMBER:
        return value->as.number != 0.0;
    case VALUE_BOOLEAN:
        return value->as.boolean;
    case VALUE_STRING:
        return value->as.string.data != NULL;
    default:
        return 0;
    }
}

static void print_value(const Value *value)
{
    switch (value->kind) {
    case VALUE_NUMBER:
        printf("%.2f", value->as.number);
        break;

    case VALUE_BOOLEAN:
        printf("%s", value->as.boolean ? "true" : "false");
        break;

    case VALUE_STRING:
        printf("\"%.*s\"", (int)value->as.string.length,
               value->as.string.data);
        break;

    case VALUE_NIL:
        printf("nil");
        break;

    default:
        break;
    }
}

static Value *evaluate(Value *result, const Node *node, Context *context)
{
    Value left;
    Value right;
    Binding *binding;
    uint32_t i;

    context->evaluations++;

    switch (node->kind) {
    case NODE_LITERAL:
        *result = node->data.literal;
        break;

    case NODE_IDENTIFIER:
        binding = lookup_binding(context, node->data.named.name);
        if (binding != NULL)
            *result = binding->value;
        else
            *result = nil_value();
        break;

    case NODE_UNARY:
        evaluate(&left, node->data.operator_node.first, context);

        if (node->data.operator_node.operation == OP_NEGATE) {
            result->kind = VALUE_NUMBER;
            result->as.number = -left.as.number;
        } else {
            result->kind = VALUE_BOOLEAN;
            result->as.boolean = !value_is_true(&left);
        }
        break;

    case NODE_BINARY:
        evaluate(&left, node->data.operator_node.first, context);
        evaluate(&right, node->data.operator_node.second, context);

        switch (node->data.operator_node.operation) {
        case OP_ADD:
            result->kind = VALUE_NUMBER;
            result->as.number = left.as.number + right.as.number;
            break;

        case OP_SUBTRACT:
            result->kind = VALUE_NUMBER;
            result->as.number = left.as.number - right.as.number;
            break;

        case OP_MULTIPLY:
            result->kind = VALUE_NUMBER;
            result->as.number = left.as.number * right.as.number;
            break;

        case OP_DIVIDE:
            result->kind = VALUE_NUMBER;
            result->as.number =
                right.as.number == 0.0
                    ? 0.0
                    : left.as.number / right.as.number;
            break;

        case OP_LESS:
            result->kind = VALUE_BOOLEAN;
            result->as.boolean = left.as.number < right.as.number;
            break;

        case OP_GREATER:
            result->kind = VALUE_BOOLEAN;
            result->as.boolean = left.as.number > right.as.number;
            break;

        case OP_EQUAL:
            result->kind = VALUE_BOOLEAN;
            result->as.boolean = left.as.number == right.as.number;
            break;

        case OP_AND:
            result->kind = VALUE_BOOLEAN;
            result->as.boolean =
                value_is_true(&left) && value_is_true(&right);
            break;

        case OP_OR:
            result->kind = VALUE_BOOLEAN;
            result->as.boolean =
                value_is_true(&left) || value_is_true(&right);
            break;

        case OP_CONCATENATE:
            if (left.kind == VALUE_STRING && right.kind == VALUE_STRING) {
                uint64_t length =
                    left.as.string.length + right.as.string.length;
                char *text = malloc((size_t)length + 1);

                if (text == NULL)
                    exit(1);

                memcpy(text, left.as.string.data,
                       (size_t)left.as.string.length);
                memcpy(text + left.as.string.length,
                       right.as.string.data,
                       (size_t)right.as.string.length + 1);

                result->kind = VALUE_STRING;
                result->as.string.data = text;
                result->as.string.length = length;
            } else {
                *result = nil_value();
            }
            break;

        default:
            *result = nil_value();
            break;
        }
        break;

    case NODE_CALL:
        if (node->data.call.argument_count != 0)
            evaluate(&left, node->data.call.arguments[0], context);
        else
            left = nil_value();

        if (strcmp(node->data.call.name, "abs") == 0) {
            result->kind = VALUE_NUMBER;
            result->as.number =
                0.0 > left.as.number ? -left.as.number : left.as.number;
        } else if (strcmp(node->data.call.name, "sqr") == 0) {
            result->kind = VALUE_NUMBER;
            result->as.number = left.as.number * left.as.number;
        } else if (strcmp(node->data.call.name, "len") == 0) {
            result->kind = VALUE_NUMBER;
            result->as.number =
                left.kind == VALUE_STRING
                    ? (double)left.as.string.length
                    : 0.0;
        } else {
            *result = nil_value();
        }
        break;

    case NODE_CONDITIONAL:
        evaluate(&left, node->data.conditional.condition, context);

        if (value_is_true(&left)) {
            evaluate(result, node->data.conditional.when_true, context);
        } else if (node->data.conditional.when_false != NULL) {
            evaluate(result, node->data.conditional.when_false, context);
        } else {
            *result = nil_value();
        }
        break;

    case NODE_BLOCK:
        *result = nil_value();
        for (i = 0; i < node->data.block.count; ++i)
            evaluate(result, node->data.block.items[i], context);
        break;

    case NODE_ASSIGNMENT:
        evaluate(&left, node->data.named.expression, context);
        binding = lookup_binding(context, node->data.named.name);
        if (binding != NULL)
            binding->value = left;
        *result = left;
        break;

    default:
        *result = nil_value();
        break;
    }

    return result;
}

int main(void)
{
    Node *program = new_node(NODE_BLOCK);
    Context context = {0};
    Value result;
    uint32_t i;

    program->data.block.items[0] =
        assignment_node("x", number_node(5.0));

    program->data.block.items[1] =
        assignment_node(
            "y",
            binary_node(
                OP_SUBTRACT,
                binary_node(
                    OP_MULTIPLY,
                    identifier_node("x"),
                    number_node(2.0)),
                number_node(3.0)));

    program->data.block.items[2] =
        assignment_node(
            "z",
            conditional_node(
                binary_node(
                    OP_GREATER,
                    identifier_node("y"),
                    number_node(10.0)),
                call_node("sqr", identifier_node("x")),
                call_node(
                    "abs",
                    unary_node(OP_NEGATE, identifier_node("y")))));

    program->data.block.items[3] =
        assignment_node(
            "msg",
            binary_node(
                OP_CONCATENATE,
                string_node("Hi, "),
                string_node("C!")));

    program->data.block.items[4] =
        binary_node(
            OP_ADD,
            identifier_node("z"),
            call_node("len", identifier_node("msg")));

    program->data.block.count = 5;

    printf("%d\n", node_count(program));

    evaluate(&result, program, &context);
    print_value(&result);

    printf("\n%d\n", context.evaluations);

    for (i = 0; i < context.count; ++i) {
        printf("%s=", context.bindings[i].name);
        print_value(&context.bindings[i].value);
        printf(" (%d)\n", context.bindings[i].value.kind);
    }

    for (i = 0; i < context.count; ++i) {
        if (context.bindings[i].value.kind == VALUE_STRING)
            free(context.bindings[i].value.as.string.data);
    }

    destroy_node(program);
    return 0;
}