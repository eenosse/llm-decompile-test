#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VALUE_NUMBER = 0,
    VALUE_BOOLEAN = 1,
    VALUE_STRING = 2,
    VALUE_NULL = 3
} ValueKind;

typedef struct {
    uint32_t kind;
    uint32_t padding;
    union {
        double number;
        uint32_t boolean;
        char *string;
    } data;
    uint64_t length;
} Value;

typedef enum {
    NODE_LITERAL = 0,
    NODE_VARIABLE = 1,
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

        struct {
            char name[16];
        } variable;

        struct {
            uint32_t operator;
            uint32_t padding;
            Node *operand;
        } unary;

        struct {
            uint32_t operator;
            uint32_t padding;
            Node *left;
            Node *right;
        } binary;

        struct {
            char name[16];
            Node *arguments[4];
            unsigned char count;
        } call;

        struct {
            Node *condition;
            Node *when_true;
            Node *when_false;
        } conditional;

        struct {
            Node *items[8];
            unsigned char count;
        } block;

        struct {
            char name[16];
            Node *expression;
        } assignment;
    } as;
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

static uint32_t next_node_id;

static Value null_value(void)
{
    Value value = {0};
    value.kind = VALUE_NULL;
    return value;
}

static Value number_value(double number)
{
    Value value = {0};
    value.kind = VALUE_NUMBER;
    value.data.number = number;
    return value;
}

static Value boolean_value(int boolean)
{
    Value value = {0};
    value.kind = VALUE_BOOLEAN;
    value.data.boolean = boolean != 0;
    return value;
}

static Node *new_node(NodeKind kind)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    node->kind = (uint32_t)kind;
    node->id = next_node_id++;
    return node;
}

static Node *new_number(double number)
{
    Node *node = new_node(NODE_LITERAL);
    node->as.literal = number_value(number);
    return node;
}

static Node *new_string(const char *text)
{
    Node *node = new_node(NODE_LITERAL);

    node->as.literal.kind = VALUE_STRING;
    node->as.literal.data.string = strdup(text);
    node->as.literal.length = strlen(text);
    return node;
}

static Node *new_variable(const char *name)
{
    Node *node = new_node(NODE_VARIABLE);
    strncpy(node->as.variable.name, name, 15);
    return node;
}

static Node *new_unary(Operator operator, Node *operand)
{
    Node *node = new_node(NODE_UNARY);
    node->as.unary.operator = (uint32_t)operator;
    node->as.unary.operand = operand;
    return node;
}

static Node *new_binary(Operator operator, Node *left, Node *right)
{
    Node *node = new_node(NODE_BINARY);
    node->as.binary.operator = (uint32_t)operator;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

static Node *new_call(const char *name, Node *argument)
{
    Node *node = new_node(NODE_CALL);

    strncpy(node->as.call.name, name, 15);
    node->as.call.arguments[0] = argument;
    node->as.call.count = 1;
    return node;
}

static Node *new_assignment(const char *name, Node *expression)
{
    Node *node = new_node(NODE_ASSIGNMENT);
    strncpy(node->as.assignment.name, name, 15);
    node->as.assignment.expression = expression;
    return node;
}

static int count_nodes(const Node *node)
{
    int count = 1;
    unsigned int i;

    if (node == NULL)
        return 1;

    switch (node->kind) {
    case NODE_UNARY:
        return 1 + count_nodes(node->as.unary.operand);

    case NODE_BINARY:
        return 1 + count_nodes(node->as.binary.left)
                 + count_nodes(node->as.binary.right);

    case NODE_CALL:
        for (i = 0; i < node->as.call.count; ++i)
            count += count_nodes(node->as.call.arguments[i]);
        return count;

    case NODE_CONDITIONAL:
        count += count_nodes(node->as.conditional.condition);
        count += count_nodes(node->as.conditional.when_true);
        if (node->as.conditional.when_false != NULL)
            count += count_nodes(node->as.conditional.when_false);
        return count;

    case NODE_BLOCK:
        for (i = 0; i < node->as.block.count; ++i)
            count += count_nodes(node->as.block.items[i]);
        return count;

    case NODE_ASSIGNMENT:
        return 1 + count_nodes(node->as.assignment.expression);

    default:
        return count;
    }
}

static void free_node(Node *node)
{
    unsigned int i;

    if (node == NULL)
        return;

    switch (node->kind) {
    case NODE_LITERAL:
        if (node->as.literal.kind == VALUE_STRING)
            free(node->as.literal.data.string);
        break;

    case NODE_UNARY:
        free_node(node->as.unary.operand);
        break;

    case NODE_BINARY:
        free_node(node->as.binary.left);
        free_node(node->as.binary.right);
        break;

    case NODE_CALL:
        for (i = 0; i < node->as.call.count; ++i)
            free_node(node->as.call.arguments[i]);
        break;

    case NODE_CONDITIONAL:
        free_node(node->as.conditional.condition);
        free_node(node->as.conditional.when_true);
        free_node(node->as.conditional.when_false);
        break;

    case NODE_BLOCK:
        for (i = 0; i < node->as.block.count; ++i)
            free_node(node->as.block.items[i]);
        break;

    case NODE_ASSIGNMENT:
        free_node(node->as.assignment.expression);
        break;
    }

    free(node);
}

static Value *find_binding(Environment *environment, const char *name)
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
    environment->bindings[i].value = null_value();
    return &environment->bindings[i].value;
}

