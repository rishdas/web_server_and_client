#include "balanced_parenthsis.h"

int main(int argc, char *argv[])
{
  int       max_len;
  char      *inp_str;
  int       inp_choice;
  int       ds_choice;
  if (argc < 5) {
      fprintf(stderr, "Invalid Arguments\n");
      exit(EXIT_FAILURE);
  }
  max_len = atoi(argv[2]);
  inp_str = argv[1];
  inp_choice = atoi(argv[3]);
  ds_choice  = atoi(argv[4]);
  if( inp_choice > 0 ) {
     inp_str = get_string_from_file(inp_str, max_len);
  }
  printf("Input String: %s, Max_len: %d Input Choice: %s\n",
	 inp_str, max_len, (inp_choice > 0 ? "File" : "Command line"));
  switch(ds_choice) {
  case 1:
    {
      if( is_balance_ll(inp_str) == 0){
         printf("TRUE\n");
      } else {
         printf("FALSE\n");
      }
    }
    break;
  case 2:
    {
      if( is_balance_arr(inp_str, max_len) == 0){
         printf("TRUE\n");
      } else {
         printf("FALSE\n");
      }
    }
    break;
  default:
    printf("Wrong choice\n");
  }
  if( inp_choice > 0 ) {
     free(inp_str);
  }
}
int push_paren_ll(paren_l **head, char ch, unsigned int pos)
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

int pop_paren_ll(paren_l **head)
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

int is_balance_ll(char *inp_str)
{
  paren_l   *head;
  paren_arr *inp_str_cp;
  int       pos=0;
  int       status;
  inp_str_cp = inp_str;
  head = NULL;
  while(*inp_str != '\0')
    {
      pos = pos +1;
      if (*inp_str == '(') {
	  status = push_paren_ll(&head, *inp_str, pos);
      }
      if (*inp_str == ')') {
	  status = pop_paren_ll(&head);
	  if (status == 1) {
	      printf("Unbalanced string : %s at pos :%d\n", inp_str_cp, pos);
	      return 1;
	  }
      }
      inp_str++;
    }
  if(head == NULL) {
     printf(" %s is a balanced string\n", inp_str_cp);
     return 0;
  } else {
    printf("Unbalanced string : %s at pos :%d\n", inp_str_cp, head->pos);
    return 1;
  }
}

char *get_string_from_file(char *inp, int max_len)
{
  FILE *ifp;
  ifp = fopen(inp, "r");
  char *inp_str = (char *)malloc(sizeof(char)*max_len);
  if (ifp == NULL) {
      fprintf(stderr,"ERROR opening the file\n");
      exit(1);
  }
  if (fscanf(ifp, "%s", inp_str) == 1) {
      return inp_str;
  } else {
      fprintf(stderr, "File Empty\n");
      exit(1);
  }
  fclose(ifp);
}

int push_paren_arr(char *str_stack, int *pos_stack,
		   char ch, unsigned int pos, int *top, int max_len)
{
  if (*top == (max_len-1)) {
      fprintf(stderr, "Stack Full Exiting\n");
      exit(1);
  }
  if (ch != '(') {
      printf("Pushed charcter into stack not ( \n");
      return 1;
  }
  (*top)++;
  str_stack[*top] = ch;
  pos_stack[*top] = pos;
  return 0;
}

int pop_paren_arr(int *top)
{
  if(*top == -1) {
    printf("Stack empty exiting\n");
    return 1;
  }
  (*top)--;
  return 0;
}

int is_balance_arr(char *inp_str, int max_len)
{
  char         *paren_stack;
  int          *paren_pos_stack;
  unsigned int pos = 0;
  int          top = -1;
  int          status;
  char         *inp_str_cp;
  inp_str_cp = inp_str;
  paren_stack     = (char *)malloc(sizeof(char)*max_len);
  paren_pos_stack = (int  *)malloc(sizeof(int)*max_len);
  while(*inp_str != '\0')
    {
      pos = pos + 1;
      if (*inp_str == '(') {
	  status = push_paren_arr(paren_stack, paren_pos_stack,
		                  '(', pos, &top, max_len);
	  if (status != 0) {
	      fprintf(stderr, "Stack push failed exiting");
	  }
      }
      if (*inp_str == ')') {
	  status = pop_paren_arr(&top);
	  if (status == 1) {
	      printf("Unbalanced string : %s at pos :%d\n", inp_str_cp, pos);
	      return 1;
	  }
      }
      inp_str++;
    }
  if(top == -1) {
     printf(" %s is a balanced string\n", inp_str_cp);
     return 0;
  } else {
     printf("Unbalanced string : %s at pos :%d\n", inp_str_cp, paren_pos_stack[top]);
     return 1;
  }
  free(paren_stack);
  free(paren_pos_stack);
}
