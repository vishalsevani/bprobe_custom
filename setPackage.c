/*

   setPackage - routines to deal with sets of ranges.

   $Header: /home/roycroft/carter/Ping+/rpr/src/Release/RCS/setPackage.c,v 1.4 1996/07/09 15:53:32 carter Exp $


*/

#include<stdio.h>
#include "setPackage.h"

#define MIN(x, y)   (((x) < (y)) ? (x) : (y))
#define MAX(x, y)   (((x) > (y)) ? (x) : (y))

static char rcsid[] = "$Id: setPackage.c,v 1.4 1996/07/09 15:53:32 carter Exp $";

void coalesceSet(Set *set);
void restoreElementCount(Element *elt);
void saveElementCount(Element *elt);

#ifdef TESTPKG
main () {
  testSetPackage();
}
#endif

void testSetPackage() {
  SetPtr setA, setB, setC, setD, setE, peakSet;
  float a[5] = {1.0,2.0,3.0,4.0,5.0};
  float b[4] = {0.5,2.5,3.0,4.1};
  float c[5] = {1.0,1.3,2.34,6.7,9.0};
  int peak, numSets;

  setA = createSet(a, 5, 0.1);
  setB = createSet(b, 4, 0.5);
  setC = createSet(c, 5, 0.1);

  printf("setA has %d element.\n", 
	 setSize(setA));

  dumpSet(setA,0);

  coalesceSet(setA);
  coalesceSet(setB);
  coalesceSet(setC);

  setD = unionSets(setA, setB);
  setE = unionSets(setC, setD);

  findPeakInSet(setE, &peakSet, &peak, &numSets);
  printf("tester: peak is %d found in %d sets\n", 
	 peak, numSets);
  dumpSet(peakSet,0);

  

  setA = destroySet(setA);
  setB = destroySet(setB);
  setC = destroySet(setC);
  
}

void clearFuzz(Set *set) {
  ElementPtr element;
  printf("clearFuzz\n");
  for (element=set->first; element; element=element->next) {
    element->itemLow = element->item;
    element->itemHigh = element->item;
  }
}
  
void adjustFuzz(Set *set, float fuzzFactor) {
  ElementPtr element;
  float increment;

#ifdef SET_DEBUG
  dumpSet(set,1);
  printf("adjustFuzz +/- %10.4f fuzzFactor\n", fuzzFactor);
#endif
  for (element=set->first; element; element=element->next) {
    increment = element->item * fuzzFactor;
    element->itemLow -= increment;
    element->itemHigh += increment;
  }
  coalesceSet(set);
}

Set *createSet(float elements[], int count, float fuzzFactor) {
/* 
   takes a SORTED list of floats and builds a linked list
   where each item is elements[i] +/- fuzzFactor
*/ 

  int i;
  SetPtr new;
  float last = elements[0];
  ElementPtr head = NULL, prev = NULL, current = NULL;
#if SET_DEBUG
  printf("creatSet called.\n");
  for(i=0;i<count;i++) {
    printf("%10.3f ", elements[i]);
  }
  printf("\n");
#endif
  new = makeSet();
  new->count = count;
  for(i=0;i<count;i++) {
    /* do a safety check since we depend on lists being sorted */
    if (elements[i] < last) {
      printf("SORTED LIST ASSUMPTION VIOLATION");
      printf("%f is less that %f\n", elements[i], last);
      exit(0);
    }
    last = elements[i];

    /* malloc, initialize and link in new list element */
    current = makeElement();
    current->item = elements[i];
    current->itemLow = elements[i] - fuzzFactor * elements[i];
    current->itemHigh = elements[i] + fuzzFactor * elements[i];
    current->elementCount = 1;  /* only one reading per bucket at first */
    current->setCount = 1;      /* this element is only made up from one set */
    current->setList->setID.setID = (unsigned int)new;  /* use the sets pointer as a UID */
    current->setList->setID.class = BASE;
    current->setID.setID = (unsigned int)new;  /* use the sets pointer as a UID */
    current->setID.class = BASE;
    current->next = NULL;
    if (!head) head = current;      /* update head the first time */
    if (prev) prev->next = current; /* previous element points at current */
    prev = current;  /* update prev for next addition */
  }
  new->first = head;

#if SET_DEBUG
  printf("\n");
  dumpSet(new,1);
#endif

  return(new);
} /* end createSet */

Set *destroySet(Set *set) {
  ElementPtr elt;

  destroyList(set->first);
  free(set);
  set = NULL;
  return(NULL);
}

int setSize(Set *set) {
  return(set->count);
}