static int value_is_true(Value value)
{
    switch (value.kind) {
    case VALUE_NUMBER:
        return value.data.number != 0.0;

    case VALUE_BOOLEAN:
        return value.data.boolean != 0;

    case VALUE_STRING:
        return value.data.string != NULL;

    default:
        return 0;
    }
}

static Value evaluate(Node *node, Environment *environment);

static Value evaluate_call(Node *node, Environment *environment)
{
    Value argument = null_value();
    double result = 0.0;

    if (node->as.call.count != 0)
        argument = evaluate(node->as.call.arguments[0], environment);

    if (strcmp(node->as.call.name, "abs") == 0) {
        if (argument.kind == VALUE_NUMBER) {
            result = argument.data.number;
            if (result < 0.0)
                result = -result;
        }
        return number_value(result);
    }

    if (strcmp(node->as.call.name, "sqr") == 0) {
        result = argument.data.number;
        return number_value(result * result);
    }

    if (strcmp(node->as.call.name, "len") == 0) {
        if (argument.kind == VALUE_STRING)
            result = (double)argument.length;
        return number_value(result);
    }

    return null_value();
}

static Value evaluate_binary(Node *node, Environment *environment)
{
    Value left = evaluate(node->as.binary.left, environment);
    Value right = evaluate(node->as.binary.right, environment);
    char *joined;

    switch (node->as.binary.operator) {
    case OP_ADD:
        return number_value(left.data.number + right.data.number);

    case OP_SUBTRACT:
        return number_value(left.data.number - right.data.number);

    case OP_MULTIPLY:
        return number_value(left.data.number * right.data.number);

    case OP_DIVIDE:
        if (right.data.number == 0.0)
            return number_value(0.0);
        return number_value(left.data.number / right.data.number);

    case OP_EQUAL:
        return boolean_value(left.data.number == right.data.number);

    case OP_LESS:
        return boolean_value(left.data.number < right.data.number);

    case OP_GREATER:
        return boolean_value(left.data.number > right.data.number);

    case OP_AND:
        return boolean_value(value_is_true(left) && value_is_true(right));

    case OP_OR:
        return boolean_value(value_is_true(left) || value_is_true(right));

    case OP_CONCATENATE:
        if (left.kind != VALUE_STRING || right.kind != VALUE_STRING)
            return null_value();

        joined = malloc((size_t)(left.length + right.length + 1));
        if (joined == NULL)
            exit(1);

        memcpy(joined, left.data.string, (size_t)left.length);
        memcpy(joined + left.length, right.data.string,
               (size_t)right.length + 1);

        left.kind = VALUE_STRING;
        left.data.string = joined;
        left.length += right.length;
        return left;

    default:
        return null_value();
    }
}

