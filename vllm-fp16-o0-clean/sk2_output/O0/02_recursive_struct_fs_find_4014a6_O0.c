fs_find(node* root, char* name) {
  if (!strcmp(root->name, name)) {
    returnroot->name;
  }
  node* child = root->child;
  while (child != NULL) {
    char* found = _find_in_tree(child, name);
    if (found != NULL) {
      returnfound;
    }
  }
  returnNULL;
}