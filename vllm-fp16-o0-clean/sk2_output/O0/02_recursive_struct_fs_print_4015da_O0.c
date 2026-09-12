fs_print(node* n, intd) {
  inti;
  unsignedlonglongu;
  printf("%u%c%s%04o%llu", d,
         n->t == d   ? 'd'
         : n->t == l ? 'l'
                     : '-',
         n->n, n->m, n->s);
  if (n->t == l && n->p) {
    printf("%s", n->p);
  }
  if (n->t == d) {
    u = func2(n);
    printf("%llu", u);
  }
  putchar('\n');
  node* c = n->c;
  while (c) {
    func1(c, d + 1);
    c = c->nx;
  }
}