main(intargc, char** argv) {
  FSTree* root = _fstree_new("root", 2, 0);
  FSTree* usr = _fstree_new("usr", 2, 0);
  FSTree* usr_bin = _fstree_add(root, usr);
  FSTree* usr_lib = _fstree_add(usr, _fstree_new("lib", 2, 0));
  FSTree* etc = _fstree_add(root, _fstree_new("etc", 2, 0));
  unsignedi;
  charbuf[32];
  for (i = 0; i < 3; i++) {
    snprintf(buf, 32, "tool%u", i);
    _fstree_add(usr_bin, _fstree_new(buf, 1, 12 + i * 4096));
    snprintf(buf, 32, "libz%u.so", i);
    _fstree_add(usr_lib, _fstree_new(buf, 1, 12 + i * 777));
  }
  FSTree* tool0 = _fstree_add(etc, _fstree_new("tool0.link", 3, 12));
  tool0->link = _fstree_find(root, "tool0");
  intis_link =
      tool0
          ->parent_link_name_and_size_and_inode_number_and_children_and_sibling_and_parent_and_name_and_type_and_data_and_link_and_link_name_and_link_size_and_link_inode_number_and_link_parent_and_link_sibling_and_link_children_and_link_parent_link_name_and_link_parent_link_size_and_link_parent_link_inode_number_and_link_parent_link_sibling_and_link_parent_link_children_and_link_link_name_and_link_link_size_and_link_link_inode_number_and_link_link_parent_and_link_link_sibling_and_link_link_children_and_link_link_parent_link_name_and_link_link_parent_link_size_and_link_link_parent_link_inode_number_and_link_link_parent_link_sibling_and_link_link_parent_link_children_and_link_link_link_name_and_link_link_link_size_and_link_link_link_inode_number_and_link_link_link_parent_and_link_link_link_sibling_and_link_link_link_children_and_link_link_link_parent_link_name_and_link_link_link_parent_link_size_and_link_link_link_parent_link_inode_number_and_link_link_link_parent_link_sibling_and_link_link_link_parent_link_children_and_link_link_link_link_name_and_link_link_link_link_size_and_link_link_link_link_inode_number_and_link_link_link_link_parent_and_link_link_link_link_sibling_and_link_link_link_link_children_and_link_link_link_link_parent_link_name_and_link_link_link_link_parent_link_size_and_link_link_link_link_parent_link_inode_number_and_link_link_link_link_parent_link_sibling_and_link_link_link_link_parent_link_children_and_link_link_link_link_link_name_and_link_link_link_link_link_size_and_link_link_link_link_link_inode_number_and_link_link_link_link_link_parent_and_link_link_link_link_link_sibling_and_link_link_link_link_link_children_and_link_link_link_link_link_parent_link_name_and_link_link_link_link_link_parent_link_size_and_link_link_link_link_link_parent_link_inode_number_and_link_link_link_link_link