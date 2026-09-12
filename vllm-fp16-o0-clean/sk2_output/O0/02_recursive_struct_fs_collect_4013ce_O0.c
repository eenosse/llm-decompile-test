fs_collect(Node* node, intdepth, Node* root) {
  inttype = node->type;
  switch (type) {
  case1:
    root->num_of_functions++;
    break;
  case2:
    root->num_of_variables++;
    break;
  case3:
    root->num_of_constants++;
    break;
  }
  root->num_of_nodes += node->num_of_children;
  if (depth > root->depth) {
    root->depth = depth;
  }
  Node* child = node->child;
  while (child != NULL) {
    func1(child, depth + 1, root);
    child = child->brother;
  }
}