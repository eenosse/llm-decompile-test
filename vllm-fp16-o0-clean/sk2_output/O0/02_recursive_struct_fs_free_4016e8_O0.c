fs_free(_list_node* node) {
  _list_node* next = node->next;
  _func1(next);
  free(node);
}