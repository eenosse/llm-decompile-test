#define _GNU_SOURCE

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
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
    union {
        double number;
        int32_t boolean;
        struct {
            char *data;
            size_t length;
        } string;
    } as;
} Value;

typedef struct Node Node;

typedef enum {
    NODE_LITERAL = 0,
    NODE_VARIABLE = 1,
    NODE_UNARY = 2,
    NODE_BINARY = 3,
    NODE_CALL = 4,
    NODE_CONDITIONAL = 5,
    NODE_SEQUENCE = 6,
    NODE_ASSIGNMENT = 7
} NodeKind;

struct Node {
    uint32_t kind;
    uint32_t identifier;
    union {
        Value literal;

        struct {
            char name[16];
        } variable;

        struct {
            uint32_t operation;
            Node *operand;
        } unary;

        struct {
            uint32_t operation;
            Node *left;
            Node *right;
        } binary;

        struct {
            char name[16];
            Node *arguments[4];
            uint8_t argument_count;
        } call;

        struct {
            Node *condition;
            Node *when_true;
            Node *when_false;
        } conditional;

        struct {
            Node *items[8];
            uint8_t count;
        } sequence;

        struct {
            char name[16];
            Node *expression;
        } assignment;
    } as;
};

typedef struct {
    char name[16];
    Value value;
} EnvironmentEntry;

typedef struct {
    EnvironmentEntry entries[16];
    uint32_t count;
    uint32_t evaluations;
} Environment;

_Static_assert(sizeof(Value) == 24, "unexpected Value layout");
_Static_assert(sizeof(Node) == 80, "unexpected Node layout");
_Static_assert(sizeof(EnvironmentEntry) == 40,
               "unexpected EnvironmentEntry layout");
_Static_assert(sizeof(Environment) == 648, "unexpected Environment layout");

static uint32_t next_identifier;

static Node *allocate_node(uint32_t kind)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    node->kind = kind;
    node->identifier = next_identifier++;
    return node;
}

static Node *make_number(double number)
{
    Node *node = allocate_node(NODE_LITERAL);
    node->as.literal.kind = VALUE_NUMBER;
    node->as.literal.as.number = number;
    return node;
}

static Node *make_string(const char *text)
{
    Node *node = allocate_node(NODE_LITERAL);
    node->as.literal.kind = VALUE_STRING;
    node->as.literal.as.string.data = strdup(text);
    node->as.literal.as.string.length = strlen(text);
    return node;
}

static Node *make_variable(const char *name)
{
    Node *node = allocate_node(NODE_VARIABLE);
    strncpy(node->as.variable.name, name, 15);
    return node;
}

static Node *make_binary(uint32_t operation, Node *left, Node *right)
{
    Node *node = allocate_node(NODE_BINARY);
    node->as.binary.operation = operation;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

static Node *make_unary(uint32_t operation, Node *operand)
{
    Node *node = allocate_node(NODE_UNARY);
    node->as.unary.operation = operation;
    node->as.unary.operand = operand;
    return node;
}

static Node *make_conditional(Node *condition, Node *when_true,
                              Node *when_false)
{
    Node *node = allocate_node(NODE_CONDITIONAL);
    node->as.conditional.condition = condition;
    node->as.conditional.when_true = when_true;
    node->as.conditional.when_false = when_false;
    return node;
}

static Node *make_call(const char *name, Node *argument)
{
    Node *node = allocate_node(NODE_CALL);
    strncpy(node->as.call.name, name, 15);
    node->as.call.arguments[0] = argument;
    node->as.call.argument_count = 1;
    return node;
}

static Node *make_assignment(const char *name, Node *expression)
{
    Node *node = allocate_node(NODE_ASSIGNMENT);
    strncpy(node->as.assignment.name, name, 15);
    node->as.assignment.expression = expression;
    return node;
}

static Node *make_sequence(Node *const *items, uint32_t count)
{
    Node *node = allocate_node(NODE_SEQUENCE);
    uint32_t index;

    if (count > 8)
        count = 8;

    for (index = 0; index < count; ++index)
        node->as.sequence.items[index] = items[index];

    node->as.sequence.count = (uint8_t)count;
    return node;
}

static Value value_number(double number)
{
    Value value = {0};
    value.kind = VALUE_NUMBER;
    value.as.number = number;
    return value;
}

static Value value_boolean(int32_t boolean)
{
    Value value = {0};
    value.kind = VALUE_BOOLEAN;
    value.as.boolean = boolean;
    return value;
}

static Value value_nil(void)
{
    Value value = {0};
    value.kind = VALUE_NIL;
    return value;
}

static Value *environment_lookup(Environment *environment, const char *name)
{
    uint32_t index;

    for (index = 0; index < environment->count; ++index) {
        if (strcmp(environment->entries[index].name, name) == 0)
            return &environment->entries[index].value;
    }

    if (environment->count == 16)
        return NULL;

    index = environment->count;
    strncpy(environment->entries[index].name, name, 15);
    environment->entries[index].value = value_nil();
    environment->count++;
    return &environment->entries[index].value;
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
    case VALUE_NIL:
    default:
        return 0;
    }
}

