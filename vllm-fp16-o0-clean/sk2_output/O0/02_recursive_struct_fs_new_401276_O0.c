*fs_new(constchar* name, inttype, node* next) {
  node* n = calloc(1, sizeof(node));
  if (n == NULL) exit(1);
  strncpy(n->name, name, 31);
  n->type = type;
  n->next = next;
  if (type == INT)
    n->size = 493;
  else
    n->size = 420;
  returnn;
}