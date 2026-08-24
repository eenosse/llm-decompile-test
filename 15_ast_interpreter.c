/*
 * 15_ast_interpreter.c
 * Target feature: recursive tagged union - an expression AST where the union
 * arms contain pointers back to the same node type, evaluated by a recursive
 * switch. The value type is itself a small tagged union.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef enum ValueTag {
    V_NUM  = 0,
    V_BOOL = 1,
    V_STR  = 2,
    V_NIL  = 3
} ValueTag;

typedef struct Value {
    ValueTag tag;
    union {
        double num;
        int    boolean;
        struct {
            char  *chars;
            size_t len;
        } str;
    } as;
} Value;

typedef enum NodeKind {
    N_LITERAL = 0,
    N_VAR     = 1,
    N_UNARY   = 2,
    N_BINARY  = 3,
    N_CALL    = 4,
    N_IF      = 5,
    N_SEQ     = 6,
    N_ASSIGN  = 7
} NodeKind;

typedef enum OpCode {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_LT, OP_GT, OP_EQ, OP_AND, OP_OR,
    OP_NEG, OP_NOT, OP_CONCAT
} OpCode;

typedef struct Node Node;

struct Node {
    NodeKind kind;
    uint32_t line;
    union {
        Value literal;
        struct {
            char name[16];
        } var;
        struct {
            OpCode op;
            Node  *operand;
        } unary;
        struct {
            OpCode op;
            Node  *lhs;
            Node  *rhs;
        } binary;
        struct {
            char   name[16];
            Node  *args[4];
            uint8_t argc;
        } call;
        struct {
            Node *cond;
            Node *then_br;
            Node *else_br;
        } branch;
        struct {
            Node  *stmts[8];
            uint8_t count;
        } seq;
        struct {
            char  name[16];
            Node *value;
        } assign;
    } u;
};

typedef struct Binding {
    char  name[16];
    Value value;
} Binding;

typedef struct Env {
    Binding slots[16];
    uint32_t count;
    uint32_t evaluations;
} Env;

static uint32_t g_line = 1;

static Node *node_new(NodeKind k)
{
    Node *n = (Node *)calloc(1, sizeof(Node));
    if (!n)
        exit(1);
    n->kind = k;
    n->line = g_line++;
    return n;
}

static Node *lit_num(double v)
{
    Node *n = node_new(N_LITERAL);
    n->u.literal.tag = V_NUM;
    n->u.literal.as.num = v;
    return n;
}

static Node *lit_str(const char *s)
{
    Node *n = node_new(N_LITERAL);
    n->u.literal.tag = V_STR;
    n->u.literal.as.str.chars = strdup(s);
    n->u.literal.as.str.len = strlen(s);
    return n;
}

static Node *var_ref(const char *name)
{
    Node *n = node_new(N_VAR);
    strncpy(n->u.var.name, name, sizeof(n->u.var.name) - 1);
    return n;
}

static Node *binary(OpCode op, Node *a, Node *b)
{
    Node *n = node_new(N_BINARY);
    n->u.binary.op  = op;
    n->u.binary.lhs = a;
    n->u.binary.rhs = b;
    return n;
}

static Node *unary(OpCode op, Node *a)
{
    Node *n = node_new(N_UNARY);
    n->u.unary.op = op;
    n->u.unary.operand = a;
    return n;
}

static Node *assign(const char *name, Node *v)
{
    Node *n = node_new(N_ASSIGN);
    strncpy(n->u.assign.name, name, sizeof(n->u.assign.name) - 1);
    n->u.assign.value = v;
    return n;
}

static Node *if_expr(Node *c, Node *t, Node *e)
{
    Node *n = node_new(N_IF);
    n->u.branch.cond = c;
    n->u.branch.then_br = t;
    n->u.branch.else_br = e;
    return n;
}

static Node *call1(const char *name, Node *a0)
{
    Node *n = node_new(N_CALL);
    strncpy(n->u.call.name, name, sizeof(n->u.call.name) - 1);
    n->u.call.args[0] = a0;
    n->u.call.argc = 1;
    return n;
}

static Node *seq_of(Node **items, unsigned count)
{
    Node *n = node_new(N_SEQ);
    unsigned i;
    if (count > 8) count = 8;
    for (i = 0; i < count; i++)
        n->u.seq.stmts[i] = items[i];
    n->u.seq.count = (uint8_t)count;
    return n;
}

static Value mk_num(double d)  { Value v; v.tag = V_NUM;  v.as.num = d;    return v; }
static Value mk_bool(int b)    { Value v; v.tag = V_BOOL; v.as.boolean = b; return v; }
static Value mk_nil(void)      { Value v; memset(&v, 0, sizeof(v)); v.tag = V_NIL; return v; }

static Value *env_slot(Env *e, const char *name)
{
    uint32_t i;
    for (i = 0; i < e->count; i++)
        if (strcmp(e->slots[i].name, name) == 0)
            return &e->slots[i].value;
    if (e->count == 16)
        return NULL;
    strncpy(e->slots[e->count].name, name, sizeof(e->slots[0].name) - 1);
    e->slots[e->count].value = mk_nil();
    return &e->slots[e->count++].value;
}

static int truthy(const Value *v)
{
    switch (v->tag) {
    case V_NUM:  return v->as.num != 0.0;
    case V_BOOL: return v->as.boolean;
    case V_STR:  return v->as.str.len != 0;
    case V_NIL:  return 0;
    }
    return 0;
}

static Value eval(Node *n, Env *env);

static Value apply_binary(OpCode op, Value a, Value b)
{
    if (op == OP_CONCAT && a.tag == V_STR && b.tag == V_STR) {
        Value out;
        size_t len = a.as.str.len + b.as.str.len;
        char *buf = (char *)malloc(len + 1);
        if (!buf) exit(1);
        memcpy(buf, a.as.str.chars, a.as.str.len);
        memcpy(buf + a.as.str.len, b.as.str.chars, b.as.str.len + 1);
        out.tag = V_STR;
        out.as.str.chars = buf;
        out.as.str.len = len;
        return out;
    }
    switch (op) {
    case OP_ADD: return mk_num(a.as.num + b.as.num);
    case OP_SUB: return mk_num(a.as.num - b.as.num);
    case OP_MUL: return mk_num(a.as.num * b.as.num);
    case OP_DIV: return mk_num(b.as.num == 0.0 ? 0.0 : a.as.num / b.as.num);
    case OP_LT:  return mk_bool(a.as.num < b.as.num);
    case OP_GT:  return mk_bool(a.as.num > b.as.num);
    case OP_EQ:  return mk_bool(a.as.num == b.as.num);
    case OP_AND: return mk_bool(truthy(&a) && truthy(&b));
    case OP_OR:  return mk_bool(truthy(&a) || truthy(&b));
    default:     return mk_nil();
    }
}

static Value builtin(const char *name, Value arg)
{
    if (strcmp(name, "abs") == 0)
        return mk_num(arg.as.num < 0 ? -arg.as.num : arg.as.num);
    if (strcmp(name, "sqr") == 0)
        return mk_num(arg.as.num * arg.as.num);
    if (strcmp(name, "len") == 0)
        return mk_num(arg.tag == V_STR ? (double)arg.as.str.len : 0.0);
    return mk_nil();
}

static Value eval(Node *n, Env *env)
{
    env->evaluations++;
    switch (n->kind) {
    case N_LITERAL:
        return n->u.literal;
    case N_VAR: {
        Value *slot = env_slot(env, n->u.var.name);
        return slot ? *slot : mk_nil();
    }
    case N_UNARY: {
        Value v = eval(n->u.unary.operand, env);
        if (n->u.unary.op == OP_NEG)
            return mk_num(-v.as.num);
        return mk_bool(!truthy(&v));
    }
    case N_BINARY: {
        Value a = eval(n->u.binary.lhs, env);
        Value b = eval(n->u.binary.rhs, env);
        return apply_binary(n->u.binary.op, a, b);
    }
    case N_CALL: {
        Value arg = n->u.call.argc ? eval(n->u.call.args[0], env) : mk_nil();
        return builtin(n->u.call.name, arg);
    }
    case N_IF: {
        Value c = eval(n->u.branch.cond, env);
        if (truthy(&c))
            return eval(n->u.branch.then_br, env);
        return n->u.branch.else_br ? eval(n->u.branch.else_br, env) : mk_nil();
    }
    case N_SEQ: {
        Value last = mk_nil();
        uint8_t i;
        for (i = 0; i < n->u.seq.count; i++)
            last = eval(n->u.seq.stmts[i], env);
        return last;
    }
    case N_ASSIGN: {
        Value v = eval(n->u.assign.value, env);
        Value *slot = env_slot(env, n->u.assign.name);
        if (slot)
            *slot = v;
        return v;
    }
    }
    return mk_nil();
}

static void value_print(const Value *v)
{
    switch (v->tag) {
    case V_NUM:  printf("%.4f", v->as.num); break;
    case V_BOOL: printf("%s", v->as.boolean ? "true" : "false"); break;
    case V_STR:  printf("\"%s\"(%zu)", v->as.str.chars, v->as.str.len); break;
    case V_NIL:  printf("nil"); break;
    }
}

#ifndef RESULT_ONLY
static void ast_print(const Node *n, int depth)
{
    static const char *opnames[] = {"+","-","*","/","<",">","==","and","or","neg","not","++"};
    uint8_t i;

    printf("%*s", depth * 2, "");
    switch (n->kind) {
    case N_LITERAL: printf("literal "); value_print(&n->u.literal); putchar('\n'); break;
    case N_VAR:     printf("var %s\n", n->u.var.name); break;
    case N_UNARY:
        printf("unary %s\n", opnames[n->u.unary.op]);
        ast_print(n->u.unary.operand, depth + 1);
        break;
    case N_BINARY:
        printf("binary %s\n", opnames[n->u.binary.op]);
        ast_print(n->u.binary.lhs, depth + 1);
        ast_print(n->u.binary.rhs, depth + 1);
        break;
    case N_CALL:
        printf("call %s/%u\n", n->u.call.name, n->u.call.argc);
        for (i = 0; i < n->u.call.argc; i++)
            ast_print(n->u.call.args[i], depth + 1);
        break;
    case N_IF:
        printf("if\n");
        ast_print(n->u.branch.cond, depth + 1);
        ast_print(n->u.branch.then_br, depth + 1);
        if (n->u.branch.else_br)
            ast_print(n->u.branch.else_br, depth + 1);
        break;
    case N_SEQ:
        printf("seq/%u\n", n->u.seq.count);
        for (i = 0; i < n->u.seq.count; i++)
            ast_print(n->u.seq.stmts[i], depth + 1);
        break;
    case N_ASSIGN:
        printf("assign %s\n", n->u.assign.name);
        ast_print(n->u.assign.value, depth + 1);
        break;
    }
}
#endif

static uint32_t ast_count(const Node *n)
{
    uint32_t total = 1;
    uint8_t i;

    switch (n->kind) {
    case N_UNARY:  total += ast_count(n->u.unary.operand); break;
    case N_BINARY: total += ast_count(n->u.binary.lhs) + ast_count(n->u.binary.rhs); break;
    case N_CALL:
        for (i = 0; i < n->u.call.argc; i++)
            total += ast_count(n->u.call.args[i]);
        break;
    case N_IF:
        total += ast_count(n->u.branch.cond) + ast_count(n->u.branch.then_br);
        if (n->u.branch.else_br)
            total += ast_count(n->u.branch.else_br);
        break;
    case N_SEQ:
        for (i = 0; i < n->u.seq.count; i++)
            total += ast_count(n->u.seq.stmts[i]);
        break;
    case N_ASSIGN: total += ast_count(n->u.assign.value); break;
    default: break;
    }
    return total;
}

static void ast_free(Node *n)
{
    uint8_t i;

    switch (n->kind) {
    case N_LITERAL:
        if (n->u.literal.tag == V_STR)
            free(n->u.literal.as.str.chars);
        break;
    case N_UNARY:  ast_free(n->u.unary.operand); break;
    case N_BINARY: ast_free(n->u.binary.lhs); ast_free(n->u.binary.rhs); break;
    case N_CALL:
        for (i = 0; i < n->u.call.argc; i++)
            ast_free(n->u.call.args[i]);
        break;
    case N_IF:
        ast_free(n->u.branch.cond);
        ast_free(n->u.branch.then_br);
        if (n->u.branch.else_br)
            ast_free(n->u.branch.else_br);
        break;
    case N_SEQ:
        for (i = 0; i < n->u.seq.count; i++)
            ast_free(n->u.seq.stmts[i]);
        break;
    case N_ASSIGN: ast_free(n->u.assign.value); break;
    default: break;
    }
    free(n);
}

int main(void)
{
    Node *stmts[5];
    Node *program;
    Env env;
    Value result;
    uint32_t i;

    /* x = 12; y = x*3 - 4; z = if (y > 20) sqr(x) else abs(0-y); msg = "val:"++"ok"; z + len(msg) */
    stmts[0] = assign("x", lit_num(12.0));
    stmts[1] = assign("y", binary(OP_SUB, binary(OP_MUL, var_ref("x"), lit_num(3.0)), lit_num(4.0)));
    stmts[2] = assign("z", if_expr(binary(OP_GT, var_ref("y"), lit_num(20.0)),
                                   call1("sqr", var_ref("x")),
                                   call1("abs", unary(OP_NEG, var_ref("y")))));
    stmts[3] = assign("msg", binary(OP_CONCAT, lit_str("val:"), lit_str("ok")));
    stmts[4] = binary(OP_ADD, var_ref("z"), call1("len", var_ref("msg")));
    program  = seq_of(stmts, 5);

#ifdef RESULT_ONLY
    printf("%u\n", ast_count(program));
#else
    printf("AST (%u nodes):\n", ast_count(program));
    ast_print(program, 0);
#endif

    memset(&env, 0, sizeof(env));
    result = eval(program, &env);

    BENCH_VERBOSE(printf("result = "));
    value_print(&result);
    BENCH_OUTPUT(printf("  (evaluations=%u)\n", env.evaluations),
                 printf(" %u\n", env.evaluations));

    BENCH_OUTPUT(printf("environment (%u bindings):\n", env.count),
                 printf("%u\n", env.count));
    for (i = 0; i < env.count; i++) {
        BENCH_OUTPUT(printf("  %-6s = ", env.slots[i].name),
                     printf("%s ", env.slots[i].name));
        value_print(&env.slots[i].value);
        BENCH_OUTPUT(printf("  [tag=%d]\n", (int)env.slots[i].value.tag),
                     printf(" %d\n", (int)env.slots[i].value.tag));
    }

    for (i = 0; i < env.count; i++)
        if (env.slots[i].value.tag == V_STR)
            free(env.slots[i].value.as.str.chars);
    ast_free(program);

    return 0;
}