void unionElements(Element *eltA, Element *eltB, Element *eltC) {
  eltC->itemLow = MIN(eltA->itemLow, eltB->itemLow);
  eltC->itemHigh = MAX(eltA->itemHigh, eltB->itemHigh);
  /* in the newly merged element, add the counts for the subordinate 
     elements */
  eltC->elementCount = eltA->elementCount + eltB->elementCount;
  addSetsToIDList(eltC, eltA);
  addSetsToIDList(eltC, eltB);
}

int overlap(Element *eltA, Element *eltB, Element *eltC) {
  eltC->itemLow = MAX(eltA->itemLow, eltB->itemLow);
  eltC->itemHigh = MIN(eltA->itemHigh, eltB->itemHigh);
  return(eltC->itemLow <= eltC->itemHigh);
}

Set *intersectSets(Set *setA, Set * setB) {
  Set *mergedSet = NULL;
  Element *left, *right;
  ElementPtr mergedElement = NULL;
  ElementPtr mergedElementsHead = NULL, mergedElementsTail = NULL;
  int mergedElementsCount = 0;

#ifdef SET_DEBUG 
  printf("intersectSets...\n");
  dumpSet(setA,0); dumpSet(setB,0);
#endif


  /* get pointers to the two sets of elements */
  left = setA->first;
  right = setB->first;

  /* as long as we have elements from both sets */
  while((left != NULL )&& (right != NULL)) {
    mergedElement = makeElement();
    if (overlap(left, right, mergedElement)) {
#ifdef SET_DEBUG
      printf("merged into -> "); dumpElement(mergedElement,0);
      printf("\n");
#endif
      /* add to the merged set list */
      if (!mergedElementsHead) {
	mergedElementsHead = mergedElement;
      }
      if (!mergedElementsTail) {
	mergedElementsTail = mergedElement;
      }
      else { /* add to tail */
	mergedElementsTail->next = mergedElement;
	mergedElementsTail = mergedElement;
      }
      /* Count this member of the set */
      mergedElementsCount++;
      /* one of A_h or B_h will be equal to C_h,
	 drop that piece.
	 could bt both, I guess */
      if (left->itemHigh == mergedElement->itemHigh) {
	left = left->next;
      }
      if (right->itemHigh == mergedElement->itemHigh) {
	right = right->next;
      }
    }
    else { /* no overlap, drop (smaller) piece (on the left) */
      /* throw away the botched merger */
      mergedElement = destroyElement(mergedElement);
      if (left->itemHigh < right->itemHigh) {
#ifdef SET_DEBUG
	printf("throwaway "); dumpElement(left,0);
#endif
	left = left->next;
      }
      else {
#ifdef SET_DEBUG
	printf("throwaway "); dumpElement(right,0);
#endif
	right = right->next;
      }
#ifdef SET_DEBUG
      printf("\n");
#endif
    }
  } /* end loop over both lists */
  /* once one of the list runs out, were done */

  if (mergedElementsHead) {
    mergedSet = makeSet();
    mergedSet->first = mergedElementsHead;
    mergedSet->count = mergedElementsCount;
  }

  if (mergedSet) {
#ifdef SET_DEBUG
    printf("intersetSets: coalescing set\n");
#endif
    coalesceSet(mergedSet);
#ifdef SET_DEBUG
    printf("intersectSets: merged set results ->");
    dumpSet(mergedSet,0);
#endif
  }
  else {
#ifdef SET_DEBUG
    printf("EMPTY INTERESECTION!\n");
#endif
  }
  return(mergedSet);
} /* end intersectSets */

