#ifndef RESULT_ONLY_OUTPUT_MODE_H
#define RESULT_ONLY_OUTPUT_MODE_H

/*
 * Keep one implementation of each benchmark while selecting its presentation
 * at preprocessing time. The unselected format string is not emitted into the
 * binary, which is important for the stripped-binary experiments.
 */
#ifdef RESULT_ONLY
#define BENCH_OUTPUT(verbose_expr, result_expr) \
    do { (void)(result_expr); } while (0)
#define BENCH_VERBOSE(verbose_expr) \
    do { } while (0)
#define BENCH_VERBOSE_ARG(value) \
    ((void)(value))
#else
#define BENCH_OUTPUT(verbose_expr, result_expr) \
    do { (void)(verbose_expr); } while (0)
#define BENCH_VERBOSE(verbose_expr) \
    do { (void)(verbose_expr); } while (0)
#define BENCH_VERBOSE_ARG(value) \
    ((void)0)
#endif

#endif
