#include<stdio.h>
#include<stdlib.h>


typedef struct list {
  char paren;
  unsigned int pos;
  struct list *next;
} paren_l;

typedef char paren_arr;
int push_paren_ll(paren_l**, char, unsigned int);
int pop_paren_ll(paren_l**);
int is_balance_ll(char*);
char *get_string_from_file(char*, int); //Free the string after use
int is_balance_arr(char*, int);
int push_paren_arr(char*, int*, char, unsigned int, int*, int);
int pop_paren_arr(int*);