Set *unionSets(Set *setA, Set * setB) {
  Set *mergedSet = NULL;
  Element *left, *right;
  ElementPtr mergedElement = NULL;
  ElementPtr mergedElementsHead = NULL, mergedElementsTail = NULL;
  int mergedElementsCount = 0;

#ifdef CDEBUG 
  printf("unionSets...\n");
  dumpSet(setA,1); dumpSet(setB,1);
#endif


  /* get pointers to the two sets of elements */
  left = setA->first;
  right = setB->first;

  /* as long as we have elements from both sets */
  while(left && right) {
    mergedElement = makeElement();
    if (overlap(left, right, mergedElement)) {
      int dropRight = 0, dropLeft = 0;
      unionElements(left, right, mergedElement);
#ifdef CDEBUG
      dumpElement(left,0); dumpElement(right,0);
      printf("merged into -> "); dumpElement(mergedElement,0);
      printf("\n");
#endif
      /* want to drop the piece to the left,
	 or both if the highs are equal */
      
      if ( /* (left->itemHigh == mergedElement->itemHigh) || */
	  (left->itemHigh <= right->itemHigh)) {
	dropLeft = 1;
      }
      if (/* (right->itemHigh == mergedElement->itemHigh) || */
	  (right->itemHigh <= left->itemHigh)) {
	dropRight = 1;
      }
      /* drop one or both.
	 if not dropped then the number of points in that
	 element has already been counted once. So,
	 clear the elementCount field and the next merged
	 piece (if any) will only count the elements from
	 the other piece */
      /* Sheesh, what a hack ! */
      if (dropLeft) {
	if (0 == left->elementCount) {
	  restoreElementCount(left);
	}
	left = left->next;
      }
      else {
	saveElementCount(left);
      }
      if (dropRight) {
	if (0 == right->elementCount) {
	  restoreElementCount(right);
	}
	right = right->next;    
      }
      else {
	saveElementCount(right);
      }
    }
      
    else { /* no overlap, drop (smaller) piece (on the left) */
      /* throw away the botched merger */
      mergedElement = destroyElement(mergedElement);
      if (left->itemHigh < right->itemLow) {
	mergedElement = copyElement(left);

#ifdef CDEBUG2
	printf("merge IN "); dumpElement(mergedElement,0);
#endif
	if (0 == left->elementCount) {
	  restoreElementCount(left);
	}
	left = left->next;
      }
      else { /* right->itemHigh < left->itemLow */
	mergedElement = copyElement(right);

#ifdef CDEBUG2
	printf("merge IN "); dumpElement(mergedElement,0);
#endif

	if (0 == right->elementCount) {
	  restoreElementCount(right);
	}
	right = right->next;
      }

#ifdef CDEBUG
      printf("\n");
#endif

    }
    /* add to the merged set list */
    if (!mergedElementsHead) {
      mergedElementsHead = mergedElement;
    }
    if (!mergedElementsTail) {
      mergedElementsTail = mergedElement;
    }
    else { /* add to tail */
      mergedElementsTail->next = mergedElement;
      mergedElementsTail = mergedElement;
    }
    /* count this member of the set */
    mergedElementsCount++;
  } /* end loop over both lists */

  while (left) {
    mergedElement = copyElement(left);
    mergedElementsTail->next = mergedElement;
    mergedElementsTail = mergedElement;
    mergedElementsCount++;
#ifdef CDEBUG2
    printf("carry along left"); dumpElement(mergedElement,0);
#endif
    if (0 == left->elementCount) {
      restoreElementCount(left);
    }
    left = left->next;
  }

  while (right) {
    mergedElement = copyElement(right);
    mergedElementsTail->next = mergedElement;
    mergedElementsTail = mergedElement;
    mergedElementsCount++;
#ifdef CDEBUG2
    printf("carry along right"); dumpElement(mergedElement,0);
#endif
    if (0 == right->elementCount) {
      restoreElementCount(right);
    }
    right = right->next;
  }

  if (mergedElementsHead) {
    mergedSet = makeSet();
    mergedSet->first = mergedElementsHead;
    mergedSet->count = mergedElementsCount;
  }

  if (mergedSet) {

#ifdef CDEBUG
    printf("unionSets: coalescing set\n");
    dumpSet(mergedSet,0);
#endif

    coalesceSet(mergedSet);

#ifdef CDEBUG
    printf("unionSets: merged set results ->");
    dumpSet(mergedSet,1);
#endif
  }
  else {

#ifdef CDEBUG
    printf("EMPTY UNION!\n");
#endif
  }
  return(mergedSet);
} /* end unionSets */

void dumpElement(Element *element, int checkZero) {
  printf("(%10.4g,%10.4g [%d <%d>]) ", 
	 element->itemLow, element->itemHigh,
	 element->elementCount,
	 element->savedElementCount);
  printf("setID: %u ", element->setID);
  printf(" %d sets :", element->setCount);
  dumpSetIDList(element);
  if (checkZero && element->elementCount < 1) exit(0);
  if (checkZero && element->elementCount < countSets(element->setList,BASE)) exit(0);
}

void dumpList(Element *head, int checkZero) {
  Element *element;

  for(element=head      ; element ; element = element->next) {
    dumpElement(element, checkZero);
  }
  printf("\n");
}

void dumpSet(Set *set, int checkZero) {
  int i, size = setSize(set);
  Element *element;
  

  printf("set [%u] has %d elements\n", (unsigned int) set, size);
  printf("logme\t");
  dumpList(set->first, checkZero);

} /* end dumpSet */

