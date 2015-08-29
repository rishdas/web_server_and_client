#include "balanced_parenthsis.h"

int main(int argc, char *argv[])
{
  int       max_len;
  paren_l   *head;
  paren_arr *inp_str,*inp_str_cp;
  int       pos=0;
  int       status;
  if (argc < 3) {
      printf("Invalid Arguments\n");
      exit(EXIT_FAILURE);
  }
  max_len = atoi(argv[2]);
  inp_str = argv[1];
  printf("Input String: %s, Max_len: %d\n",inp_str, max_len);
  inp_str_cp = inp_str;
  head = NULL;
  while(*inp_str != '\0')
    {
      pos = pos +1;
      if (*inp_str == '(') {
	  status = push_paren(&head, *inp_str, pos);
      }
      if (*inp_str == ')') {
	  status = pop_paren(&head);
	  if (status == 1) {
	      printf("Unbalanced string : %s at pos :%d\n", inp_str_cp, pos);
	      return 0;
	  }
      }
      inp_str++;
    }
  if(head == NULL) {
     printf(" %s is a balanced string\n", inp_str_cp);
     return 0;
  } else {
    printf("Unbalanced string : %s at pos :%d\n", inp_str_cp, head->pos);
    return 0;
  }
}
int push_paren(paren_l **head, char ch, unsigned int pos)
{
  paren_l *temp;
  temp = (paren_l *)malloc(sizeof(paren_l));
  if (temp == NULL) {
      exit(EXIT_FAILURE);
  }
  
  if (ch != '(') {
      printf("Pushed charcter into stack not ( \n");
      return 1;
  }
  temp->paren = ch;
  temp->pos   = pos;
  temp->next = *head;
  *head = temp;
  return 0;
}

int pop_paren(paren_l **head)
{
  if (*head == NULL) {
      printf("paren stack empty\n");
      return 1;
  }
  paren_l *temp;
  temp = *head;
  *head = (*head)->next;
  free(temp);
  return 0;
}
