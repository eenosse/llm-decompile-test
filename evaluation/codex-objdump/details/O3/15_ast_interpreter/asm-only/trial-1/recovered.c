#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VALUE_NUMBER = 0,
    VALUE_BOOLEAN = 1,
    VALUE_STRING = 2,
    VALUE_NONE = 3
} ValueKind;

typedef struct {
    uint32_t kind;
    uint32_t padding;
    union {
        double number;
        int32_t boolean;
        struct {
            char *data;
            size_t length;
        } string;
    } as;
} Value;

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
    OP_EQUAL = 4,
    OP_LESS = 5,
    OP_GREATER = 6,
    OP_AND = 7,
    OP_OR = 8,
    OP_NEGATE = 9,
    OP_NOT = 10,
    OP_CONCATENATE = 11
} Operator;

typedef struct Node Node;

struct Node {
    uint32_t kind;
    uint32_t id;
    union {
        Value literal;
        char identifier[16];

        struct {
            uint32_t operation;
            uint32_t padding;
            Node *operand;
        } unary;

        struct {
            uint32_t operation;
            uint32_t padding;
            Node *left;
            Node *right;
        } binary;

        struct {
            char name[16];
            Node *arguments[4];
            uint8_t count;
        } call;

        struct {
            Node *condition;
            Node *when_true;
            Node *when_false;
        } conditional;

        struct {
            Node *items[8];
            uint8_t count;
        } block;

        struct {
            char name[16];
            Node *value;
        } assignment;
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
} Environment;

_Static_assert(sizeof(Value) == 24, "unexpected Value layout");
_Static_assert(sizeof(Node) == 80, "unexpected Node layout");
_Static_assert(sizeof(Binding) == 40, "unexpected Binding layout");

static uint32_t next_node_id;

static Value none_value(void)
{
    Value value = {0};
    value.kind = VALUE_NONE;
    return value;
}

static Node *new_node(uint32_t kind)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    node->kind = kind;
    node->id = next_node_id++;
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
    strncpy(node->data.identifier, name, 15);
    return node;
}

static Node *unary_node(uint32_t operation, Node *operand)
{
    Node *node = new_node(NODE_UNARY);
    node->data.unary.operation = operation;
    node->data.unary.operand = operand;
    return node;
}

