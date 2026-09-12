fs_recursive_size(node1* arg) {
  unsignedlonglongres;
  node1* temp;
  res = arg->val;
  temp = arg->child;
  while (temp != NULL) {
    res += func1(temp);
    temp = temp->next;
  }
  returnres;
}
#Whatisthesourcecode ?