void findPeakInSet(Set *inSet, Set **outSet,
		   int *peakSize, int *numSets) {
  int i, size = setSize(inSet);
  int peak = 0, numberOfPeaks = 0;
  Element *element;
  Element *peakHead, *peakTail;
  Set *returnSet;

  peakTail = peakHead = makeElement(); /* a dummy header */

#ifdef CDEBUG
  printf("findPeakInSet: set has %d elements\n", size);
  dumpSet(inSet,1);
  printf("\n");
#endif
  
  for(element=inSet->first; element ; element = element->next) {
    if (element->elementCount  == peak) {
      peakTail->next = copyElement(element);
      peakTail = peakTail->next;
#ifdef CDEBUG
      printf("got another of height %d\n", peak);
      dumpList(peakHead->next,1);
#endif
      numberOfPeaks++;
    }
    if (element->elementCount > peak) {
      destroyList(peakHead->next);
      peak = element->elementCount;
      numberOfPeaks = 1;
#ifdef CDEBUG
      printf("peak of %d -->>",peak);
      dumpElement(element,0);
#endif
      peakTail = peakHead->next = copyElement(element);
    }
  }
#ifdef CDEBUG
  printf("\n");
#endif

  returnSet = makeSet();

  returnSet->count = numberOfPeaks;
  returnSet->first = peakHead->next;
  peakHead = destroyElement(peakHead);

  *outSet = returnSet;
  *peakSize = peak;
  *numSets = countSets(returnSet->first->setList, BASE);

} /* end findPeakInSet */

int countSets(SetIDList *list, setClass class) {
  if (list) {
    return((list->setID.class == class) +
	   countSets(list->next, class));
  }
  else {
    return(0);
  }
}


void coalesceSet(Set *set) { 
/*
   run through set elements an collapse any that overlap?

   waitaminnit, how am i doing this fuzzy stuff

   For now, I am unioning all overlapping elements within a set
   Maybe I should be interescting?
*/

  ElementPtr current = NULL, next = NULL;
#ifdef SET_DEBUG
  printf("coalesce: start\n");
  dumpSet(set,0);
#endif

  for (current = set->first; current != NULL; current = current->next) {
    /* go through ordered list, check for overlap between adjacent
       elements */
    next = current->next;
    while (next && (current->itemHigh >= next->itemLow)) {
      /* overlap exists, so take union of the overlapping neighbors*/
      current->itemHigh = next->itemHigh;
      /* link to the right neighbor's right neighbor*/
      current->next = next->next;
      /* incrment the count for the element */
      current->elementCount += next->elementCount;
      /* keep a list of set Ids represented by this merged element */
      addSetsToIDList(current, next);
      /* release the right neighbor */
      next = destroyElement(next);
      /* decrement the element count of the containing set*/
      set->count--;
      /* point at right neighbor for next loop iteration */
      next = current->next;
    }
  }
  for (current = set->first; current != NULL; current = current->next) {
    current->setID.setID = (unsigned int)set;
    current->setID.class = DERIVED;
  }

#ifdef SET_DEBUG
  printf("coalesce: done\n");
  dumpSet(set,1);
#endif

} /* end coalesceSet */

ElementPtr destroyList(ElementPtr elt) {
  
 if (elt) {
   elt->next = destroyList(elt->next);
   elt = destroyElement(elt);
   return(elt);
 }
 return(NULL);
}

SetIDListPtr destroySetIDList(SetIDListPtr old) {
  if (old) {
    old->next = destroySetIDList(old->next);
    free(old);
    old = NULL;
    return(NULL);
  }
  return(NULL);
}

Element *destroyElement(Element *element) {
  element->setList = destroySetIDList(element->setList);
  free(element);
  element = NULL;
  return(NULL);
}

Element *makeElement() {
     Element *ptr;
     ptr = (Element *) malloc (sizeof(Element));
     if (!ptr) { printf("malloc outta track\n"); exit(0);}
     ptr->item = 0.0;
     ptr->itemLow = 0.0;
     ptr->itemHigh = 0.0;
     ptr->elementCount = 0;
     ptr->savedElementCount = 0;
     ptr->setCount = 0;
     ptr->setList = (SetIDListPtr)malloc(sizeof(SetIDList));
     if (!ptr->setList) { printf("malloc outta track\n"); exit(0);}
     ptr->setList->setID.setID = 0;
     ptr->setList->setID.class = UNKNOWN;
     ptr->setList->next = NULL;
     ptr->setID.setID = 0;
     ptr->setID.class = UNKNOWN;
     ptr->next = NULL;
     return(ptr);
}

