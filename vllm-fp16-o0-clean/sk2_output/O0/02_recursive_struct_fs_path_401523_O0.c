fs_path(Node* node, char* buf, intlen) {
  if (node->parent) {
    intpos = func1(node->parent, buf, len);
    if (pos >= 0 && pos < len) {
      snprintf(buf + pos, len - pos, "/%s", node->name);
    } else {
      return;
    }
  } else {
    snprintf(buf, len, "%s", node->name);
  }
}