static Value evaluate(Node *node, Environment *environment)
{
    Value value;
    Value *binding;
    unsigned int i;

    environment->evaluations++;

    if (node == NULL)
        return null_value();

    switch (node->kind) {
    case NODE_LITERAL:
        return node->as.literal;

    case NODE_VARIABLE:
        binding = find_binding(environment, node->as.variable.name);
        return binding != NULL ? *binding : null_value();

    case NODE_UNARY:
        value = evaluate(node->as.unary.operand, environment);

        if (node->as.unary.operator == OP_NEGATE)
            return number_value(-value.data.number);

        return boolean_value(!value_is_true(value));

    case NODE_BINARY:
        return evaluate_binary(node, environment);

    case NODE_CALL:
        return evaluate_call(node, environment);

    case NODE_CONDITIONAL:
        value = evaluate(node->as.conditional.condition, environment);
        if (value_is_true(value))
            return evaluate(node->as.conditional.when_true, environment);
        if (node->as.conditional.when_false != NULL)
            return evaluate(node->as.conditional.when_false, environment);
        return null_value();

    case NODE_BLOCK:
        value = null_value();
        for (i = 0; i < node->as.block.count; ++i)
            value = evaluate(node->as.block.items[i], environment);
        return value;

    case NODE_ASSIGNMENT:
        value = evaluate(node->as.assignment.expression, environment);
        binding = find_binding(environment, node->as.assignment.name);
        if (binding != NULL)
            *binding = value;
        return value;

    default:
        return null_value();
    }
}

static void print_value(const Value *value)
{
    switch (value->kind) {
    case VALUE_NUMBER:
        printf("%.2f", value->data.number);
        break;

    case VALUE_BOOLEAN:
        printf("%s", value->data.boolean ? "true" : "false");
        break;

    case VALUE_STRING:
        printf("\"%.*s\"", (int)value->length, value->data.string);
        break;

    case VALUE_NULL:
        printf("nil");
        break;
    }
}

int main(void)
{
    Node *literal_x;
    Node *assign_x;
    Node *literal_subtrahend;
    Node *literal_multiplier;
    Node *variable_x;
    Node *product;
    Node *difference;
    Node *assign_y;
    Node *variable_y;
    Node *negated_y;
    Node *absolute_y;
    Node *second_variable_x;
    Node *squared_x;
    Node *limit;
    Node *second_variable_y;
    Node *condition;
    Node *conditional;
    Node *assign_z;
    Node *first_text;
    Node *second_text;
    Node *joined_text;
    Node *assign_message;
    Node *variable_message;
    Node *message_length;
    Node *variable_z;
    Node *result_expression;
    Node *program;
    Environment environment = {0};
    Value result;
    uint32_t i;

    literal_x = new_number(10.0);
    assign_x = new_assignment("x", literal_x);

    literal_subtrahend = new_number(3.0);
    literal_multiplier = new_number(2.0);
    variable_x = new_variable("x");
    product = new_binary(OP_MULTIPLY, variable_x, literal_multiplier);
    difference = new_binary(OP_SUBTRACT, product, literal_subtrahend);
    assign_y = new_assignment("y", difference);

    variable_y = new_variable("y");
    negated_y = new_unary(OP_NEGATE, variable_y);
    absolute_y = new_call("abs", negated_y);

    second_variable_x = new_variable("x");
    squared_x = new_call("sqr", second_variable_x);

    limit = new_number(10.0);
    second_variable_y = new_variable("y");
    condition = new_binary(OP_LESS, second_variable_y, limit);

    conditional = new_node(NODE_CONDITIONAL);
    conditional->as.conditional.condition = condition;
    conditional->as.conditional.when_true = squared_x;
    conditional->as.conditional.when_false = absolute_y;
    assign_z = new_assignment("z", conditional);

    first_text = new_string("Hi");
    second_text = new_string(" all");
    joined_text = new_binary(OP_CONCATENATE, second_text, first_text);
    assign_message = new_assignment("msg", joined_text);

    variable_message = new_variable("msg");
    message_length = new_call("len", variable_message);
    variable_z = new_variable("z");
    result_expression = new_binary(OP_ADD, variable_z, message_length);

    program = new_node(NODE_BLOCK);
    program->as.block.items[0] = assign_x;
    program->as.block.items[1] = assign_y;
    program->as.block.items[2] = assign_z;
    program->as.block.items[3] = assign_message;
    program->as.block.items[4] = result_expression;
    program->as.block.count = 5;

    printf("%d\n", count_nodes(program));

    result = evaluate(program, &environment);
    print_value(&result);

    printf("\n%d\n", environment.evaluations);
    printf("%d\n", environment.count);

    for (i = 0; i < environment.count; ++i) {
        printf("%s=", environment.bindings[i].name);
        print_value(&environment.bindings[i].value);
        printf(" (%d)\n", environment.bindings[i].value.kind);
    }

    for (i = 0; i < environment.count; ++i) {
        if (environment.bindings[i].value.kind == VALUE_STRING)
            free(environment.bindings[i].value.data.string);
    }

    free_node(program);
    return 0;
}