static Node *binary_node(uint32_t operation, Node *left, Node *right)
{
    Node *node = new_node(NODE_BINARY);
    node->data.binary.operation = operation;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

static Node *call_node(const char *name, Node *argument)
{
    Node *node = new_node(NODE_CALL);
    strncpy(node->data.call.name, name, 15);
    node->data.call.arguments[0] = argument;
    node->data.call.count = 1;
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

static Node *assignment_node(const char *name, Node *value)
{
    Node *node = new_node(NODE_ASSIGNMENT);
    strncpy(node->data.assignment.name, name, 15);
    node->data.assignment.value = value;
    return node;
}

static Node *block_node(Node **items, uint8_t count)
{
    Node *node = new_node(NODE_BLOCK);
    uint8_t i;

    for (i = 0; i < count; ++i)
        node->data.block.items[i] = items[i];

    node->data.block.count = count;
    return node;
}

static uint32_t count_nodes(const Node *node)
{
    uint32_t result = 1;
    uint8_t i;

    if (node == NULL || node->kind > NODE_ASSIGNMENT)
        return 1;

    switch (node->kind) {
    case NODE_LITERAL:
    case NODE_IDENTIFIER:
        break;

    case NODE_UNARY:
        result += count_nodes(node->data.unary.operand);
        break;

    case NODE_BINARY:
        result += count_nodes(node->data.binary.left);
        result += count_nodes(node->data.binary.right);
        break;

    case NODE_CALL:
        for (i = 0; i < node->data.call.count; ++i)
            result += count_nodes(node->data.call.arguments[i]);
        break;

    case NODE_CONDITIONAL:
        result += count_nodes(node->data.conditional.condition);
        result += count_nodes(node->data.conditional.when_true);
        result += count_nodes(node->data.conditional.when_false);
        break;

    case NODE_BLOCK:
        for (i = 0; i < node->data.block.count; ++i)
            result += count_nodes(node->data.block.items[i]);
        break;

    case NODE_ASSIGNMENT:
        result += count_nodes(node->data.assignment.value);
        break;
    }

    return result;
}

static void destroy_node(Node *node)
{
    uint8_t i;

    if (node == NULL)
        return;

    if (node->kind <= NODE_ASSIGNMENT) {
        switch (node->kind) {
        case NODE_LITERAL:
            if (node->data.literal.kind == VALUE_STRING)
                free(node->data.literal.as.string.data);
            break;

        case NODE_IDENTIFIER:
            break;

        case NODE_UNARY:
            destroy_node(node->data.unary.operand);
            break;

        case NODE_BINARY:
            destroy_node(node->data.binary.left);
            destroy_node(node->data.binary.right);
            break;

        case NODE_CALL:
            for (i = 0; i < node->data.call.count; ++i)
                destroy_node(node->data.call.arguments[i]);
            break;

        case NODE_CONDITIONAL:
            destroy_node(node->data.conditional.condition);
            destroy_node(node->data.conditional.when_true);
            destroy_node(node->data.conditional.when_false);
            break;

        case NODE_BLOCK:
            for (i = 0; i < node->data.block.count; ++i)
                destroy_node(node->data.block.items[i]);
            break;

        case NODE_ASSIGNMENT:
            destroy_node(node->data.assignment.value);
            break;
        }
    }

    free(node);
}

static Value *environment_slot(Environment *environment, const char *name)
{
    uint32_t i;

    for (i = 0; i < environment->count; ++i) {
        if (strcmp(environment->bindings[i].name, name) == 0)
            return &environment->bindings[i].value;
    }

    if (environment->count == 16)
        return NULL;

    i = environment->count++;
    strncpy(environment->bindings[i].name, name, 15);
    environment->bindings[i].value = none_value();
    return &environment->bindings[i].value;
}

static int truthy(Value value)
{
    switch (value.kind) {
    case VALUE_NUMBER:
        return value.as.number != 0.0;
    case VALUE_BOOLEAN:
        return value.as.boolean != 0;
    case VALUE_STRING:
        return value.as.string.data != NULL;
    default:
        return 0;
    }
}

static Value evaluate(Node *node, Environment *environment);

static Value evaluate_binary(uint32_t operation, Value left, Value right)
{
    Value result = none_value();

    if (operation == OP_CONCATENATE) {
        size_t total;
        char *text;

        if (left.kind != VALUE_STRING || right.kind != VALUE_STRING)
            return result;

        total = left.as.string.length + right.as.string.length;
        text = malloc(total + 1);
        if (text == NULL)
            exit(1);

        memcpy(text, left.as.string.data, left.as.string.length);
        memcpy(text + left.as.string.length,
               right.as.string.data, right.as.string.length + 1);

        result.kind = VALUE_STRING;
        result.as.string.data = text;
        result.as.string.length = total;
        return result;
    }

    switch (operation) {
    case OP_ADD:
        result.kind = VALUE_NUMBER;
        result.as.number = left.as.number + right.as.number;
        break;

    case OP_SUBTRACT:
        result.kind = VALUE_NUMBER;
        result.as.number = left.as.number - right.as.number;
        break;

    case OP_MULTIPLY:
        result.kind = VALUE_NUMBER;
        result.as.number = left.as.number * right.as.number;
        break;

    case OP_DIVIDE:
        result.kind = VALUE_NUMBER;
        result.as.number = right.as.number == 0.0
                         ? 0.0
                         : left.as.number / right.as.number;
        break;

    case OP_EQUAL:
        result.kind = VALUE_BOOLEAN;
        result.as.boolean = left.as.number == right.as.number;
        break;

    case OP_LESS:
        result.kind = VALUE_BOOLEAN;
        result.as.boolean = left.as.number < right.as.number;
        break;

    case OP_GREATER:
        result.kind = VALUE_BOOLEAN;
        result.as.boolean = left.as.number > right.as.number;
        break;

    case OP_AND:
        result.kind = VALUE_BOOLEAN;
        result.as.boolean = truthy(left) && truthy(right);
        break;

    case OP_OR:
        result.kind = VALUE_BOOLEAN;
        result.as.boolean = truthy(left) || truthy(right);
        break;

    default:
        break;
    }

    return result;
}

static Value evaluate_call(Node *node, Environment *environment)
{
    Value argument = none_value();
    Value result = none_value();

    if (node->data.call.count != 0)
        argument = evaluate(node->data.call.arguments[0], environment);

    if (strcmp(node->data.call.name, "abs") == 0) {
        result.kind = VALUE_NUMBER;
        result.as.number = argument.as.number < 0.0
                         ? -argument.as.number
                         : argument.as.number;
    } else if (strcmp(node->data.call.name, "sqr") == 0) {
        result.kind = VALUE_NUMBER;
        result.as.number = argument.as.number * argument.as.number;
    } else if (strcmp(node->data.call.name, "len") == 0) {
        result.kind = VALUE_NUMBER;
        result.as.number = (double)argument.as.string.length;
    }

    return result;
}

static Value evaluate(Node *node, Environment *environment)
{
    Value result = none_value();
    Value left;
    Value right;
    Value *slot;
    uint8_t i;

    ++environment->evaluations;

    if (node == NULL || node->kind > NODE_ASSIGNMENT)
        return result;

    switch (node->kind) {
    case NODE_LITERAL:
        return node->data.literal;

    case NODE_IDENTIFIER:
        slot = environment_slot(environment, node->data.identifier);
        return slot != NULL ? *slot : result;

    case NODE_UNARY:
        result = evaluate(node->data.unary.operand, environment);
        if (node->data.unary.operation == OP_NEGATE) {
            left.kind = VALUE_NUMBER;
            left.as.number = -result.as.number;
            return left;
        }

        left.kind = VALUE_BOOLEAN;
        left.as.boolean = !truthy(result);
        return left;

    case NODE_BINARY:
        left = evaluate(node->data.binary.left, environment);
        right = evaluate(node->data.binary.right, environment);
        return evaluate_binary(node->data.binary.operation, left, right);

    case NODE_CALL:
        return evaluate_call(node, environment);

    case NODE_CONDITIONAL:
        result = evaluate(node->data.conditional.condition, environment);
        if (truthy(result))
            return evaluate(node->data.conditional.when_true, environment);
        return evaluate(node->data.conditional.when_false, environment);

    case NODE_BLOCK:
        for (i = 0; i < node->data.block.count; ++i)
            result = evaluate(node->data.block.items[i], environment);
        return result;

    case NODE_ASSIGNMENT:
        result = evaluate(node->data.assignment.value, environment);
        slot = environment_slot(environment, node->data.assignment.name);
        if (slot != NULL)
            *slot = result;
        return result;
    }

    return result;
}

static void print_value(Value value)
{
    switch (value.kind) {
    case VALUE_NUMBER:
        printf("%.2f", value.as.number);
        break;
    case VALUE_BOOLEAN:
        printf("%s", value.as.boolean ? "true" : "false");
        break;
    case VALUE_STRING:
        printf("%s (%zu)", value.as.string.data, value.as.string.length);
        break;
    default:
        printf("N/A");
        break;
    }
}

int main(void)
{
    Node *assignment_x;
    Node *assignment_y;
    Node *assignment_z;
    Node *assignment_message;
    Node *final_expression;
    Node *program;
    Node *items[5];
    Environment environment = {0};
    Value result;
    uint32_t i;

    assignment_x = assignment_node("x", number_node(5.0));

    {
        Node *constant_three = number_node(3.0);
        Node *constant_two = number_node(2.0);
        Node *x = identifier_node("x");
        Node *product = binary_node(OP_MULTIPLY, x, constant_two);
        Node *difference = binary_node(OP_SUBTRACT, product, constant_three);

        assignment_y = assignment_node("y", difference);
    }

    {
        Node *negative_y =
            unary_node(OP_NEGATE, identifier_node("y"));
        Node *absolute_y = call_node("abs", negative_y);
        Node *square_x = call_node("sqr", identifier_node("x"));
        Node *condition =
            binary_node(OP_LESS, identifier_node("y"), number_node(10.0));
        Node *choice =
            conditional_node(condition, square_x, absolute_y);

        assignment_z = assignment_node("z", choice);
    }

    {
        Node *message =
            binary_node(OP_CONCATENATE,
                        string_node("Hi"),
                        string_node(" C11"));

        assignment_message = assignment_node("msg", message);
    }

    final_expression =
        binary_node(OP_ADD,
                    identifier_node("z"),
                    call_node("len", identifier_node("msg")));

    items[0] = assignment_x;
    items[1] = assignment_y;
    items[2] = assignment_z;
    items[3] = assignment_message;
    items[4] = final_expression;
    program = block_node(items, 5);

    printf("%d\n", (int)count_nodes(program));

    result = evaluate(program, &environment);
    print_value(result);

    printf("\n%d\n", (int)environment.evaluations);
    printf("%d\n", (int)environment.count);

    for (i = 0; i < environment.count; ++i) {
        printf("%s:", environment.bindings[i].name);
        print_value(environment.bindings[i].value);
        printf(" (%d)\n", (int)environment.bindings[i].value.kind);
    }

    for (i = 0; i < environment.count; ++i) {
        if (environment.bindings[i].value.kind == VALUE_STRING)
            free(environment.bindings[i].value.as.string.data);
    }

    destroy_node(program);
    return 0;
}