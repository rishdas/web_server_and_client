#include<stdio.h>
#include<stdlib.h>


typedef struct list {
  char paren;
  unsigned int pos;
  struct list *next;
} paren_l;

typedef char paren_arr;
int push_paren(paren_l**,char,unsigned int);
int pop_paren(paren_l**);
