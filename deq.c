#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deq.h"
#include "error.h"

// indices and size of array of node pointers
typedef enum {Head,Tail,Ends} End;

typedef struct Node {
  struct Node *np[Ends];        // next/prev neighbors
  Data data;
} *Node;

typedef struct {
  Node ht[Ends];                // head/tail nodes
  int len;
} *Rep;

static Rep rep(Deq q) {
  if (!q) ERROR("zero pointer");
  return (Rep)q;
}

static void put(Rep r, End e, Data d) {
  Node n = (Node)malloc(sizeof(*n));
  if(!n) ERROR("malloc() failed");
  n->data = d;

  // Determine direction based on which end a node is inserted into
  End outward = e;
  End inward = (e == Head) ? Tail : Head;

  n->np[outward] = 0;
  n->np[inward] = r->ht[e];

  // Link the node into the queue
  if (!r->ht[Head]){
    // Empty queue
    r->ht[Head] = n;
    r->ht[Tail] = n;
  } else{
    //Queue with items
    r->ht[e]->np[outward] = n;
    r->ht[e] = n;
  }
  
  r->len++;
}

static Data ith(Rep r, End e, int i)  {
  //Check if index is valid
  if(i < 0 || i >= r->len) return 0;

  //Determine direction of travel
  End step = (e == Head) ? Tail : Head;

  Node current = r->ht[e];
  for(int count = 0; count < i; count++){
    current = current->np[step];
  }
  
  return current->data;

}

static Data get(Rep r, End e)         {
  //Check for empty queue
  if(!r->ht[e]) return 0;

  Node n = r->ht[e];
  Data d = n->data;

  //Determine direction
  End inward = (e == Head) ? Tail : Head;

  r->ht[e] = n->np[inward];

  if(r->ht[e]){
    //Remove link to the removed node
    r->ht[e]->np[e]=0;
  } else {
    //For emptied queue
    r->ht[inward] = 0;
  }

  free(n);
  r->len--;
  return d;
}

static Data rem(Rep r, End e, Data d) {
  //Step through the queue from the specified end
  End step = (e == Head) ? Tail : Head;
  Node current = r->ht[e];

  while(current){
    if(current->data == d){
      //Node found
      Data found_data = current->data;
      
      //Update links
      //prev link
      if(current->np[Head]){
        current->np[Head]->np[Tail] = current->np[Tail];
      } else{
        r->ht[Head] = current->np[Tail];
      }
      //next link
      if(current->np[Tail]){
        current->np[Tail]->np[Head] = current->np[Head];
      } else{
        r->ht[Tail] = current->np[Head];
      }

      free(current);
      r->len--;
      return found_data;
    }
    current = current->np[step];
  }
  return 0; // data not found
}

extern Deq deq_new() {
  Rep r=(Rep)malloc(sizeof(*r));
  if (!r) ERROR("malloc() failed");
  r->ht[Head]=0;
  r->ht[Tail]=0;
  r->len=0;
  return r;
}

extern int deq_len(Deq q) { return rep(q)->len; }

extern void deq_head_put(Deq q, Data d) {        put(rep(q),Head,d); }
extern Data deq_head_get(Deq q)         { return get(rep(q),Head);   }
extern Data deq_head_ith(Deq q, int i)  { return ith(rep(q),Head,i); }
extern Data deq_head_rem(Deq q, Data d) { return rem(rep(q),Head,d); }

extern void deq_tail_put(Deq q, Data d) {        put(rep(q),Tail,d); }
extern Data deq_tail_get(Deq q)         { return get(rep(q),Tail);   }
extern Data deq_tail_ith(Deq q, int i)  { return ith(rep(q),Tail,i); }
extern Data deq_tail_rem(Deq q, Data d) { return rem(rep(q),Tail,d); }

extern void deq_map(Deq q, DeqMapF f) {
  for (Node n=rep(q)->ht[Head]; n; n=n->np[Tail])
    f(n->data);
}

extern void deq_del(Deq q, DeqMapF f) {
  if (f) deq_map(q,f);
  Node curr=rep(q)->ht[Head];
  while (curr) {
    Node next=curr->np[Tail];
    free(curr);
    curr=next;
  }
  free(q);
}

extern Str deq_str(Deq q, DeqStrF f) {
  char *s=strdup("");
  for (Node n=rep(q)->ht[Head]; n; n=n->np[Tail]) {
    char *d=f ? f(n->data) : n->data;
    char *t; asprintf(&t,"%s%s%s",s,(*s ? " " : ""),d);
    free(s); s=t;
    if (f) free(d);
  }
  return s;
}
