fs_attach(_list_node_t_* list, _list_node_t_* node) {
  node->prev = list;
  if (list->next) {
    list->next->next_prev = node;
    node->next_prev = list->next->next_prev;
  } else {
    list->last = node;
    node->next_prev = &list->last;
  }
  list->next = node;
}