static Value apply_binary(uint32_t operation, Value left, Value right)
{
    Value result;

    if (operation == 11 &&
        left.kind == VALUE_STRING &&
        right.kind == VALUE_STRING) {
        size_t length = left.as.string.length + right.as.string.length;
        char *text = malloc(length + 1);

        if (text == NULL)
            exit(1);

        memcpy(text, left.as.string.data, left.as.string.length);
        memcpy(text + left.as.string.length,
               right.as.string.data,
               right.as.string.length + 1);

        result = value_nil();
        result.kind = VALUE_STRING;
        result.as.string.data = text;
        result.as.string.length = length;
        return result;
    }

    switch (operation) {
    case 0:
        return value_number(left.as.number + right.as.number);
    case 1:
        return value_number(left.as.number - right.as.number);
    case 2:
        return value_number(left.as.number * right.as.number);
    case 3:
        if (right.as.number == 0.0)
            return value_number(NAN);
        return value_number(left.as.number / right.as.number);
    case 4:
        return value_boolean(left.as.number < right.as.number);
    case 5:
        return value_boolean(left.as.number > right.as.number);
    case 6:
        return value_boolean(left.as.number == right.as.number);
    case 7:
        return value_boolean(value_is_true(&left) &&
                             value_is_true(&right));
    case 8:
        return value_boolean(value_is_true(&left) ||
                             value_is_true(&right));
    default:
        return value_nil();
    }
}

static Value apply_function(const char *name, Value argument)
{
    if (strcmp(name, "abs") == 0) {
        double number = argument.as.number;
        return value_number(number < 0.0 ? -number : number);
    }

    if (strcmp(name, "sqr") == 0)
        return value_number(argument.as.number * argument.as.number);

    if (strcmp(name, "len") == 0) {
        if (argument.kind == VALUE_STRING)
            return value_number((double)argument.as.string.length);
        return value_number(NAN);
    }

    return value_nil();
}

static Value evaluate(Node *node, Environment *environment)
{
    Value first;
    Value second;
    Value result;
    Value *slot;
    uint8_t index;

    environment->evaluations++;

    switch (node->kind) {
    case NODE_LITERAL:
        return node->as.literal;

    case NODE_VARIABLE:
        slot = environment_lookup(environment, node->as.variable.name);
        return slot != NULL ? *slot : value_nil();

    case NODE_UNARY:
        first = evaluate(node->as.unary.operand, environment);

        if (node->as.unary.operation == 9)
            return value_number(-first.as.number);

        return value_boolean(!value_is_true(&first));

    case NODE_BINARY:
        first = evaluate(node->as.binary.left, environment);
        second = evaluate(node->as.binary.right, environment);
        return apply_binary(node->as.binary.operation, first, second);

    case NODE_CALL:
        if (node->as.call.argument_count != 0)
            first = evaluate(node->as.call.arguments[0], environment);
        else
            first = value_nil();

        return apply_function(node->as.call.name, first);

    case NODE_CONDITIONAL:
        first = evaluate(node->as.conditional.condition, environment);

        if (value_is_true(&first))
            return evaluate(node->as.conditional.when_true, environment);

        if (node->as.conditional.when_false != NULL)
            return evaluate(node->as.conditional.when_false, environment);

        return value_nil();

    case NODE_SEQUENCE:
        result = value_nil();

        for (index = 0; index < node->as.sequence.count; ++index)
            result = evaluate(node->as.sequence.items[index], environment);

        return result;

    case NODE_ASSIGNMENT:
        result = evaluate(node->as.assignment.expression, environment);
        slot = environment_lookup(environment, node->as.assignment.name);

        if (slot != NULL)
            *slot = result;

        return result;

    default:
        return value_nil();
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
        printf("\"%s\"(%zu)",
               value->as.string.data,
               value->as.string.length);
        break;
    case VALUE_NIL:
        printf("<nil>");
        break;
    }
}