Element *copyElement(Element *orig) {
  Element *clone;
  clone = makeElement();

  clone->item = orig->item;
  clone->itemLow = orig->itemLow;
  clone->itemHigh = orig->itemHigh;
  clone->elementCount = orig->elementCount;
  clone->savedElementCount = orig->elementCount;
  clone->setCount = orig->setCount;
  /* throw away the (fake) list before copying the true one */
  free(clone->setList); 
  clone->setList = NULL;
  clone->setList = copyIDList(orig->setList);
  clone->setID = orig->setID;
  clone->next = NULL;
  return(clone);
} 


Set *makeSet() {
  Set *ptr;
  ptr = (SetPtr)malloc(sizeof(Set));
  if (!ptr) { printf("malloc outta track\n"); exit(0);}
  ptr->first = NULL;
  ptr->count = 0;
  return(ptr);
}

void addSetsToIDList(Element *destElement, Element *sourceElement) {
  SetIDListPtr curr = NULL;

  for (curr = sourceElement->setList; curr; curr = curr->next) {
    addSetIDToList(destElement, curr->setID);
  }
  
}
 
void addSetIDToList(Element *element, SetID setID) {
  SetIDListPtr curr = NULL, prev = NULL, newID = NULL;

  for (curr = element->setList; curr; curr = curr->next) {
    if (setID.setID == curr->setID.setID) {
      return; /* already in list */
    }
    if (setID.setID > curr->setID.setID) {
      newID = (SetIDListPtr)malloc(sizeof(SetIDList));
  if (!newID) { printf("malloc outta track\n"); exit(0);}
      newID->setID.setID = setID.setID;
      newID->setID.class = setID.class;
      newID->next = curr;
      if (prev) prev->next = newID;
      else element->setList = newID;
      element->setCount++;
      return;
    }
    prev = curr;
  }
  /* add at end of list */
  printf("this should never happen: prev %u curr %u\n",
	 (unsigned int)prev, (unsigned int) curr);
  newID = (SetIDListPtr)malloc(sizeof(SetIDList));
  if (!newID) { printf("malloc outta track\n"); exit(0);}
  newID->setID = setID;
  newID->next = NULL;
  if (prev) prev->next = newID;
  else element->setList = newID;
  element->setCount++;
  return;
}

void dumpSetIDList(Element *element) {
  SetIDListPtr curr;
  int BaseCount = 0, DerivedCount = 0;

  printf("SetIDList: ");
  for (curr = element->setList; curr; curr = curr->next) {
    printf("[{%u}%u : ", (unsigned int) curr, curr->setID.setID);
    switch (curr->setID.class) {
    case BASE: printf("BASE"); BaseCount++; break;
    case DERIVED: printf("DERIVED"); DerivedCount++; break;
    case UNKNOWN: printf("UNK"); break;
    default: printf("ERROR");
    }
    printf ("] ");
  }
  printf(" %d base, %d derived\n", BaseCount, DerivedCount);
}


SetIDListPtr copyIDList(SetIDListPtr oldList) {
  SetIDListPtr newList, Ncurr, newItem;

  if (oldList) {
    newItem = (SetIDListPtr) malloc(sizeof(SetIDList));
    if (newItem) {
      newItem->setID = oldList->setID;
      newItem->next = copyIDList(oldList->next);
      return(newItem);
    }
    else {
      printf("malloc done run outta track!\n");
      exit(0);
    }
  }
  else {
    return(NULL);
  }
  return(NULL); /* to avoid brain-damaged compiler warning */
}

void saveElementCount(Element *elt) {
  /* but this may execute many times and we can only save the first time */
#if DEEP_DEBUG
    printf("saveElementCount: saving %d?\n",elt->elementCount );
	dumpElement(elt,0);
#endif

  if (elt->elementCount > 0) {
    elt->savedElementCount = elt->elementCount;
    elt->elementCount = 0;
  }
  else {
#if DEEP_DEBUG
    printf("saveElementCount: saving 0?!! must already be saved\n");
    dumpElement(elt,0);
#endif
  }
#if DEEP_DEBUG
	printf("saveElementCount: saved %d!\n",elt->savedElementCount );
	dumpElement(elt,0);
	printf("\n");
#endif
}

void restoreElementCount(Element *elt) {
#if DEEP_DEBUG
	  printf("restore elementCount");
	  printf("restoring %d\n", elt->savedElementCount);
	  dumpElement(elt,0);
#endif
  if (elt->savedElementCount > 0) {
    elt->elementCount = elt->savedElementCount;
  }
  else {
    printf("saveElementCount: restoring 0?\n");
    dumpElement(elt,1);
  }
#if DEEP_DEBUG
	  printf("restore elementCount restored ");
	  printf("restoring %d\n", elt->elementCount);
	  dumpElement(elt,0);
	  printf("\n");

#endif
}