static int count_nodes(const Node *node)
{
    int count = 1;
    uint8_t index;

    switch (node->kind) {
    case NODE_UNARY:
        count += count_nodes(node->as.unary.operand);
        break;

    case NODE_BINARY:
        count += count_nodes(node->as.binary.left);
        count += count_nodes(node->as.binary.right);
        break;

    case NODE_CALL:
        for (index = 0; index < node->as.call.argument_count; ++index)
            count += count_nodes(node->as.call.arguments[index]);
        break;

    case NODE_CONDITIONAL:
        count += count_nodes(node->as.conditional.condition);
        count += count_nodes(node->as.conditional.when_true);

        if (node->as.conditional.when_false != NULL)
            count += count_nodes(node->as.conditional.when_false);
        break;

    case NODE_SEQUENCE:
        for (index = 0; index < node->as.sequence.count; ++index)
            count += count_nodes(node->as.sequence.items[index]);
        break;

    case NODE_ASSIGNMENT:
        count += count_nodes(node->as.assignment.expression);
        break;

    default:
        break;
    }

    return count;
}

static void destroy_node(Node *node)
{
    uint8_t index;

    switch (node->kind) {
    case NODE_LITERAL:
        if (node->as.literal.kind == VALUE_STRING)
            free(node->as.literal.as.string.data);
        break;

    case NODE_UNARY:
        destroy_node(node->as.unary.operand);
        break;

    case NODE_BINARY:
        destroy_node(node->as.binary.left);
        destroy_node(node->as.binary.right);
        break;

    case NODE_CALL:
        for (index = 0; index < node->as.call.argument_count; ++index)
            destroy_node(node->as.call.arguments[index]);
        break;

    case NODE_CONDITIONAL:
        destroy_node(node->as.conditional.condition);
        destroy_node(node->as.conditional.when_true);

        if (node->as.conditional.when_false != NULL)
            destroy_node(node->as.conditional.when_false);
        break;

    case NODE_SEQUENCE:
        for (index = 0; index < node->as.sequence.count; ++index)
            destroy_node(node->as.sequence.items[index]);
        break;

    case NODE_ASSIGNMENT:
        destroy_node(node->as.assignment.expression);
        break;

    default:
        break;
    }

    free(node);
}

int main(void)
{
    Node *statements[5];
    Node *program;
    Node *temporary;
    Node *left;
    Node *right;
    Environment environment;
    Value result;
    uint32_t index;

    statements[0] = make_assignment("x", make_number(10.0));

    right = make_number(3.0);
    temporary = make_number(2.0);
    left = make_binary(2, make_variable("x"), temporary);
    statements[1] = make_assignment("y", make_binary(1, left, right));

    right = make_call("abs", make_unary(9, make_variable("y")));
    left = make_call("sqr", make_variable("x"));
    temporary = make_binary(5, make_variable("y"), make_number(10.0));
    statements[2] = make_assignment(
        "z", make_conditional(temporary, left, right));

    right = make_string("VM");
    left = make_string("Hi, ");
    statements[3] = make_assignment(
        "msg", make_binary(11, left, right));

    statements[4] = make_binary(
        0,
        make_variable("z"),
        make_call("len", make_variable("msg")));

    program = make_sequence(statements, 5);

    printf("%d\n", count_nodes(program));

    memset(&environment, 0, sizeof(environment));
    result = evaluate(program, &environment);

    print_value(&result);
    printf("\n%u\n", environment.evaluations);
    printf("%d\n", (int)environment.count);

    for (index = 0; index < environment.count; ++index) {
        printf("%s=", environment.entries[index].name);
        print_value(&environment.entries[index].value);
        printf(" (%d)\n", (int)environment.entries[index].value.kind);
    }

    for (index = 0; index < environment.count; ++index) {
        if (environment.entries[index].value.kind == VALUE_STRING)
            free(environment.entries[index].value.as.string.data);
    }

    destroy_node(program);
    return 0;
}